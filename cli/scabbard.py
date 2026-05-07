#!@Python_EXECUTABLE@

"""*
 * @file scabbard.py
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief Command line interface for scabbard util 
 *        derived from https://github.com/LLNL/SCABBARD/blob/e6c9ece1356c93418a04452f98f0f55497f4bf4d/interception/scabbard.py
 *        used to implement the LD_PRELOAD trick for getting the scabbard instrumenter working.
 * @date 2024-05-23
 * 
 *"""

import os
import pathlib
import subprocess
import sys

from argconfig import parseScabbardArgs, printScabbardHelp
from colors import *

DEBUG: bool = False

SCABBARD_PATH:str = os.environ['SCABBARD_PATH'] if 'SCABBARD_PATH' in os.environ else os.path.dirname(os.path.abspath(__file__))
# sys.path.append(SCABBARD_PATH)
# from libs.scabbard import scabbardlib as scabbard

INSTR_LIB:str = SCABBARD_PATH+"/libinstr.so"
INTERCEPT_LIB:str = SCABBARD_PATH+"/intercept.so"

ADDED_FLAGS: list = [
        f'-fpass-plugin={INSTR_LIB}',                           # load the pass plugin (during late opt) (requires adjustments to pass manager setup)
        # f'-Wl,--load-pass-plugin={INSTR_LIB}',                  # load pass plugin for cpu module (during LTO)
        '-Xoffload-linker', f'--load-pass-plugin={INSTR_LIB}',  # load pass plugin for gpu module (during LTO)
        f'-L{SCABBARD_PATH}',                                   # point the linker to where scabbard's trace libraries are
        '-ltrace',                                              # the tracing code for the gpu that the instrumentation will call on
        '-ltrace.device',                                       # the tracing code for the gpu that the instrumentation will call on
        '-lpthread',                                            # required by scabbard's libtrace (link unix pthread library)
        '@SCABBARD_ZLIB_LIBRARIES@',                            # required by scabbard's libtrace (link zlib compression library)
        '@SCABBARD_LINK_ZLIB@',                                 # required by scabbard's libtrace (link zlib compression library)
        '-fgpu-rdc',                                            # if we compile in single TU mode the LTO pass won't run on the device modules
        #'-flto',                                                # ensure that LTO is run (will conflict if -fthin-lto or -fno-lto is used)
        '-foffload-lto=full',                                   # enable LTO for the device
        '-g'                                                    # required to get the location metadata
    ]

def executeCommandWithFlags(argv: list, env: dict) -> None:
    if "SCABBARD_PATH" not in env:
        env.update({"SCABBARD_PATH": SCABBARD_PATH})
    
    new_argv = list(argv)
    new_argv[1:1] = ADDED_FLAGS
    new_cmd = ' '.join(new_argv)
    
    if DEBUG:
        prCyan(f"[scabbard.py:DBG] instrumented cmd: {new_cmd}")
    
    try:
        prGreen('*** SCABBARD ***')
        prGreen('Running Instrumented command\n')
        cmdOutput = subprocess.run(new_cmd, shell=True, check=True, env=env) #, text=True,
                                    # stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # print(cmdOutput.stdout)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise cpe
    except Exception as e:
        prRed(e)
        raise Exception(new_cmd) from e





def instr_mode(scabbard_args, args) -> None:
    env = dict(os.environ)
    executeCommandWithFlags(args, env)
    
    
def build_mode(scabbard_args, args) -> None:
    env = dict(os.environ)
    
    if len(args) < 1:
        prRed("[scabbard.build:ERR] No build command received")
        exit(-1)
    new_cmd = ' '.join(args)
    
    try:
        cmdOutput = subprocess.run(new_cmd, shell=True, check=True, env=env)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise RuntimeError('Error when running scabbard on a build command') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError('Unexpected Error when running scabbard on a build command') from e
    

def run_instrumented_exe_mode(scabbard_args, args) -> None:
    env = dict(os.environ)
    if ('device_buf_size' in scabbard_args or "device-buf-size" in scabbard_args or "--device-buff-size" in scabbard_args) \
            and scabbard_args.device_buf_size is not None and len(scabbard_args.device_buf_size) > 0:
        env.update({'SCABBARD_DEVICE_BUFFER_SIZE':scabbard_args.device_buf_size[0]})
    if len(args) < 1:
        prRed("[scabbard.run:ERR] provide a command to run a trace on (must eventually run an executable instrumented by scabbard)")
        exit(-1)
    if len(args) == 1:
        args.append('"dummy-arg"')
    if 'SCABBARD_INSTRUMENTED_EXE_NAME' not in env:
        env.update({'SCABBARD_INSTRUMENTED_EXE_NAME':os.path.abspath(args[0])})
    new_cmd = ' '.join(args)
    try:
        cmdOutput = subprocess.run(new_cmd, shell=True, check=True, env=env)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise RuntimeError('Error when running a scabbard instrumented executable') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError('Error when running scabbard on a trace command') from e
    prGreen(f"[scabbard.run:INFO] Trace Finished!\n[scabbard.trace:INFO] Trace-file generated: `{env['SCABBARD_TRACE_FILE']}`\n")



def main(argv:list) -> None:
    try:
        scabbard_args, args = parseScabbardArgs(argv[1:])
        if 'mode' not in scabbard_args or scabbard_args.mode is None or not len(scabbard_args.mode)>0:
            prRed("please select a mode (instr|trace|verif) when using the scabbard interface!")
            print(argv) #DEBUG
            printScabbardHelp()
            exit(1)
        match scabbard_args.mode:
            case 'instr':
                instr_mode(scabbard_args, args)
            case 'build':
                build_mode(scabbard_args, args)
            case 'run':
                run_instrumented_exe_mode(scabbard_args, args)
            case 'verif':
                raise Exception("`verif` is no longer a valid mode for Scabbard, "
                                "as Scabbard is now an Online-Race-Checker "
                                "(i.e. Verification occurs along side your application "
                                "during the run step).")
            case other:
                prRed(f"`{other!s}` is not a recognized mode for scabbard")
                printScabbardHelp()
    except Exception as e:
        prRed(e)
        exit(-1)



if __name__ == '__main__':
    main(sys.argv)
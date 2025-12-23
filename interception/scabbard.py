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

def runCommandWithFlags(argv: list, env: dict) -> None:
    if "SCABBARD_PATH" not in env:
        env.update({"SCABBARD_PATH": SCABBARD_PATH})
        
    if 'SCABBARD_METADATA_FILE' not in env:
        raise Exception("[scabbard.intercept.driver:ERR] `SCABBARD_METADATA_FILE` was not defined! [dev-error]")
    
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



# def executeOriginalCommand(argv: list) -> None:
#     if DEBUG:
#         print(f"[scabbard.py:DBG] running original cmd: {' '.join(argv)}")
#     try:
#         cmd: str = ' '.join(argv)
#         if DEBUG:
#             prCyan(f"[scabbard.py:DBG] executing unaltered cmd: {cmd}")
#         subprocess.run(f"LD_PRELOAD={INTERCEPT_LIB} {cmd}", shell=True, check=True)
#     except subprocess.CalledProcessError as e:
#         prRed(e)
        

# def runBuildCommand(params: list, isRoot:bool=False) -> None:
#     prGreen('*** SCABBARD ***')
#     prGreen('Intercepting commands in: ' + ' '.join(params))
#     params.insert(0, 'LD_PRELOAD='+INTERCEPT_LIB)
    
#     env: dict = dict(os.environ)
    
#     if 'SCABBARD_PATH' not in env:
#         env.update({'SCABBARD_PATH':os.path.dirname(os.path.abspath(__file__))})
#     if 'SCABBARD_METADATA_FILE' not in env:
#         env.update({'SCABBARD_METADATA_FILE':f"{os.path.abspath(os.getcwd())}/anon.scabbard.metadata"})

#     if any([x in os.path.basename(params[1]) for x in {"clang","hipcc"}]) \
#         or any([x in params[1] for x in {"clang","hipcc"}]):
#         runCommandWithFlags(params[1:], env)
#     else:
#         if DEBUG:
#             prCyan(f"[scabbard.py:DBG] instrumented cmd: {' '.join(params)}")
#         try:
#                 cmdOutput = subprocess.run(' '.join(params), shell=True, check=True, env=env) #,  text=True,
#                                             # stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#                 # print(cmdOutput.stdout)
#         except subprocess.CalledProcessError as cpe:
#             prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
#             raise RuntimeError('Error when running scabbard.intercept on a build command [scabbard.py]') from cpe
#         except Exception as e:
#             os.remove(env['SCABBARD_METADATA_FILE']+".lock")
#             prRed(e)
#             raise RuntimeError('Error when running scabbard.intercept on a build command [scabbard.py]') from e
    
#     os.remove(env['SCABBARD_METADATA_FILE']+".lock")
#     if isRoot:
#         prGreen(f"\n[scabbard.instr:INFO] Build Finished!\n[scabbard.instr:INFO] Meta-file generated: {os.environ['SCABBARD_METADATA_FILE']}\n")

#     scabbard.merge_stats_reports('./report/', './', 'output')
#     scabbard.generate_remark_reports('./report/', './', ['output'])


def instr(scabbard_args, args) -> None:
    env = dict(os.environ)
    if ('meta_file' in scabbard_args or "meta-file" in scabbard_args or "--meta-file" in scabbard_args) \
            and scabbard_args.meta_file is not None and len(scabbard_args.meta_file) > 0:
        env.update({'SCABBARD_METADATA_FILE':scabbard_args.meta_file[0]})
    elif 'SCABBARD_METADATA_FILE' not in os.environ:
        env.update({'SCABBARD_METADATA_FILE':f"{os.path.abspath(os.getcwd())}/anon.scabbard.meta"})
    lock_file = pathlib.Path(env['SCABBARD_METADATA_FILE']+".lock")
    lock_file.touch() # create the metadata file beforehand to alleviate some inter os issues
    runCommandWithFlags(args, env)
    lock_file.unlink()
    
    
def build(scabbard_args, args) -> None:
    env = dict(os.environ)
    if ('meta_file' in scabbard_args or "meta-file" in scabbard_args or "--meta-file" in scabbard_args) \
            and scabbard_args.meta_file is not None and len(scabbard_args.meta_file) > 0:
        os.environ.update({'SCABBARD_METADATA_FILE':scabbard_args.meta_file[0]})
    elif 'SCABBARD_METADATA_FILE' not in env:
        env.update({'SCABBARD_METADATA_FILE':f"{os.path.abspath(os.getcwd())}/anon.scabbard.meta"})
    lock_file = pathlib.Path(env['SCABBARD_METADATA_FILE']+".lock") 
    
    if len(args) < 1:
        prRed("[scabbard.build:ERR] provide a command to run (no command received)")
        exit(-1)
    new_cmd = ' '.join(args)
    
    lock_file.touch() # create the metadata file beforehand to alleviate some inter os issues
    try:
        cmdOutput = subprocess.run(new_cmd, shell=True, check=True, env=env)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise RuntimeError('Error when running scabbard.build on a build command') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError('Error when running scabbard on a trace command') from e
    prGreen(f"[scabbard.build:INFO] Build Finished!\n[scabbard.build:INFO] meta-file generated: `{env['SCABBARD_METADATA_FILE']}\n")
    lock_file.unlink()
    

def trace(scabbard_args, args) -> None:
    env = dict(os.environ)
    if ('trace_file' in scabbard_args or "trace-file" in scabbard_args or "--trace-file" in scabbard_args) \
            and scabbard_args.trace_file is not None and len(scabbard_args.trace_file) > 0:
        env.update({'SCABBARD_TRACE_FILE':scabbard_args.trace_file[0]})
    elif 'SCABBARD_TRACE_FILE' not in env:
        env.update({'SCABBARD_TRACE_FILE':f"{os.path.abspath(os.getcwd())}/{os.path.basename(os.path.abspath(args[0]))}.scabbard.trace"})
    if len(args) < 1:
        prRed("[scabbard.trace:ERR] provide a command to run a trace on (must eventually run an executable instrumented by scabbard)")
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
        raise RuntimeError('Error when running scabbard.trace on a trace command') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError('Error when running scabbard on a trace command') from e
    prGreen(f"[scabbard.trace:INFO] Trace Finished!\n[scabbard.trace:INFO] Trace-file generated: `{env['SCABBARD_TRACE_FILE']}`\n")



def verif(scabbard_args, args) -> None:
    if 'trace_file' not in scabbard_args or scabbard_args.trace_file is None or not len(scabbard_args.trace_file) > 0:
        raise RuntimeError("scabbard trace expected a trace file as an argument but found none!")
    if 'meta_file' not in scabbard_args or scabbard_args.meta_file is None or not len(scabbard_args.meta_file) > 0:
        raise RuntimeError("scabbard trace expected a trace file as an argument but found none!")
    try:
        cmdOutput = subprocess.run(f"{SCABBARD_PATH}/verif {scabbard_args.meta_file[0]} {scabbard_args.trace_file[0]}", 
                                    shell=True)
                                    # shell=True, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # print(cmdOutput.stdout)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise RuntimeError('Error when running scabbard.verif on a verify command') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError('Error when running scabbard.verif') from e



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
                instr(scabbard_args, args)
            case 'build':
                build(scabbard_args, args)
            case 'trace':
                trace(scabbard_args, args)
            case 'verif':
                verif(scabbard_args, args)
            case other:
                prRed(f"`{other!s}` is not a recognized mode for scabbard")
                printScabbardHelp()
    except Exception as e:
        prRed(e)
        exit(-1)



if __name__ == '__main__':
    main(sys.argv)
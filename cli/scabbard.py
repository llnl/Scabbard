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
import re

from argconfig import parseScabbardArgs, printScabbardHelp
from colors import *

DEBUG: bool = True

SCABBARD_PATH:str = os.environ['SCABBARD_PATH'] if 'SCABBARD_PATH' in os.environ else os.path.dirname(os.path.abspath(__file__))
# sys.path.append(SCABBARD_PATH)
# from libs.scabbard import scabbardlib as scabbard

INSTR_LIB:str = SCABBARD_PATH+"/libinstr.so"
INTERCEPT_LIB:str = SCABBARD_PATH+"/intercept.so"

ADDED_FLAGS: list = [
        '-Wno-unused-command-line-argument',                    # mute warnings about unnecessary cli flags
        f'-fpass-plugin={INSTR_LIB}',                           # load the pass plugin (during late opt) (requires adjustments to pass manager setup)
        #f'-Wl,--load-pass-plugin={INSTR_LIB}',                  # load pass plugin for cpu module (during LTO)
        '-Xoffload-linker', f'--load-pass-plugin={INSTR_LIB}',  # load pass plugin for gpu module (during LTO)
        f'-L{SCABBARD_PATH}',                                   # point the linker to where scabbard's trace libraries are
        '-lrtl',                                                # the tracing code for the gpu that the instrumentation will call on
        '-lrtl.device',                                         # the tracing code for the gpu that the instrumentation will call on
        '-lpthread',                                            # required by scabbard's libtrace (link unix pthread library)
        '-fgpu-rdc',                                            # if we compile in single TU mode the LTO pass won't run on the device modules
        #'-flto',                                                # ensure that LTO is run (will conflict if -fthin-lto or -fno-lto is used)
        '-foffload-lto=full',                                   # enable LTO for the device
        '-g',                                                   # required to get the location metadata
    ]

def executeCommandWithFlags(argv: list[str], env: dict[str,str]) -> None:
    if "SCABBARD_PATH" not in env:
        env.update({"SCABBARD_PATH": SCABBARD_PATH})
    
    def split_compilation(args: list[str]) -> tuple[list[str],list[str],list[str]]:
        new_args = list()
        file_types: re.Pattern = re.compile(r"^.+\.(?:c{1,2}|cpp|cu|hip)$",flags=re.IGNORECASE)
        out_flag: re.Pattern = re.compile(r"""^--?o(?:utput=?|=)?(["'`][^"'`]+["'`]|\S+)?$""")
        ds_flag: re.Pattern = re.compile(r"^--?fgpu-default-stream(?:\s|=)?(per-thread)?$",flags=re.IGNORECASE)
        lang_flags: re.Pattern = re.compile(r"^--?(?:x|language=?)(\w+)?$")
        lang_std_flags: re.Pattern = re.compile(r"^--?std=?(\S+)?$")
        lang: str = "hip"
        lang_std: str|None = None
        out_file: pathlib.Path|None = None
        src_file: pathlib.Path|None = None
        ds: int = 1
        i = iter(args)
        x: str|None
        while (x := next(i, None)) is not None:
            x = x.strip(' ')
            m: re.Match|None
            if (m := out_flag.match(x)) is not None:
                if m.lastindex is not None and m.lastindex > 0:
                    out_file = pathlib.Path(m[1]).absolute()
                    continue
                if (x := next(i, None)) is not None:
                    out_file = pathlib.Path(x.strip(' ')).absolute()
                else:
                    raise RuntimeError(f'poorly formatted output argument `{m[0]} {x}`')
            elif (m := file_types.match(x)) is not None:
                src_file = pathlib.Path(x).absolute()
            elif (m := ds_flag.match(x)) is not None:
                new_args.append(x)
                if m.lastindex is not None and m.lastindex > 0:
                    ds = 2
                elif (x := next(i, None)) is not None:
                    x = x.strip(' ').lower()
                    ds = 2 if x == 'per-thread' else 1
                    new_args.append(x)
            elif (m := lang_flags.match(x)) is not None:
                if  m.lastindex is not None and m.lastindex > 0:
                    lang = m[1]
                elif (x := next(i, None)) is not None:
                    lang = x.strip(' ')
            elif (m := lang_std_flags.match(x)) is not None:
                if  m.lastindex is not None and m.lastindex > 0:
                    lang_std = m[1]
                elif (x := next(i, None)) is not None:
                    lang_std = x.strip(' ')
            else:
                new_args.append(x)
        # ==
        if src_file is None:
            raise RuntimeError("No sourcefile argument found in build command!")
        if out_file is None:
            out_file = pathlib.Path("./a.out").absolute()
        # ==
        main_o_file: pathlib.Path = out_file.parent / (str(src_file.stem) + '.o')
        dsh_o_file: pathlib.Path = out_file.parent / "scabbard_dsh.o"
        # ==
        main_comp: list[str] = list(new_args)
        dsh_comp: list[str] = list(new_args)
        link_args: list[str] = list(new_args)
        main_comp.extend((f"-x{lang}",f"-std={lang_std}",'-c', f"{src_file}", '-o', f"{main_o_file}"))
        dsh_comp.extend((f"-D__SCABBARD_HIP_DEFAULT_STREAM__={ds}u",f"-xhip",f"-std=c++17",'-c',
                         f"{SCABBARD_PATH}/default_stream_helper.c","-Wno-null-character", 
                         '-o', str(dsh_o_file)))
        link_args.extend((str(dsh_o_file),str(main_o_file),'-o',str(out_file),"-Wno-null-character"))
        # ==
        main_comp[1:1] = ADDED_FLAGS
        link_args[1:1] = ADDED_FLAGS
        # ==
        return (main_comp, dsh_comp, link_args)
    #?END fn split_compilation()

    prGreen('*** SCABBARD ***')
    for args in split_compilation(argv):
        new_cmd = ' '.join(args)
        
        try:
            if DEBUG:
                prCyan(f"[scabbard.py:DBG] instrumented cmd: ```\n[scabbard.py:DBG]    {new_cmd}\n[scabbard.py:DBG]  ```")
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
    if ('rtl_alloc_len' in scabbard_args or "rtl-alloc-len" in scabbard_args or "--rtl-alloc-len" in scabbard_args) \
            and scabbard_args.rtl_alloc_len is not None and len(scabbard_args.rtl_alloc_len) > 0:
        env.update({'SCABBARD_RTL_MEM_CHUNK_LEN':scabbard_args.rtl_alloc_len[0]})
    if ('stdout' in scabbard_args or "stdout" in scabbard_args or "--stdout" in scabbard_args) \
            and scabbard_args.stdout is not None and len(scabbard_args.stdout) > 0:
        env.update({'SCABBARD_RTL_STDOUT':scabbard_args.stdout[0]})
    if ('stderr' in scabbard_args or "stderr" in scabbard_args or "--stderr" in scabbard_args) \
            and scabbard_args.stderr is not None and len(scabbard_args.stderr) > 0:
        env.update({'SCABBARD_RTL_STDERR':scabbard_args.stderr[0]})
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
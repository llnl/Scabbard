#!@Python_EXECUTABLE@

"""*
 * @file driver.py
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief derived from https://github.com/LLNL/FAROS/blob/e6c9ece1356c93418a04452f98f0f55497f4bf4d/interception/clang_driver.py
 *        used to implement the LD_PRELOAD trick for getting the scabbard instrumenter working.
 * @date 2024-05-23
 * 
 *"""

import os
import pathlib
import subprocess
import platform
import sys
from colors import prGreen, prCyan, prRed
# from exceptions import CompileException
from builtins import Exception
from scabbard import executeOriginalCommand, ADDED_FLAGS, runBuildCommand, SCABBARD_PATH


DEBUG: bool = False

# --------------------------------------------------------------------------- #
# --- Installation Paths ---------------------------------------------------- #
# --------------------------------------------------------------------------- #


# --------------------------------------------------------------------------- #
# --- Classes --------------------------------------------------------------- #
# --------------------------------------------------------------------------- #

# class Command:
#   def __init__(self, cmd):
#     self.name = cmd[0]
#     self.parameters = cmd[1:]

#   def executeOriginalCommand(self):
#     try:
#       cmd = [self.name] + self.parameters
#       subprocess.run(' '.join(cmd), shell=True, check=True)
#     except subprocess.CalledProcessError as e:
#       prRed(e)

#   def getOriginalCommand(self):
#     return ' '.join([self.name] + self.parameters[1:])

#   def runCommandWithFlags(self):
#     new_cmd = [self.name] + ADDED_FLAGS.split() + self.parameters
#     new_cmd.insert(0, 'PATH='+os.getenv('FAROS_SAVEDPATH'))
#     try:
#       cmdOutput = subprocess.run(' '.join(new_cmd), shell=True, check=True)
#     except Exception as e:
#       prRed(e)
#       raise CompileException(new_cmd) from e

def runCommandWithFlags(argv: list) -> None:
    env: dict = dict(os.environ)
    if "SCABBARD_PATH" not in env:
        env.update({"SCABBARD_PATH": SCABBARD_PATH})
        
    if 'SCABBARD_METADATA_FILE' not in env:
        raise Exception("[scabbard.intercept.driver:ERR] `SCABBARD_METADATA_FILE` was not defined! [dev-error]")
    
    new_cmd: str
    if any([x in os.path.basename(argv[0]) for x in ["clang","hipcc","mpcc"]]):
        new_argv = list(argv)
        new_argv[1:1] = ADDED_FLAGS
        new_cmd = ' '.join(new_argv)
    elif "cmake" in os.path.basename(argv[0]):
        prRed("\n[scabbard.intercept.driver:WARN] Intercepted a CMake command!"+
              "\n                           -> CMake is not supported by the scabbard interceptor!"+
              "\n                              Try directly calling the configured build tool (i.e. `make`, `ninja`, etc.)\n")
        executeOriginalCommand(argv) # might try this for now
        return
    elif any([x in os.path.basename(argv[0]) for x in {"make", "ninja", "MSBuild"}]):
        try:
            executeOriginalCommand(argv)
        except Exception as e:
            prRed(f"[scabbard.intercept.driver:ERR] {e}")
            raise RuntimeError("failed to run the build command [driver.py]") from e
        except:
            raise RuntimeError("failed to run the build command [driver.py]")
        return
    else:
        new_cmd = ' '.join(argv)
    
    if DEBUG:
        prCyan(f"[driver.py] cmd run: {new_cmd}")
    try:
        cmdOutput = subprocess.run(new_cmd, shell=True, check=True, env=env) #, text=True,
                                    # stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        # print(cmdOutput.stdout)
    except subprocess.CalledProcessError as cpe:
        prRed(str(cpe.stderr) if cpe.stderr is not None else str(cpe.stdout))
        raise RuntimeError('Error when running scabbard.intercept on a build command [driver.py]') from cpe
    except Exception as e:
        prRed(e)
        raise RuntimeError("Error when running scabbard.intercept on a build command [driver.py]") from e




def main(argv: list) -> None:
    try:
        runCommandWithFlags(argv[1:])
    except Exception as e: # Fall back to original command
        prRed(e)
        executeOriginalCommand(argv[1:])



if __name__ == '__main__':
    main(sys.argv)
  
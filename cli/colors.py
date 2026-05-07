"""*
 * @file colors.py
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief derived from https://github.com/LLNL/FAROS/blob/e6c9ece1356c93418a04452f98f0f55497f4bf4d/interception/colors.py
 *        used to implement the LD_PRELOAD trick for getting the scabbard instrumenter working.
 * @date 2024-05-23
 * 
 *"""


def prGreen(skk):
    print("\033[92m{}\033[00m" .format(skk))

def prCyan(skk):
    print("\033[96m{}\033[00m" .format(skk))

def prRed(skk):
    print("\033[91m{}\033[00m" .format(skk))
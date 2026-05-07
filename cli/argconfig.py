"""*
 * @file argconfig.py
 * @author osterhoutan (osterhoutan+scabbard@gmail.com)
 * @brief configures the argument parser for the scabbard python script
 * @date 2024-05-23
 * 
 *"""
 
from argparse import ArgumentParser
from typing import List

__all__ = ['ScabbardArgParser','parseScabbardArgs','printScabbardHelp']

ScabbardArgParser: ArgumentParser = ArgumentParser(
        prog="scabbard",
        usage="""
        Scabbard is a tool that allows you to track down data races that occur between a GPU and CPU 
          (GPU write and CPU read) that occur because of the new heterogenous memory models gpu code
          now enjoys.  It is currently only designed to work with AMDs Hip programming language.
          
        Using scabbard is a three step process:
          (1) build your code using scabbards instrumentation tool.
          (2) run your code using the scabbard trace tool.
          (3) analyse the trace file produced with the scabbard verify tool.
          
        Use:   <>-data-type  {}-optional   []-required   ...-multiple accepted
          $ scabbard [mode] -h 
          Use -h on a subprogram/mode to leran more on how to use it.
        """,
        epilog="""
        no support information yet provided. (TODO)
        """    
    )

_SubArgParser = ScabbardArgParser.add_subparsers(
        title='mode',
        description="""
        The mode in which you wish the scabbard interface to function.
        """,
        help="Choose the mode that correlates to your current step",
    )

InstrArgParser = _SubArgParser.add_parser('instr',
        # aliases=['build','instrument','instr','compile','link'
        #          'Build','Instrument','Instr','Compile','Link'
        #          'BUILD','INSTRUMENT','INSTR','COMPILE','LINK'],
        prog='instrument single file compilation',
        help="use this to launch your build process(es) so that instrumented binaries are produced. (FOR SINGLE FILE BUILDS ONLY)",
        usage="""
            <>-data-type  {}-optional   []-required   ...-multiple accepted
        
        $ scabbard instr  [build-tool-exe] {extra-build args}...  
        
        example:
          $ scabbard instr hipcc main.cpp
        """,
        epilog="""
        no support information yet provided. (TODO)
        """   
    )
InstrArgParser.set_defaults(mode='instr')

BuildArgParser = _SubArgParser.add_parser('build',
        # aliases=['build','instrument','instr','compile','link'
        #          'Build','Instrument','Instr','Compile','Link'
        #          'BUILD','INSTRUMENT','INSTR','COMPILE','LINK'],
        prog='instrument a make or cmake recipe',
        help="use this to launch your build process(es) that have already been configured to use the scabbard instrumenter",
        usage="""
            <>-data-type  {}-optional   []-required   ...-multiple accepted
        
        $ scabbard build [build-tool-exe] {extra-build args}...  
        
        example:
          $ scabbard build make all -j 1
        """,
        epilog="""
        no support information yet provided. (TODO)
        """   
    )
BuildArgParser.set_defaults(mode='build')

RunArgParser = _SubArgParser.add_parser('run',
        aliases=['trace','run','launch','verify',
                 'Trace','Run','Launch','Verify',
                 "TRACE","RUN","LAUNCH","VERIFY"],
        prog='program-runner',
        help="use this to launch an instrumented executable to produce a trace file",
        usage="""
                <>-data-type   {}-optional   []-required   ...-multiple accepted
        
        $ scabbard run {options} [instrumented-exe] {extra-launch-args}...  
        
        example:
          $ scabbard run ./my-exe -my --extra="launch args"
        """,
        epilog="""
        no support information yet provided. (TODO)
        """  
    )
RunArgParser.set_defaults(mode='run')

VerifArgParser = _SubArgParser.add_parser('verif',
        # aliases=['verify','verif','analyse','analyze',
        #          'Verify','Verif','Analyze','Analyse',
        #          'VERIFY','VERIF','ANALYZE','ANALYSE'],
        prog='verifier',
        help="[LEGACY] This mode is no longer used as Scabbard is now an online data race detector."
             "(i.e. Verification occurs along side your application during the run step)"
             "[/LEGACY]",
        usage="""
            [LEGACY] 
              This mode is no longer used as Scabbard is now an online data race detector. (YEA!)
              (i.e. Verification occurs along side your application during the run step)
            [/LEGACY]
        """,
        epilog="""
        [LEGACY/]
        """ 
    )
VerifArgParser.set_defaults(mode='verif')


RunArgParser.add_argument('--device-buff-size','-b',
        nargs=1,
        default=None,
        metavar='<uint64>',
        required=False,
        help= "Size of the device cycle buffer--in number of event entires--"
              "generated for every kernel launch."
              "(default: 65536) "
              "[decrease if memory balloons; increase if data loss occurs]"
    )

def parseScabbardArgs(argv:List[str]) -> tuple:
    return ScabbardArgParser.parse_known_args(argv)


def printScabbardHelp() -> None:
    ScabbardArgParser.print_help()

# __all__ = ['ParsedArgs','parseArgs']


# @dataclass(repr=True,eq=False,order=False,unsafe_hash=True)
# class ParsedArgs:
#     mode: str | None
#     meta_file: Path | None
#     trace_file: Path | None
#     executable: Path | None
#     args: List[str] | None


# def _parseComplexModeArg(result:ParsedArgs, arg:str, argv: List[str]) -> None:
#     path = shutil.which(arg)


# def parseArgs(argv: List[str]) -> ParsedArgs:
#     result: ParsedArgs = ParsedArgs(None,None,None,None,None)
#     match str.lower(argv[1]):
#         case 'build' | 'instrument' | 'instr' | 'compile' | 'comp' | 'i' | 'b' | 'c':
#             result.mode = 'instr'
#         case 'run' | 'launch' | 'trace' | 'r' | 'l' | 't':
#             result.mode = 'run'
#         case 'verify' | 'verif' | 'ver' | 'analyse' | 'analyze' | 'anal' | 'v' | 'a':
#             result.mode = 'verif'
#         case 'make' | 'clang' | 'clang++' | 'hipcc' | 'ninja' | 'cc' | 'cpp':
#             result.mode = 'instr'
#             tmp = shutil.which(argv[1])
#             if tmp is not None:
#                 result.executable = Path(tmp)
#                 result.args = argv[2:] if len(argv)>=3 else None
#                 return result
#             else: 
#                 raise ArgumentError('mode',f"could not identify `{argv[1]}` as a scabbard mode or valid executable!")
#         case _:
#             _parseComplexModeArg(result, argv[1], argv[2:] if len(argv)>=3 else list())
    

 
 
 
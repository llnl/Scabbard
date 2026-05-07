#!/usr/bin/zsh
#flux: -N1
#flux: -n1
#flux: -c8
#flux: -g1

# NOTE: you either need to run this from the top level directory of scabbard or with SCABBARD_PATH already defined
# NOTE: you must have hipcc on your path or have ROCM_PATH defined


echo -e "\n\n==== CHECKING the Environment Variables ====\n\n"

# get the rocm and hipcc version
HIP_EXE=${ROCM_PATH:-/opt/rocm}/bin/hipcc

echo "HIPCC: '$HIP_EXE'"

if [ -z "$CXX_ARGS_EXTRA" ]; then
  CXX_ARGS_EXTRA=""
fi

# remove file extension
FILE=$(realpath $1)
FILE_BASE=$(echo "$FILE" | awk 'match($0, /^(.+)\.cpp$/, arr) { print arr[1]}')

if [ -z "$TEST_NO_INSTRUMENT" ]; then
  echo "" &> /dev/null
else
  echo "TEST_NO_INSTRUMENT: TRUE"
  echo -e "\n\n===== BUILDING with NO instrumentation ====\n\n"
  echo "$HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -o$FILE_BASE.out $FILE"
  $HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -o$FILE_BASE.out $FILE || \
    { printf "\n\e[31m[run_x.sh] ERROR: during NON-instrumented build\e[0m\n"; rm $FILE_BASE.out 1> /dev/null 2> /dev/null; exit -1; }
  echo -e "\n\n===== RUNNING with NO instrumentation ====\n\n"
  echo "$HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -o$FILE_BASE.out $FILE"
  $FILE_BASE.out || \
    { printf "\n\e[31m[run_x.sh] ERROR: during NON-instrumented run\e[0m\n"; rm $FILE_BASE.out 1> /dev/null 2> /dev/null; exit -1; }
  exit 0
fi


# if [[ -z "${ROCM_PATH}"]]; then
#     HIP_EXE="$(ROCM_PATH)/bin/hipcc"
# else
#   if [[ -n $ZSH_VERSION ]]; then
#     builtin whence -p "hipcc" &> /dev/null
#   else  # bash:
#     builtin type -P "hipcc" &> /dev/null
#   fi
#   [[ $? -eq 0 ]] && echo "ROCM_PATH was not defined and no hipcc found on path" && exit -1
#   HIP_EXE="hipcc"
# fi

if [[ -v SCABBARD_PATH ]]; then
  echo "" &> /dev/null # don't do anything, just continue
else
  echo -e "\n:ERROR: SCABBARD_PATH was not defined!\n  \$SCABBARD_PATH: \`$SCABBARD_PATH\`\n"
  exit -1
fi
alias scabbard="$SCABBARD_PATH/scabbard"

# export SCABBARD_METADATA_FILE="$FILE_BASE.scabbard.meta"
# rm $SCABBARD_METADATA_FILE 1> /dev/null 2> /dev/null
# rm ./anon.scabbard.meta 1> /dev/null 2> /dev/null
# rm ./anon.scabbard.meta.lock 1> /dev/null 2> /dev/null

if [ -z "$TEST_IR" ]; then
  echo "" &> /dev/null
else
  echo "TEST_IR: TRUE"
  echo -e "\n\n===== BUILDING IR with Instrumentation ====\n\n"
  echo "scabbard instr $HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -S -emit-llvm -o$FILE_BASE.ll $FILE"
  scabbard instr $HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -S -emit-llvm -o$FILE_BASE.ll $FILE || \
    { printf "\n\e[31m[run_x.sh] ERROR: during NON-instrumented build\e[0m\n"; rm $FILE_BASE.ll 1> /dev/null 2> /dev/null; exit -1; }
  echo -e "\n\n===== DONE ====\nSee... $FILE_BASE.ll\n\n"
  exit 0
fi

# determine if this should be compiled as a manual or instrumented test
if [[ $FILE_BASE == *".man" ]]; then # this is a manual file

  echo -e "\n\n==== BUILDING the MANUALLY instrumented executable ====\n\n"
  
  # make sure they provide a metadata file for manually instrumented files
  test -f $SCABBARD_METADATA_FILE || \ 
    { printf "\n\e[31m[run_x.sh] ERROR: manually instrumented files are no longer supported without accompaning metadata files\e[0m\n    \e[31m\xE2\x86\xB3\e[0m Please Create and poulate $SCABBARD_METADATA_FILE to use this util\n"; exit -1; }
  
  echo "$HIP_EXE $CXX_ARGS_EXTRA -L$SCABBARD_PATH -ltrace -lpthread -I$(pwd)/lib/include -I$(pwd)/libtrace/include -I$(pwd)/libtrace -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE"
  $HIP_EXE $CXX_ARGS_EXTRA -L$SCABBARD_PATH -ltrace -lpthread -I$(pwd)/lib/include -I$(pwd)/libtrace/include -I$(pwd)/libtrace -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE || \
    { printf "\n\e[31m[run_x.sh] ERROR: during build of manually instrumented file\e[0m\n"; exit -1; }


else # this is a file that needs to be instrumented

  echo -e "\n\n==== INSTRUMENTING and BUILDING the executable ====\n\n"

  # echo "$HIP_EXE -fpass-plugin=$SCABBARD_PATH/libinstr.so -L$SCABBARD_PATH -ltrace -lpthread -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE"
  # $HIP_EXE -fpass-plugin=$SCABBARD_PATH/libinstr.so -L$SCABBARD_PATH -ltrace -lpthread -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE || \
  #   { printf "\n\e[31m[run_x.sh] ERROR: during instrumented build\e[0m\n"; rm $SCABBARD_METADATA_FILE.lock 1> /dev/null 2> /dev/null; exit -1; }

  echo "scabbard instr $HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE"
  scabbard instr $HIP_EXE $CXX_ARGS_EXTRA -std=c++17 -x hip -g -O2 -o$FILE_BASE.instr.out $FILE || \
    { printf "\n\e[31m[run_x.sh] ERROR: during instrumented build\e[0m\n"; rm $SCABBARD_METADATA_FILE.lock 1> /dev/null 2> /dev/null; exit -1; }

  # # remove lock file regardless of the outcome
  # rm $SCABBARD_METADATA_FILE.lock 1> /dev/null 2> /dev/null
  # # make sure that a metadata file exists after instrumentation
  # test -f $SCABBARD_METADATA_FILE || 
  #   { printf "\n\e[31m[run_x.sh] ERROR: metadata file \"$SCABBARD_METADATA_FILE\" was not created during the instrumentation/build process\e[0m\n"; exit -1; }

fi


#compile the required environment variables
export SCABBARD_INSTRUMENTED_EXE_NAME="$FILE_BASE.instr.out"
export SCABBARD_TRACE_FILE="$FILE_BASE.scabbard.trace"

# run the built executable
echo -e "\n\n==== RUNNING the executable ====\n\nthe executable: $SCABBARD_INSTRUMENTED_EXE_NAME\n"
echo "scabbard trace --trace-file=$SCABBARD_TRACE_FILE $SCABBARD_INSTRUMENTED_EXE_NAME"
scabbard trace --trace-file=$SCABBARD_TRACE_FILE $SCABBARD_INSTRUMENTED_EXE_NAME || \
  { printf "\n\e[31m[run_x.sh] ERROR: during trace\e[0m\n"; exit -1; }

# $ROCM_PATH/bin/rocgdb $SCABBARD_INSTRUMENTED_EXE_NAME

# <<< removed -> moved to an online approach >>>
# make sure that a trace file exists after instrumentation
# test -e $SCABBARD_TRACE_FILE || \ 
#   { printf "\n\e[31m[run_x.sh] ERROR: the trace file \"$SCABBARD_TRACE_FILE\" was not created during the trace/running step\e[0m\n"; exit -1; }

# <<< removed -> moved to an online approach >>>
# run the verifier
# echo -e "\n\n==== VERIFYING generated trace file ====\n\n"
# echo "scabbard verif $SCABBARD_METADATA_FILE $SCABBARD_TRACE_FILE"
# scabbard verif $SCABBARD_METADATA_FILE $SCABBARD_TRACE_FILE || \
#   { printf "\n\e[31m[run_x.sh] ERROR: during verify\e[0m\n"; exit -1; }


echo -e "\n\n==== END ====\n\n"

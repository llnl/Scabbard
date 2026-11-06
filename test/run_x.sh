#!/usr/bin/zsh
#flux: -N1
#flux: -n1
#flux: -c16
#flux: -g1

# NOTE: you either need to run this from the top level directory of scabbard or with SCABBARD_PATH already defined
# NOTE: you must have hipcc on your path or have ROCM_PATH defined


echo -e "\n\n==== CHECKING the Environment Variables ====\n\n"

# get the rocm and hipcc version
HIP_EXE=${ROCM_PATH:-/opt/rocm}/bin/hipcc
ROCGDB_EXE=${ROCM_PATH:-/opt/rocm}/bin/rocgdb
# HIP_EXE=/opt/rocm-6.4.2/bin/hipcc
# if [ -z "$ROCM_PATH" ]; then
#   CXX_ARGS_EXTRA=""
# else

# fi


echo "HIPCC: '$HIP_EXE'"

if [ -z "$CXX_ARGS_EXTRA" ]; then
  CXX_ARGS_EXTRA=""
fi

if [ -z "$RUN_ARGS" ]; then
  RUN_ARGS=""
fi

if [ -z "$GDB_ARGS" ]; then
  GDB_ARGS=""
fi

if [ -z "$APP_ARGS" ]; then
  APP_ARGS=""
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



#compile the required environment variables
SCABBARD_INSTRUMENTED_EXE_NAME="$FILE_BASE.instr.out"

if [ -z "$USE_GDB" ]; then
# run the built executable
echo -e "\n\n==== RUNNING the executable ====\n\nthe executable: $SCABBARD_INSTRUMENTED_EXE_NAME\n"
echo "scabbard run $RUN_ARGS $SCABBARD_INSTRUMENTED_EXE_NAME $APP_ARGS"
scabbard run $RUN_ARGS $SCABBARD_INSTRUMENTED_EXE_NAME $APP_ARGS || \
  { printf "\n\e[31m[run_x.sh] ERROR: running instrumented exe\e[0m\n"; exit -1; }

else
# run the built executable with rocgdb
echo -e "\n\n==== RUNNING the executable w/rocgdb ====\n\nthe executable: $SCABBARD_INSTRUMENTED_EXE_NAME\n"
echo "scabbard run $RUN_ARGS $ROCGDB_EXE $GDB_ARGS $SCABBARD_INSTRUMENTED_EXE_NAME $APP_ARGS"
scabbard run $RUN_ARGS $ROCGDB_EXE $GDB_ARGS $SCABBARD_INSTRUMENTED_EXE_NAME $APP_ARGS || \
  { printf "\n\e[31m[run_x.sh] ERROR: running instrumented exe w/ \"$ROCGDB_EXE\"\e[0m\n"; exit -1; }
fi



echo -e "\n\n==== END ====\n\n"

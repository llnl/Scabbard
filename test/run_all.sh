#!/bin/bash

#FLUX -N1 -n1 -g1 -c16


./test/run_x.sh ./test/device/def-race.cpp >> res-def.txt 2>&1
./test/run_x.sh ./test/device/maybe-race.cpp >> res-maybe.txt 2>&1
./test/run_x.sh ./test/device/no-race.cpp >> res-no.txt 2>&1

./test/run_x.sh ./test/device/dual-stream/def-race.cpp >> res-dual-def.txt 2>&1
./test/run_x.sh ./test/device/dual-stream/maybe-race.cpp >> res-dual-maybe.txt 2>&1
./test/run_x.sh ./test/device/dual-stream/no-race.cpp >> res-dual-no.txt 2>&1

#! /usr/bin/bash


# /usr/share/lmod/lmod/libexec/ml_cmd rocm/7.0.0
# export ROCM_PATH="/opt/rocm-7.0.0"
# /usr/share/lmod/lmod/libexec/ml_cmd rocm/6.3.1
# export ROCM_PATH="/opt/rocm-6.3.1"
/usr/share/lmod/lmod/libexec/ml_cmd rocm/6.3.1
export ROCM_PATH="/opt/rocm-6.3.1"
/usr/share/lmod/lmod/libexec/ml_cmd python/3.10
/usr/share/lmod/lmod/libexec/ml_cmd cmake/3.29.2

/usr/tce/packages/cmake/cmake-3.29.2/bin/cmake "$@"

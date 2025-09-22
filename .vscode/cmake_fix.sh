#! /usr/bin/bash


/usr/share/lmod/lmod/libexec/ml_cmd rocm/6.4.2
export ROCM_PATH="/opt/rocm-6.4.2"
/usr/share/lmod/lmod/libexec/ml_cmd python/3.10
/usr/share/lmod/lmod/libexec/ml_cmd cmake/3.29.2

/usr/tce/packages/cmake/cmake-3.29.2/bin/cmake "$@"

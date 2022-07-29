#!/bin/bash
module swap PrgEnv-intel PrgEnv-gnu
module load cce
module load boost
module swap gcc/9.3.0 gcc/7.3.0
module swap cray-libsci/20.06.1 cray-libsci/20.03.1
module unload darshan
. ~/spack/share/spack/setup-env.sh
export CRAYPE_LINK_TYPE=dynamic

#!/bin/bash
spack install mochi-symbiomon@develop+aggregator+bedrock+reducer ^mochi-reducer@develop+aggregator ^mochi-sdskv@develop-test+aggrservice ^libfabric fabrics=gni %gcc@7.3.0
spack install mochi-sdskv@develop-test+aggrservice+symbiomon ^libfabric fabrics=gni %gcc@7.3.0
spack install mochi-reducer@develop+aggregator+symbiomon ^mochi-sdskv@develop-test+aggrservice+symbiomon ^libfabric fabrics=gni %gcc@7.3.0
#spack install py-mochi-symbiomon ^mochi-symbiomon@develop+aggregator+bedrock+reducer ^libfabric fabrics=verbs,rxm ^python@2.7.5+ucs4


#!/bin/bash
# Build EricaNext (source/ -> build/, installed to install/ of this repository) against ANLNext in $ANLNEXT_INSTALL.
# run_script/run_chain.py reads install/lib/python, so PYTHONPATH does not need this build.
#   bash scripts/env/build_ericanext.sh
#   ANLNEXT_INSTALL=$HOME/opt/anlnext-g4 bash scripts/env/build_ericanext.sh
# With EricaNext_try set up, `source ~/opt/setup_ericanext.sh` first (sets ANLNEXT_INSTALL and the libraries).
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../.." && pwd)
: "${ANLNEXT_INSTALL:?set ANLNEXT_INSTALL to the ANLNext installation (the directory with include/ and lib/)}"
[ -f "$ANLNEXT_INSTALL/include/anlnext/BasicModule.hh" ] || { echo "no ANLNext in $ANLNEXT_INSTALL"; exit 1; }
PYTHON=${PYTHON:-$(command -v python3)}

cd $REPO
rm -rf build install && mkdir build install && cd build
cmake ../source -DCMAKE_INSTALL_PREFIX=../install -DANLNEXT_INSTALL=$ANLNEXT_INSTALL -DPython3_EXECUTABLE=$PYTHON
make -j$(nproc) && make install
echo "built: $REPO/install (EricaPy in install/lib/python)"

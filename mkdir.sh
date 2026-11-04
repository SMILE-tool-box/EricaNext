rm -rf build
mkdir build
cd build
cmake ../source/ -DCMAKE_INSTALL_PREFIX=/usr/local -DUSE_PYTHON=ON
make
sudo make install

apt-get install -y autoconf curl libtool
curl -O https://capnproto.org/capnproto-c++-0.6.1.tar.gz
tar xvf capnproto-c++-0.6.1.tar.gz
cd capnproto-c++-0.6.1
./configure --prefix=/usr/local CPPFLAGS=-DPIC CFLAGS=-fPIC CXXFLAGS=-fPIC LDFLAGS=-fPIC --disable-shared --enable-static
make -j4
make install

cd ..
git clone https://github.com/commaai/c-capnproto.git
cd c-capnproto
git submodule update --init --recursive
autoreconf -f -i -s
CFLAGS="-fPIC" ./configure --prefix=/usr/local
make -j4
make install


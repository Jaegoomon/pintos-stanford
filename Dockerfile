FROM ubuntu:14.04

WORKDIR /root

RUN apt update
RUN apt-get install -y build-essential \ 
    software-properties-common \
    wget \
    gdb
# Install compiler
RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test
RUN apt install -y gcc-4.4 g++-4.4

RUN apt update
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.4 60 --slave /usr/bin/g++ g++ /usr/bin/g++-4.4

# Install bochs emulator
# RUN wget --no-check-certificate https://sourceforge.net/projects/bochs/files/bochs/2.6.2/bochs-2.6.2.tar.gz/download
# RUN tar -xzvf download
# RUN cd bochs-2.6.2/ && \
#     ./configure --enable-gdb-stub --with-nogui && \
#     make install
# RUN cd ~ && \
#     rm -rf download bochs-2.6.2

# Install i386 toolchain
RUN wget --no-check-certificate https://ftp.gnu.org/gnu/binutils/binutils-2.27.tar.gz && tar xzf binutils-2.27.tar.gz
RUN cd binutils-2.27 && \
    ./configure --target=i386-elf --disable-multilib --disable-nls --disable-werror && \
    make -j8 && \
    make install && \
    cd ~

RUN wget --no-check-certificate https://ftp.gnu.org/gnu/gcc/gcc-6.2.0/gcc-6.2.0.tar.bz2 && tar xjf gcc-6.2.0.tar.bz2
RUN cd gcc-6.2.0 && \
    ./contrib/download_prerequisites && \
    ./configure --target=i386-elf --disable-multilib --disable-nls \
    --disable-werror --disable-libssp --disable-libmudflap \
    --with-newlib --without-headers --enable-languages=c,c++ && \
    make -j8 all-gcc && \
    make install-gcc

RUN rm -rf binutils-2.27* gcc-6.2.0*

# Install qemu simulator
RUN apt install -y qemu
RUN ln -s /usr/bin/qemu-system-i386 /usr/bin/qemu

RUN echo "export PINTOSDIR=\$HOME" >> ~/.bashrc
RUN echo "export PATH=\$PINTOSDIR/src/utils:\$PATH" >> ~/.bashrc
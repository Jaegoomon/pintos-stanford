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
RUN wget --no-check-certificate https://sourceforge.net/projects/bochs/files/bochs/2.6.2/bochs-2.6.2.tar.gz/download
RUN tar -xzvf download
RUN cd bochs-2.6.2/ && \
    ./configure --enable-gdb-stub --with-nogui && \
    make install
RUN cd ~ && \
    rm -rf download bochs-2.6.2

RUN echo "export PINTOSDIR=\$HOME" >> ~/.bashrc
RUN echo "export PATH=\$PINTOSDIR/src/utils:\$PATH" >> ~/.bashrc
FROM ubuntu:20.04
ENV PYTHONUNBUFFERED 1
ENV PYTHONPATH /tmp/openpilot:$PYTHONPATH

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    bzip2 \
    ca-certificates \
    capnproto \
    clang \
    curl \
    g++ \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    git \
    libarchive-dev \
    libavformat-dev libavcodec-dev libavdevice-dev libavutil-dev libswscale-dev libavresample-dev libavfilter-dev \
    libbz2-dev \
    libcapnp-dev \
    libcurl4-openssl-dev \
    libffi-dev \
    libtool \
    libssl-dev \
    libsqlite3-dev \
    libusb-1.0-0 \
    libzmq3-dev \
    locales \
    opencl-headers \
    ocl-icd-opencl-dev \
    make \
    patch \
    pkg-config \
    python \
    python-dev \
    unzip \
    wget \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* && \
    cd /usr/lib/gcc/arm-none-eabi/9.2.1 && \
    rm -rf arm/ && \
    rm -rf thumb/nofp thumb/v6* thumb/v8* thumb/v7+fp thumb/v7-r+fp.sp

RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV PATH="/root/.pyenv/bin:/root/.pyenv/shims:${PATH}"

ENV PANDA_PATH=/tmp/openpilot/panda
ENV OPENPILOT_REF="ee0dd36a3c775dbd82493c84f4e7272c1eb3fcbd"
ENV OPENDBC_REF="296f190000a2e71408e207ba21a2257cc853ec15"

COPY requirements.txt /tmp/
RUN pyenv install 3.8.10 && \
    pyenv global 3.8.10 && \
    pyenv rehash && \
    pip install --no-cache-dir -r /tmp/requirements.txt

ENV CPPCHECK_DIR=/tmp/cppcheck
COPY tests/misra/install.sh /tmp/
RUN /tmp/install.sh

RUN git config --global --add safe.directory /tmp/openpilot/panda
RUN cd /tmp && \
    git clone https://github.com/commaai/openpilot.git tmppilot || true && \
    cd /tmp/tmppilot && \
    git fetch origin $OPENPILOT_REF && \
    git checkout $OPENPILOT_REF && \
    git submodule update --init cereal opendbc rednose_repo && \
    git -C opendbc fetch && \
    git -C opendbc checkout $OPENDBC_REF && \
    git -C opendbc reset --hard HEAD && \
    git -C opendbc clean -xfd && \
    mkdir /tmp/openpilot && \
    cp -pR SConstruct site_scons/ tools/ selfdrive/ system/ common/ cereal/ opendbc/ rednose/ third_party/ /tmp/openpilot && \
    rm -rf /tmp/openpilot/panda && \
    rm -rf /tmp/tmppilot

RUN cd /tmp/openpilot && \
    git clone https://github.com/commaai/panda_jungle.git && \
    cd panda_jungle && \
    git fetch && \
    git checkout 7b7197c605915ac34f3d62f314edd84e2e78a759 && \
    rm -rf .git/

RUN cd /tmp/openpilot && \
    pip install --no-cache-dir -r opendbc/requirements.txt && \
    pip install --no-cache-dir --upgrade aenum lru-dict pycurl tenacity atomicwrites serial smbus2

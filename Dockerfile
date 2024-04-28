FROM ubuntu:24.04
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
    libavformat-dev libavcodec-dev libavdevice-dev libavutil-dev libswscale-dev libavfilter-dev \
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
    python3 \
    python3-dev \
    python3-pip \
    python-is-python3 \
    qtbase5-dev \
    unzip \
    wget \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/* && \
    cd /usr/lib/gcc/arm-none-eabi/* && \
    rm -rf arm/ && \
    rm -rf thumb/nofp thumb/v6* thumb/v8* thumb/v7+fp thumb/v7-r+fp.sp

RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ENV PANDA_PATH=/tmp/openpilot/panda
ENV OPENPILOT_REF="3e1617deaacc3a03e5b79dc9b432dddb877edc61"
ENV OPENDBC_REF="e0d4be4a6215d44809718dc84efe1b9f0299ad63"

COPY requirements.txt /tmp/
RUN pip3 install --break-system-packages --no-cache-dir -r /tmp/requirements.txt

ENV PATH="/tmp/venv/bin/:$PATH"

ENV CPPCHECK_DIR=/tmp/cppcheck
COPY tests/misra/install.sh /tmp/
RUN /tmp/install.sh
ENV SKIP_CPPCHECK_INSTALL=1

RUN git config --global --add safe.directory /tmp/openpilot/panda
RUN cd /tmp && \
    git clone https://github.com/commaai/openpilot.git tmppilot || true && \
    cd /tmp/tmppilot && \
    git fetch origin $OPENPILOT_REF && \
    git checkout $OPENPILOT_REF && \
    git submodule update --init cereal opendbc rednose_repo body && \
    git -C opendbc fetch && \
    git -C opendbc checkout $OPENDBC_REF && \
    git -C opendbc reset --hard HEAD && \
    git -C opendbc clean -xfd && \
    mkdir /tmp/openpilot && \
    cp -pR SConstruct site_scons/ tools/ selfdrive/ system/ common/ cereal/ opendbc/ rednose/ rednose_repo/ third_party/ body/ /tmp/openpilot && \
    rm -rf /tmp/openpilot/panda && \
    rm -rf /tmp/tmppilot

RUN cd /tmp/openpilot && \
    pip3 install --break-system-packages --no-cache-dir -r opendbc/requirements.txt && \
    pip3 install --break-system-packages --no-cache-dir --upgrade aenum lru-dict pycurl tenacity atomicwrites serial smbus2


# for Jenkins
COPY README.md panda.tar.* /tmp/
RUN mkdir /tmp/openpilot/panda && \
    tar -xvf /tmp/panda.tar.gz -C /tmp/openpilot/panda/ || true

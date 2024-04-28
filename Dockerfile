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

COPY requirements.txt /tmp/
RUN pip3 install --break-system-packages --no-cache-dir -r /tmp/requirements.txt

ENV PATH="/tmp/venv/bin/:$PATH"

ENV CPPCHECK_DIR=/tmp/cppcheck
COPY tests/misra/install.sh /tmp/
RUN /tmp/install.sh
ENV SKIP_CPPCHECK_INSTALL=1

ENV CEREAL_REF="861144c136c91f70dcbc652c2ffe99f57440ad47"
ENV OPENDBC_REF="e0d4be4a6215d44809718dc84efe1b9f0299ad63"

RUN git config --global --add safe.directory /tmp/openpilot/panda
RUN mkdir -p /tmp/openpilot/ && \
    cd /tmp/openpilot/ && \
    git clone --depth 1 https://github.com/commaai/cereal && \
    git clone --depth 1 https://github.com/commaai/opendbc && \
    cd cereal && git fetch origin $CEREAL_REF && git checkout FETCH_HEAD && rm -rf .git/ && cd .. && \
    cd opendbc && git fetch origin $OPENDBC_REF && git checkout FETCH_HEAD && rm -rf .git/ && cd .. && \
    cp -pR opendbc/SConstruct opendbc/site_scons/ . && \
    scons -j8 --minimal opendbc/ cereal/

RUN cd /tmp/openpilot && \
    pip3 install --break-system-packages --no-cache-dir -r opendbc/requirements.txt

# for Jenkins
COPY README.md panda.tar.* /tmp/
RUN mkdir /tmp/openpilot/panda && \
    tar -xvf /tmp/panda.tar.gz -C /tmp/openpilot/panda/ || true

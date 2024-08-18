FROM ubuntu:24.04

ENV PYTHONUNBUFFERED 1
ENV PYTHONPATH /tmp/pythonpath:$PYTHONPATH

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y --no-install-recommends \
    make \
    g++ \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    git \
    libffi-dev \
    libusb-1.0-0 \
    python3 \
    python3-dev \
    python3-pip \
 && rm -rf /var/lib/apt/lists/* && \
    apt clean && \
    cd /usr/lib/gcc/arm-none-eabi/* && \
    rm -rf arm/ && \
    rm -rf thumb/nofp thumb/v6* thumb/v8* thumb/v7+fp thumb/v7-r+fp.sp

COPY requirements.txt /tmp/
RUN pip3 install --break-system-packages --no-cache-dir -r /tmp/requirements.txt

ENV CPPCHECK_DIR=/tmp/cppcheck
COPY tests/misra/install.sh /tmp/
RUN /tmp/install.sh && rm -rf $CPPCHECK_DIR/.git/
ENV SKIP_CPPCHECK_INSTALL=1

# TODO: this should be a "pip install" or not even in this repo at all
ENV OPENDBC_REF="d377af6c2d01b30d3de892cee91f1ed8fb50d6e8"
RUN git config --global --add safe.directory /tmp/pythonpath/panda
RUN mkdir -p /tmp/pythonpath/ && \
    cd /tmp/ && \
    git clone --depth 1 https://github.com/commaai/opendbc && \
    cd opendbc && git fetch origin $OPENDBC_REF && git checkout FETCH_HEAD && rm -rf .git/ && \
    pip3 install --break-system-packages --no-cache-dir Cython numpy  && \
    scons -j8 --minimal opendbc/ && \
    mv opendbc/ $PYTHONPATH && rm -rf /tmp/opendbc/

# for Jenkins
COPY README.md panda.tar.* /tmp/
RUN mkdir /tmp/pythonpath/panda && \
    tar -xvf /tmp/panda.tar.gz -C /tmp/pythonpath/panda/ || true

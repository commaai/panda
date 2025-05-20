FROM ubuntu:24.04

ENV PYTHONUNBUFFERED=1
ENV PYTHONPATH=/tmp/pythonpath

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    make \
    g++ \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    git \
    libffi-dev \
    libusb-1.0-0 \
    python3-dev \
    python3-pip \
 && rm -rf /var/lib/apt/lists/* && \
    apt clean

ENV CPPCHECK_DIR=/tmp/cppcheck
COPY tests/misra/install.sh /tmp/
RUN /tmp/install.sh && rm -rf $CPPCHECK_DIR/.git/
ENV SKIP_CPPCHECK_INSTALL=1

COPY pyproject.toml __init__.py $PYTHONPATH/panda/
COPY python/__init__.py $PYTHONPATH/panda/python/
RUN pip3 install --break-system-packages --no-cache-dir $PYTHONPATH/panda/[dev]

RUN git config --global --add safe.directory $PYTHONPATH/panda

# for Jenkins
COPY README.md panda.tar.* /tmp/
RUN mkdir -p /tmp/pythonpath/panda && \
    tar -xvf /tmp/panda.tar.gz -C /tmp/pythonpath/panda/ || true

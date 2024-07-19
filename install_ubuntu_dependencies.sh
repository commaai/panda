#!/usr/bin/env bash
set -e

SUDO=""

# Use sudo if not root
if [[ ! $(id -u) -eq 0 ]]; then
  if [[ -z $(which sudo) ]]; then
    echo "Please install sudo or run as root"
    exit 1
  fi
  SUDO="sudo"
fi

$SUDO apt-get update 
$SUDO apt-get install -y --no-install-recommends \
    make \
    bzip2 \
    ca-certificates \
    capnproto \
    clang \
    g++ \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    git \
    libarchive-dev \
    libbz2-dev \
    libcapnp-dev \
    libffi-dev \
    libtool \
    libusb-1.0-0 \
    libzmq3-dev \
    locales \
    opencl-headers \
    ocl-icd-opencl-dev \
    python3 \
    python3-dev \
    python3-pip \
    python3-venv \
    python-is-python3 \
    zlib1g-dev \
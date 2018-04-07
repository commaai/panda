FROM ubuntu:16.04

RUN apt-get update && apt-get install -y gcc-arm-none-eabi libnewlib-arm-none-eabi gperf texinfo help2man python python-pip
RUN pip install setuptools

COPY . /panda

WORKDIR /panda

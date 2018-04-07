FROM ubuntu:16.04

RUN apt-get update && apt-get install -y clang make python python-pip
COPY tests/requirements.txt /panda/tests/requirements.txt
RUN pip install -r /panda/tests/requirements.txt
COPY . /panda


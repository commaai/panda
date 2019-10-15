FROM ubuntu:16.04

RUN apt-get update && apt-get install -y make python python-pip git
COPY requirements.txt /tmp/
RUN pip install -r /tmp/requirements.txt
COPY . /panda

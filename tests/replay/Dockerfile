FROM ubuntu:16.04

RUN apt-get update && apt-get install -y make python python-pip git
COPY tests/safety/requirements.txt /panda/tests/safety/requirements.txt
RUN pip install -r /panda/tests/safety/requirements.txt
COPY . /panda

WORKDIR /panda/tests/replay
RUN git clone https://github.com/commaai/openpilot-tools.git openpilot_tools 


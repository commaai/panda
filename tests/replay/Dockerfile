FROM ubuntu:16.04

RUN apt-get update && apt-get install -y make clang python python-pip git libarchive-dev libusb-1.0-0

RUN git clone --depth 1 https://github.com/commaai/openpilot.git || true
RUN rm -rf /openpilot/panda
COPY . /openpilot/panda

RUN pip install -r /openpilot/panda/tests/replay/requirements.txt
RUN ./openpilot/panda/tests/replay/install_capnp.sh

WORKDIR /openpilot/panda/tests/replay
RUN git clone https://github.com/commaai/openpilot-tools.git openpilot_tools || true

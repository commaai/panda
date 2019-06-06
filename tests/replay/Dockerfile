FROM ubuntu:16.04

RUN apt-get update && apt-get install -y make clang python python-pip git libarchive-dev libusb-1.0-0

RUN git clone https://github.com/commaai/openpilot.git || true
WORKDIR /openpilot
RUN git checkout 1e8098c140d924adb70167a216837fbf61d85f29
RUN rm -rf /openpilot/panda
COPY . /openpilot/panda

WORKDIR /
RUN pip install -r /openpilot/panda/tests/replay/requirements.txt
RUN ./openpilot/panda/tests/replay/install_capnp.sh

WORKDIR /openpilot/panda/tests/replay
RUN git clone https://github.com/commaai/openpilot-tools.git openpilot_tools || true
WORKDIR openpilot_tools
RUN git checkout 3df301e5783d7dc37cf3b079e96ad07cd4f2d0c3

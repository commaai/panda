FROM ubuntu:16.04

RUN apt-get update && apt-get install -y make clang python python-pip git libarchive-dev libusb-1.0-0

COPY tests/safety_replay/requirements.txt requirements.txt
RUN pip install -r requirements.txt
COPY tests/safety_replay/install_capnp.sh install_capnp.sh
RUN ./install_capnp.sh

RUN git clone https://github.com/commaai/openpilot.git || true
WORKDIR /openpilot
RUN git checkout 1e8098c140d924adb70167a216837fbf61d85f29
RUN rm -rf /openpilot/panda
COPY . /openpilot/panda

WORKDIR /openpilot/panda/tests/safety_replay
RUN git clone https://github.com/commaai/openpilot-tools.git openpilot_tools || true
WORKDIR openpilot_tools
RUN git checkout 3df301e5783d7dc37cf3b079e96ad07cd4f2d0c3

FROM ubuntu:24.04

ENV PYTHONUNBUFFERED=1
ENV WORKDIR=/tmp/panda/

ENV VIRTUAL_ENV=/tmp/.venv
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

ENV DEBIAN_FRONTEND=noninteractive

# deps install
COPY pyproject.toml __init__.py setup.sh /tmp/
RUN mkdir -p /tmp/python && touch /tmp/python/__init__.py
RUN apt-get update && apt-get install -y --no-install-recommends sudo && /tmp/setup.sh

RUN git config --global --add safe.directory $WORKDIR/panda

COPY . $WORKDIR

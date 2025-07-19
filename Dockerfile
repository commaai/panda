FROM ubuntu:24.04

ENV PYTHONUNBUFFERED=1
ENV VIRTUAL_ENV=/tmp/.venv
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

# deps install
COPY pyproject.toml __init__.py setup.sh /tmp/
RUN mkdir -p /tmp/python && touch /tmp/python/__init__.py
RUN apt-get update && apt-get install -y --no-install-recommends sudo && DEBIAN_FRONTEND=noninteractive /tmp/setup.sh

ENV WORKDIR=/tmp/panda/
RUN git config --global --add safe.directory $WORKDIR/panda
COPY . $WORKDIR
WORKDIR $WORKDIR

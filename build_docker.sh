#!/bin/bash
echo "
FROM debian:buster

RUN apt -y update && apt -y install build-essential gcc-multilib g++-multilib
RUN cd /work && ./build.sh
" | docker build --force-rm --no-cache -f - -v `pwd`:/work --output type=tar,dest=/dev/null

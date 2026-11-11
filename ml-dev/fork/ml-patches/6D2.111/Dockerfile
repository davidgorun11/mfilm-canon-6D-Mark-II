# Everything build_fork.sh needs for the 6D Mark II, on any machine with Docker:
#   docker build -t mfilm-build ml-dev/fork/ml-patches/6D2.111
#   docker run --rm -v "$PWD:/w" -w /w mfilm-build ml-dev/build_fork.sh 6D2.111
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential make git zip unzip python3 python3-setuptools python3-numpy python3-segno python3-docutils \
    gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi qemu-user ca-certificates xxd \
    && rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/reticulatedpines/magiclantern_simplified /root/ml/magiclantern_simplified \
    && git -C /root/ml/magiclantern_simplified checkout -q ed3e7c0df

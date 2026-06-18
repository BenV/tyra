# syntax=docker/dockerfile:1
FROM ubuntu:24.04

# Install necessary packages for PS2DEV, VCLPP and VCL.
ENV PS2DEV=/usr/local/ps2dev
RUN mkdir -p $PS2DEV
RUN chown -R $USER: $PS2DEV

RUN apt-get update
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get install -y git make g++ texinfo bison flex gettext libgmp3-dev \
    libmpfr-dev libmpc-dev gcc binutils cmake wget patch zlib1g-dev libgsl-dev \
    autopoint unzip curl genisoimage ffmpeg make libmpc-dev qemu-user-static \
    binfmt-support psmisc optipng pngquant

# Setup PS2DEV env
ENV PS2DEV=/usr/local/ps2dev
ENV PS2SDK=$PS2DEV/ps2sdk
ENV GSKIT=$PS2DEV/gsKit
ENV PATH=$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin

ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        PS2DEV_FILE="ps2dev-ubuntu-24.04-arm.tar.gz"; \
    else \
        PS2DEV_FILE="ps2dev-ubuntu-latest.tar.gz"; \
    fi && \
    curl -o ps2dev-latest.tar.gz -LC - "https://github.com/ps2dev/ps2dev/releases/download/latest/${PS2DEV_FILE}" && \
    tar -xf ps2dev-latest.tar.gz --strip-components 1 -C $PS2DEV

# Compile VCLPP
RUN mkdir -p /temp/vclpp
RUN git clone https://github.com/glampert/vclpp.git /temp/vclpp
WORKDIR "/temp/vclpp"
RUN make

# Download VCL
RUN mkdir -p /temp/vcl
WORKDIR "/temp/vcl"
RUN wget https://github.com/h4570/tyra/raw/master/assets/vcl

RUN cp /temp/vcl/vcl /usr/bin/vcl && \
    cp /temp/vclpp/vclpp /usr/bin/vclpp && \
    rm -rf /temp/vcl && \
    rm -rf /temp/vclpp

# Expose Docker's automatic architecture variable to the build
ARG TARGETARCH

# Conditionally split the repositories ONLY if building on ARM64
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        sed -i 's/Types: deb/Types: deb\nArchitectures: arm64/' /etc/apt/sources.list.d/ubuntu.sources && \
        printf "Types: deb\n\
URIs: http://archive.ubuntu.com/ubuntu/\n\
Suites: noble noble-updates noble-backports\n\
Components: main universe restricted multiverse\n\
Architectures: i386\n\
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n" > /etc/apt/sources.list.d/ubuntu-i386.sources; \
    fi

# Add the architecture and update (works natively on both systems now)
RUN dpkg --add-architecture i386 && apt-get update
RUN apt-get install -y libstdc++5:i386
RUN update-binfmts --install i386 /usr/bin/qemu-i386-static --magic '\x7fELF\x01\x01\x01\x03\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x03\x00\x01\x00\x00\x00' --mask '\xff\xff\xff\xff\xff\xff\xff\xfc\xff\xff\xff\xff\xff\xff\xff\xff\xf8\xff\xff\xff\xff\xff\xff\xff'

# Set chmod
RUN chmod 755 /usr/bin/vclpp && \
    chmod 755 /usr/bin/vcl

WORKDIR /src
CMD ["/bin/bash"]
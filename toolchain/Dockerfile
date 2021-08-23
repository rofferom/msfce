FROM debian:buster AS toolchain-builder

# Build toolchain
RUN apt-get update && \
    apt-get install -y \
        build-essential wget multistrap && \
    apt-get clean

COPY build_toolchain.sh /root
COPY fix_rootfs.sh /root
COPY raspi.conf /root

RUN /root/build_toolchain.sh

# Install host tools
FROM debian:buster

RUN apt-get update && \
    apt-get install -y \
        cmake pkg-config make

COPY --from=toolchain-builder /opt/raspi1-toolchain /opt/raspi1-toolchain
COPY environment-setup /opt/raspi1-toolchain

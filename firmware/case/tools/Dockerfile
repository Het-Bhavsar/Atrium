# Toolchain for the Atrium charging case firmware: GCC for rv32ec and ch32fun (MIT),
# pinned. Built by firmware/case/tools/case.sh image.
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
	gcc-riscv64-unknown-elf libnewlib-dev make gcc libc6-dev git ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

ARG CH32FUN_REV=b28d285460ce052a179dba8f4a22c6f73ff8e328
RUN git clone https://github.com/cnlohr/ch32fun.git /opt/ch32fun \
	&& git -C /opt/ch32fun checkout --quiet ${CH32FUN_REV}

ENV CH32FUN=/opt/ch32fun/ch32fun

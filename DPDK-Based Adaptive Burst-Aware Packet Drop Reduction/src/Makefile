# Makefile — DPDK Major Project
# Compiles: dpdk_baseline, dpdk_adaptive, dpdk_validator
# Usage: make all

CC      = gcc
CFLAGS  = $(shell pkg-config --cflags libdpdk)
LDFLAGS = $(shell pkg-config --libs libdpdk)

TARGETS = dpdk_baseline dpdk_adaptive dpdk_validator

all: $(TARGETS)

dpdk_baseline: dpdk_baseline.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)
	@echo "Built: dpdk_baseline"

dpdk_adaptive: dpdk_adaptive.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)
	@echo "Built: dpdk_adaptive"

dpdk_validator: dpdk_validator.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)
	@echo "Built: dpdk_validator"

clean:
	rm -f $(TARGETS)
	@echo "Cleaned all binaries"

.PHONY: all clean

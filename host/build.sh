#!/bin/bash
# Build the Host Simulation

gcc -g \
    host/main.c \
    host/ubpf_esp32_mock.c \
    host/scheduler.c \
    vm/ubpf_vm.c \
    vm/ubpf_loader.c \
    vm/ubpf_jit.c \
    vm/ubpf_jit_x86_64.c \
    vm/ubpf_jit_support.c \
    vm/ubpf_instruction_valid.c \
    -I vm/inc -I vm -I components/ubpf_esp32/include -I host \
    -DUBPF_HAS_ELF_H \
    -o host_runner -lm

echo "Build complete. Run with:"
echo "./host_runner examples/esp32_ubpf_demo/main/filters/init.o examples/esp32_ubpf_demo/main/filters/producer.o examples/esp32_ubpf_demo/main/filters/consumer.o"


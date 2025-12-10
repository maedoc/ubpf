# Host Simulation Environment

This directory contains a host-based simulation environment for testing uBPF programs designed for ESP32 targets. It provides a complete simulation of the ESP32 uBPF runtime environment on a desktop system.

## Overview

The host simulation allows you to:
- Test uBPF programs without ESP32 hardware
- Debug BPF programs with full system access
- Simulate concurrent BPF task execution
- Mock ESP32-specific APIs (NVS storage, delays, etc.)

## Components

### 1. `main.c` - Main Entry Point
The main application that:
- Loads three BPF ELF files (init, producer, consumer)
- Initializes the simulation environment
- Registers BPF programs
- Runs the init program
- Starts the scheduler for concurrent execution

**Usage:**
```bash
./host_runner <init.o> <producer.o> <consumer.o>
```

### 2. `scheduler.c` / `scheduler.h` - Cooperative Scheduler
A lightweight cooperative scheduler using `ucontext.h` that simulates ESP32 FreeRTOS-like task management:

- **`sim_task_create()`**: Creates simulated tasks (green threads)
- **`sim_task_delay()`**: Delays current task (yields control)
- **`sim_run()`**: Runs scheduler until all tasks complete
- **`sim_get_tick()`**: Gets current simulated time in milliseconds

Supports up to 10 concurrent tasks with 64KB stacks each.

### 3. `ubpf_esp32_mock.c` - ESP32 API Mock
Implements ESP32-specific uBPF helpers for host simulation:

#### Available Helpers:
- **`bpf_log(fmt, val)`**: Log messages to stdout
- **`bpf_delay_ms(ms)`**: Delay task execution (cooperative yield)
- **`bpf_nvs_set(key, val)`**: Store values in mock NVS (persists to `nvs_mock.txt`)
- **`bpf_nvs_get(key)`**: Retrieve values from mock NVS
- **`bpf_task_create(id)`**: Create new BPF task from registered program

#### Mock Features:
- **NVS Storage**: In-memory key-value store with file persistence
- **Program Registry**: Register BPF programs by ID for task creation
- **ELF Loading**: Uses uBPF's ELF loader with data relocation support

### 4. `build.sh` - Build Script
Compiles the host runner with all necessary dependencies:
- uBPF VM components
- Scheduler and mock implementations
- ELF loading support

## Building

```bash
# From project root
./host/build.sh
```

Or manually:
```bash
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
```

## Running the Example

```bash
./host_runner examples/esp32_ubpf_demo/main/filters/init.o \
              examples/esp32_ubpf_demo/main/filters/producer.o \
              examples/esp32_ubpf_demo/main/filters/consumer.o
```

## Example BPF Programs

The example demonstrates a producer-consumer pattern:

1. **`init.o`**: Initializes the system and creates producer/consumer tasks
2. **`producer.o`**: Periodically produces data and stores it in NVS
3. **`consumer.o`**: Periodically consumes data from NVS

## Architecture

```
┌─────────────────────────────────────────────┐
│                 Host Runner                 │
├─────────────────────────────────────────────┤
│  Scheduler  │  uBPF VM  │  ESP32 Mock API  │
├─────────────────────────────────────────────┤
│            ucontext.h (green threads)       │
└─────────────────────────────────────────────┘
```

## Key Features

- **Cooperative Multitasking**: Tasks yield control with `sim_task_delay()`
- **Time Simulation**: Simulated tick counter with real-time base
- **Persistent Storage**: NVS mock saves to `nvs_mock.txt` file
- **ELF Support**: Full ELF loading with data section relocation
- **Debug Output**: Comprehensive logging for BPF program execution

## Limitations

- Maximum 10 concurrent tasks
- Cooperative scheduling (no preemption)
- NVS storage limited to 16 key-value pairs
- 64KB stack per task (configurable)

## Integration with ESP32

The host simulation uses the same API as the ESP32 implementation (`ubpf_esp32.h`), allowing seamless porting between host testing and ESP32 deployment.
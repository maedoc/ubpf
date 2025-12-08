# uBPF Project Context

## Overview

**uBPF** (Userspace eBPF) is an Apache-licensed library for executing eBPF programs in userspace. It is designed to be portable and includes:
- An interpreter (all platforms).
- JIT compilers for x86-64 and Arm64.
- An eBPF assembler and disassembler.

The project is primarily C-based but includes Python tooling for testing and assembly.

## Key Directories

- **`vm/`**: Contains the core C implementation of the VM, JIT compilers (`ubpf_jit_*.c`), and the loader.
- **`tests/`**: Contains data-driven test cases (`.data` files). Each file describes an eBPF program (ASM or raw bytes) and the expected result/error.
- **`test_framework/`**: Python scripts used to run the tests. `test_vm.py` is the main runner that parses `.data` files and executes them against the `vm/test` binary.
- **`bpf/`**: Contains `.bpf.c` source files, likely for integration testing or examples.
- **`external/`**: Submodules for `bpf_conformance` and `ebpf-verifier`.

## Building and Running

### Prerequisites
- **CMake** (3.16+)
- **Clang/LLVM** (optional, for compiling C to eBPF)
- **Python 3** (for tests)
- **Git Submodules**: Ensure submodules are initialized:
  ```bash
  git submodule update --init --recursive
  ```

### Build Steps (Linux/macOS)

1.  **Configure**:
    ```bash
    cmake -S . -B build -DUBPF_ENABLE_TESTS=true
    ```
    *   On macOS with Homebrew LLVM: add `-DUBPF_ALTERNATE_LLVM_PATH=/opt/homebrew/opt/llvm/bin`

2.  **Build**:
    ```bash
    cmake --build build --config Debug
    ```

3.  **Dependencies (Linux)**:
    If needed, `scripts/build-libbpf.sh` can build and install `libbpf`.

### Running Tests

Tests are driven by CMake/CTest but utilize Python scripts to execute the VM.

1.  **Run all tests**:
    ```bash
    cmake --build build --target test
    # OR
    ctest --test-dir build
    ```

2.  **Test Mechanism**:
    The testing framework reads `.data` files from `tests/`. It assembles the eBPF code (if needed) and pipes it to the `vm/test` binary (built as `ubpf_test`), checking stdout/stderr for expected results.

## Development Conventions

- **Style**: The project uses `clang-format`. Ensure code is formatted before submitting.
- **Testing**: New features or bug fixes should include corresponding `.data` test cases in `tests/`.
- **Architecture**:
    - `ubpf_vm.c`: Main interpreter loop.
    - `ubpf_jit_*.c`: Architecture-specific JIT implementations.
    - `ubpf_loader.c`: ELF loading logic.

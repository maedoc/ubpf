#!/usr/bin/env python3
"""
ubpf_demo.py - Demo script showing how to use ubpf with Python ctypes

This script:
1. Compiles a BPF filter from C using clang
2. Loads it into ubpf via ctypes bindings
3. Executes the filter against sample packet data
"""

import ctypes
import subprocess
import sys
import os
from pathlib import Path

# Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
UBPF_ROOT = SCRIPT_DIR.parent
BUILD_DIR = UBPF_ROOT / "b"
LIB_DIR = BUILD_DIR / "lib"

DEMO_FILTER_C = SCRIPT_DIR / "demo_filter.c"
DEMO_FILTER_O = SCRIPT_DIR / "demo_filter.o"

# Try to find libubpf.so, fall back to libubpf.a
LIBUBPF_SO = LIB_DIR / "libubpf.so"
LIBUBPF_A = LIB_DIR / "libubpf.a"


def compile_bpf_program(source_file: Path, output_file: Path) -> bool:
    """Compile a C file to BPF bytecode using clang."""
    print(f"Compiling {source_file} -> {output_file}")

    # Compile with options to minimize relocations
    # -fno-common: place each global in its own section
    # -fdata-sections: put data in separate sections
    cmd = [
        "clang",
        "-O2",
        "-target", "bpf",
        "-fno-common",
        "-fdata-sections",
        "-c",
        str(source_file),
        "-o", str(output_file),
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Compilation failed:\n{result.stderr}")
            return False
        print("Compilation successful!")
        return True
    except FileNotFoundError:
        print("Error: clang not found. Please install clang.")
        return False


def build_shared_library():
    """Build libubpf.so if it doesn't exist."""
    if LIBUBPF_SO.exists():
        return True

    print("Building libubpf.so from static library...")

    # Find all object files or build from .a
    if not LIBUBPF_A.exists():
        print(f"Error: {LIBUBPF_A} not found. Please build ubpf first:")
        print("  cd ubpf && cmake -S . -B b && cmake --build b")
        return False

    # Extract objects from static library and create shared library
    vm_dir = BUILD_DIR / "vm" / "CMakeFiles" / "ubpf.dir"

    # Find all .o files in the build
    obj_files = list(vm_dir.glob("*.o")) if vm_dir.exists() else []

    if obj_files:
        cmd = ["gcc", "-shared", "-o", str(LIBUBPF_SO)] + [str(f) for f in obj_files]
    else:
        # Alternative: create wrapper that links statically
        # This is a workaround - ideally you'd build with -fPIC
        print("Creating shared library wrapper...")
        wrapper_c = SCRIPT_DIR / "_ubpf_wrapper.c"
        wrapper_c.write_text("""
// Wrapper to expose ubpf functions
#include "../vm/inc/ubpf.h"
""")
        cmd = [
            "gcc", "-shared", "-fPIC",
            "-o", str(LIBUBPF_SO),
            "-I", str(UBPF_ROOT / "vm" / "inc"),
            "-L", str(LIB_DIR),
            "-Wl,--whole-archive", str(LIBUBPF_A), "-Wl,--no-whole-archive",
        ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to create shared library:\n{result.stderr}")
            return False
        print("Created libubpf.so")
        return True
    except Exception as e:
        print(f"Error creating shared library: {e}")
        return False


class UbpfVM:
    """Python wrapper for ubpf VM using ctypes."""

    def __init__(self, lib_path: Path):
        self.lib = ctypes.CDLL(str(lib_path))
        self._setup_functions()
        self.vm = None
        self.global_data = None
        self.global_data_size = 0
        self.data_reloc_fn = None

    def _setup_functions(self):
        """Set up ctypes function signatures."""
        # struct ubpf_vm* ubpf_create(void)
        self.lib.ubpf_create.argtypes = []
        self.lib.ubpf_create.restype = ctypes.c_void_p

        # void ubpf_destroy(struct ubpf_vm* vm)
        self.lib.ubpf_destroy.argtypes = [ctypes.c_void_p]
        self.lib.ubpf_destroy.restype = None

        # int ubpf_load_elf(struct ubpf_vm* vm, const void* elf, size_t elf_len, char** errmsg)
        self.lib.ubpf_load_elf.argtypes = [
            ctypes.c_void_p,  # vm
            ctypes.c_void_p,  # elf data
            ctypes.c_size_t,  # elf_len
            ctypes.POINTER(ctypes.c_char_p),  # errmsg
        ]
        self.lib.ubpf_load_elf.restype = ctypes.c_int

        # int ubpf_register_data_relocation(...)
        self.lib.ubpf_register_data_relocation.argtypes = [
            ctypes.c_void_p,  # vm
            ctypes.c_void_p,  # user_context
            ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8),
                           ctypes.c_uint64, ctypes.c_char_p, ctypes.c_uint64, ctypes.c_uint64)
        ]
        self.lib.ubpf_register_data_relocation.restype = ctypes.c_int

        # int ubpf_exec(const struct ubpf_vm* vm, void* mem, size_t mem_len, uint64_t* bpf_return_value)
        self.lib.ubpf_exec.argtypes = [
            ctypes.c_void_p,  # vm
            ctypes.c_void_p,  # mem
            ctypes.c_size_t,  # mem_len
            ctypes.POINTER(ctypes.c_uint64),  # return value
        ]
        self.lib.ubpf_exec.restype = ctypes.c_int

    @staticmethod
    def _data_relocation_handler(user_context, data_ptr, data_size, symbol_name, symbol_offset, symbol_size):
        """Handle data relocations for global variables."""
        # user_context is actually a pointer to our UbpfVM instance
        vm = ctypes.cast(user_context, ctypes.POINTER(ctypes.c_void_p)).contents

        # For simplicity, we'll create a global data buffer
        # This is a simplified implementation - production code would need proper management
        vm_obj = ctypes.cast(vm, ctypes.py_object).value

        if vm_obj.global_data is None:
            vm_obj.global_data = (ctypes.c_uint8 * data_size)()
            vm_obj.global_data_size = data_size
            # Copy initial data from ELF
            for i in range(data_size):
                vm_obj.global_data[i] = data_ptr[i]

        # Return address of the symbol in global data
        symbol_addr = ctypes.cast(vm_obj.global_data, ctypes.c_uint64).value + symbol_offset
        return symbol_addr

    def create(self) -> bool:
        """Create a new ubpf VM instance."""
        self.vm = self.lib.ubpf_create()
        if not self.vm:
            print("Failed to create ubpf VM")
            return False
        return True

    def load_elf(self, elf_data: bytes) -> bool:
        """Load an ELF file into the VM."""
        # Create data relocation handler
        self.data_reloc_fn = ctypes.CFUNCTYPE(
            ctypes.c_uint64,
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_uint64,
            ctypes.c_char_p,
            ctypes.c_uint64,
            ctypes.c_uint64
        )(self._data_relocation_handler)

        # Register data relocation handler
        self.lib.ubpf_register_data_relocation(
            self.vm,
            ctypes.c_void_p(id(self)),  # Pass self's address as user_context
            self.data_reloc_fn
        )

        errmsg = ctypes.c_char_p()
        elf_buf = ctypes.create_string_buffer(elf_data)

        result = self.lib.ubpf_load_elf(
            self.vm,
            elf_buf,
            len(elf_data),
            ctypes.byref(errmsg)
        )

        if result < 0:
            err = errmsg.value.decode() if errmsg.value else "Unknown error"
            print(f"Failed to load ELF: {err}")
            return False
        return True

    def exec(self, memory: bytes) -> tuple[int, int]:
        """Execute the loaded BPF program.

        Returns: (status, return_value)
            status: 0 on success, negative on error
            return_value: the value returned by the BPF program
        """
        mem_buf = ctypes.create_string_buffer(memory)
        return_value = ctypes.c_uint64()

        status = self.lib.ubpf_exec(
            self.vm,
            mem_buf,
            len(memory),
            ctypes.byref(return_value)
        )

        return status, return_value.value

    def destroy(self):
        """Destroy the VM instance."""
        if self.vm:
            self.lib.ubpf_destroy(self.vm)
            self.vm = None

    def __enter__(self):
        self.create()
        return self

    def __exit__(self, *args):
        self.destroy()


def create_sample_packet(protocol: int, tos: int = 0) -> bytes:
    """Create a sample packet with the given IP protocol number and TOS value."""
    # Simplified packet structure matching demo_filter.c
    packet = bytearray(42)  # Minimum ethernet + IP header

    # Ethernet header (14 bytes)
    packet[0:6] = b'\xff\xff\xff\xff\xff\xff'  # dst mac
    packet[6:12] = b'\x00\x11\x22\x33\x44\x55'  # src mac
    packet[12:14] = b'\x08\x00'  # ethertype (IPv4)

    # IP header (20 bytes minimum)
    packet[14] = 0x45  # version (4) + IHL (5)
    packet[15] = tos  # TOS - now configurable
    packet[16:18] = b'\x00\x1c'  # total length
    packet[18:20] = b'\x00\x00'  # identification
    packet[20:22] = b'\x00\x00'  # flags + fragment
    packet[22] = 64  # TTL
    packet[23] = protocol  # Protocol
    packet[24:26] = b'\x00\x00'  # checksum
    packet[26:30] = bytes([192, 168, 1, 1])  # src IP
    packet[30:34] = bytes([192, 168, 1, 2])  # dst IP

    return bytes(packet)


def main():
    print("=" * 60)
    print("ubpf Python Demo with Global State & Moving Average")
    print("=" * 60)

    # Step 1: Compile the BPF filter
    if not compile_bpf_program(DEMO_FILTER_C, DEMO_FILTER_O):
        sys.exit(1)

    # Step 2: Build shared library if needed
    if not build_shared_library():
        sys.exit(1)

    # Step 3: Load and run the filter
    print("\nLoading ubpf library...")
    try:
        vm = UbpfVM(LIBUBPF_SO)
    except OSError as e:
        print(f"Failed to load library: {e}")
        sys.exit(1)

    print("Creating VM...")
    with vm:
        # Load the compiled ELF
        print(f"Loading ELF file: {DEMO_FILTER_O}")
        elf_data = DEMO_FILTER_O.read_bytes()
        if not vm.load_elf(elf_data):
            sys.exit(1)

        print("\n" + "=" * 60)
        print("Packet Stream Processing with Moving Average")
        print("=" * 60)
        print("\nDemonstrating adaptive filtering based on TOS field moving average")
        print("Creating a stream of TCP packets with varying TOS values...\n")

        # Create a stream of packets with varying TOS values
        # This simulates a real-world scenario where we track traffic patterns
        packet_stream = [
            (6, 32, "TCP"),   # TOS=32 (normal)
            (6, 64, "TCP"),   # TOS=64 (expedited forwarding)
            (6, 48, "TCP"),   # TOS=48 (assured forwarding)
            (6, 32, "TCP"),   # TOS=32 (back to normal)
            (6, 40, "TCP"),   # TOS=40 (AF class 3)
            (6, 128, "TCP"),  # TOS=128 (high priority)
            (6, 36, "TCP"),   # TOS=36 (close to average)
            (6, 200, "TCP"),  # TOS=200 (outlier - may be dropped)
            (6, 44, "TCP"),   # TOS=44 (close to average)
            (6, 240, "TCP"),  # TOS=240 (outlier - likely dropped)
            (6, 38, "TCP"),   # TOS=38 (close to average)
            (6, 30, "TCP"),   # TOS=30 (close to average)
        ]

        print(f"{'#':<4} {'Protocol':<10} {'TOS':<6} {'Action':<8} {'Reason'}")
        print("-" * 60)

        # Simulate packet stream processing
        for i, (protocol, tos, proto_name) in enumerate(packet_stream, 1):
            packet = create_sample_packet(protocol, tos)
            status, result = vm.exec(packet)

            if status < 0:
                action = "ERROR"
                reason = "Execution failed"
            elif result:
                action = "ACCEPT"
                if i < 5:
                    reason = "Protocol OK, TOS lenient (<5 packets)"
                else:
                    reason = f"Protocol OK, TOS near avg (Δ ≤ 64)"
            else:
                action = "DROP"
                reason = "TOS outlier or invalid protocol"

            print(f"{i:<4} {proto_name:<10} {tos:<6} {action:<8} {reason}")

        print("\n" + "=" * 60)
        print("Key Observations:")
        print("-" * 60)
        print("• First 5 packets: Filter is lenient with TOS check")
        print("• After 5 packets: Moving average stabilizes, filters outliers")
        print("• Packets 8 & 10 (TOS=200, 240): Likely dropped as outliers")
        print("• Other packets: Accepted based on adaptive TOS threshold")
        print("\nThis demonstrates how eBPF can maintain state across")
        print("packet processing and make adaptive filtering decisions!")
        print("=" * 60)

    print("\nDemo complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()

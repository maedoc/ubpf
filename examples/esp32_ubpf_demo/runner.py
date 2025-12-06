#!/usr/bin/env python3

import subprocess
import sys
import time
import os
import select
import signal
import uuid
import shutil
import tempfile

def run_demo(timeout=60):
    # Generate unique filenames
    unique_id = uuid.uuid4().hex
    temp_dir = tempfile.gettempdir()
    flash_bin_path = os.path.join(temp_dir, f"ubpf-demo-{unique_id}-flash.bin")
    efuse_bin_path = os.path.join(temp_dir, f"ubpf-demo-{unique_id}-efuse.bin")

    # Merge binaries
    # Offsets from build/flash_args
    esptool_cmd = [
        "esptool.py", "--chip=esp32c3", "merge_bin",
        f"--output={flash_bin_path}", "--fill-flash-size=2MB",
        "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "2MB",
        "0x0", "build/bootloader/bootloader.bin",
        "0x10000", "build/esp32_ubpf_demo.bin",
        "0x8000", "build/partition_table/partition-table.bin"
    ]

    print(f"[Runner] Generating flash image: {' '.join(esptool_cmd)}")
    try:
        # Run from current directory (examples/esp32_ubpf_demo)
        subprocess.run(esptool_cmd, check=True, capture_output=True)
    except subprocess.CalledProcessError as e:
        print(f"[Runner] Error generating flash image: {e}")
        if e.stderr:
            print(e.stderr.decode())
        return 1

    # Prepare efuse binary
    efuse_src = "build/qemu_efuse.bin"
    if os.path.exists(efuse_src):
        print(f"[Runner] Using existing efuse image: {efuse_src}")
        shutil.copyfile(efuse_src, efuse_bin_path)
    else:
        print("[Runner] Creating blank efuse image")
        with open(efuse_bin_path, "wb") as f:
            f.write(b"\xff" * 0x200)

    # Run QEMU
    qemu_cmd = [
        "qemu-system-riscv32", "-M", "esp32c3",
        f"-drive", f"file={flash_bin_path},if=mtd,format=raw",
        f"-drive", f"file={efuse_bin_path},if=none,format=raw,id=efuse",
        "-global", "driver=nvram.esp32c3.efuse,property=drive,value=efuse",
        "-global", "driver=timer.esp32c3.timg,property=wdt_disable,value=true",
        "-nic", "user,model=open_eth", "-nographic",
        "-serial", "mon:stdio"
    ]

    print(f"Starting QEMU: {' '.join(qemu_cmd)}")
    process = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
        bufsize=0,
        start_new_session=True
    )

    start_time = time.time()
    output_buffer = b""
    
    # Non-blocking setup
    os.set_blocking(process.stdout.fileno(), False)
    
    exit_code = 1 # Default fail
    success_seen = False
    
    try:
        while True:
            if time.time() - start_time > timeout:
                print(f"\nTIMEOUT ({timeout}s) reached!")
                break
            
            if process.poll() is not None:
                print("\nProcess exited prematurely.")
                break
            
            ready = select.select([process.stdout], [], [], 0.1)
            if ready[0]:
                chunk = os.read(process.stdout.fileno(), 1024)
                if not chunk:
                    break
                
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
                
                output_buffer += chunk
                full_text = output_buffer.decode('utf-8', errors='replace')
                
                if not success_seen and "BPF: Producer: Set counter" in full_text:
                    print("\n[Runner] 'Producer Task' detected via BPF Log.")
                    success_seen = True
                    # Don't exit immediately, let it run a bit to prove stability or just exit?
                    # The original script just ran for 25s.
                    # Let's exit success immediately for efficiency, or wait for Consumer.
                    exit_code = 0
                    break
                    
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        # Cleanup
        print("\n[Runner] Cleaning up...")
        try:
            os.killpg(os.getpgid(process.pid), signal.SIGTERM)
        except:
            pass
        
        # Wait briefly for process to die
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
            except:
                pass

        if os.path.exists(flash_bin_path):
            try:
                os.remove(flash_bin_path)
            except:
                pass
        if os.path.exists(efuse_bin_path):
            try:
                os.remove(efuse_bin_path)
            except:
                pass

    return exit_code

if __name__ == "__main__":
    sys.exit(run_demo())

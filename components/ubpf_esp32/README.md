# uBPF for ESP32

This component embeds the [uBPF](https://github.com/iovisor/ubpf) VM for ESP32, allowing execution of eBPF programs.

## Features
*   **JIT-less Execution**: Uses the uBPF interpreter (JIT not supported on Xtensa/RISC-V).
*   **System Helpers**:
    *   `log(fmt, val)`: Wraps `ESP_LOGI`.
    *   `delay_ms(ms)`: Wraps `vTaskDelay`.
    *   `nvs_get(key)` / `nvs_set(key, val)`: Access NVS storage.
*   **String Support**: Handles `.rodata` relocation automatically.

## Usage

1.  **Compile BPF**:
    Compile your C BPF programs with `clang -target bpf -fno-common -c filter.c -o filter.o`.
    Note: Use global char arrays for strings to ensure compatibility if strictly needed, though the component handles relocations now.

2.  **Embed**:
    Register the `.o` files in your component's `CMakeLists.txt` using `EMBED_FILES`.

3.  **Run**:
    ```c
    #include "ubpf_esp32.h"
    
    // ... access embedded symbols ...
    
    ubpf_esp32_init();
    struct ubpf_vm *vm = ubpf_esp32_create();
    uint64_t res = ubpf_esp32_run(vm, code_start, code_len, mem, mem_len);
    ubpf_esp32_destroy(vm);
    ```

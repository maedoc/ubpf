# Product Requirement Document: uBPF for ESP32 (Updated)

## 1. Overview
The goal is to enable the execution of eBPF programs on the ESP32 platform using the `ubpf` (Userspace eBPF) VM. This will allow dynamic, updateable logic to be deployed to ESP32 devices. To facilitate robust testing and development without hardware, a Host Simulation layer will be created using `ucontext` to accurately mimic the ESP32's FreeRTOS single-core, cooperative multitasking behavior on Linux.

## 2. Scope

### 2.1 ESP32 Component (`components/ubpf_esp32`)
*   **Embed uBPF VM**: Integrate `vm/` sources.
*   **System Helpers (BPF accessible)**:
    *   `bpf_log(fmt, arg)`: Wraps `ESP_LOGI`.
    *   `bpf_delay_ms(ms)`: Wraps `vTaskDelay`.
    *   `bpf_nvs_*`: Wraps NVS API.
*   **Execution**: BPF filters run within standard FreeRTOS tasks.

### 2.2 Native Host Implementation (`host/`)
*   **Simulation Engine (`scheduler.c`)**:
    *   Use `<ucontext.h>` to implement userspace green threads.
    *   **Single Process**: Runs entirely within one Linux process/thread.
    *   **Scheduler**: A round-robin or priority-based scheduler that respects task delays.
*   **Mock Layer (`ubpf_esp32_mock.c`)**:
    *   `bpf_delay_ms`: Calls `sim_task_delay()` to context switch.
    *   `bpf_log`: stdout.
    *   `bpf_nvs`: File-backed storage.

### 2.3 Example Application (`examples/esp32_ubpf_demo`)
*   **Producer/Consumer**:
    *   Two tasks running concurrent BPF filters.
    *   Exchange data via NVS.
    *   Synchronize via delays.

## 3. Architecture

### 3.1 Directory Structure
```
ubpf/
├── components/ubpf_esp32/
│   ├── include/ubpf_esp32.h
│   ├── src/ubpf_esp32.c
│   └── CMakeLists.txt
├── examples/esp32_ubpf_demo/
│   └── ...
├── host/
│   ├── scheduler.c     <-- New: ucontext scheduler
│   ├── scheduler.h     <-- New: Scheduler API
│   ├── ubpf_esp32_mock.c
│   ├── main.c          <-- Updated: Uses scheduler
│   └── build.sh
```

### 3.2 Host Scheduler API
*   `void sim_init(void)`: Setup main context.
*   `void sim_task_create(void (*func)(void*), void *arg)`: Create a green thread.
*   `void sim_task_delay(int ms)`: Yield execution for `ms` milliseconds.
*   `void sim_run(void)`: Start the scheduling loop.

## 4. Implementation Plan

1.  **Scheduler**: Implement `host/scheduler.c` using `ucontext` (getcontext, makecontext, swapcontext).
2.  **Integration**: Wire `bpf_delay_ms` in the mock layer to `sim_task_delay`.
3.  **Test Runner**: Update `host/main.c` to spawn two tasks (Producer/Consumer) and enter `sim_run()`.
4.  **Verify**: Ensure the log output shows interleaved execution (Producer -> Consumer -> Producer...).
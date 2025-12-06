#ifndef UBPF_ESP32_H
#define UBPF_ESP32_H

#ifndef __bpf__
#include <stddef.h>
#include <stdint.h>
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long size_t;
#define NULL ((void*)0)
#endif

// Helper ID definitions for BPF programs
#define UBPF_HELPER_LOG 1
#define UBPF_HELPER_DELAY_MS 2
#define UBPF_HELPER_NVS_SET 3
#define UBPF_HELPER_NVS_GET 4
#define UBPF_HELPER_TASK_CREATE 5

// Helper signatures for C BPF programs
#ifndef __bpf__
// Host/Device side API
struct ubpf_vm;

/**
 * @brief Initialize the UBPF ESP32 environment (NVS, etc)
 */
void ubpf_esp32_init(void);

/**
 * @brief Create a VM and register ESP32 helpers
 * @return struct ubpf_vm* 
 */
struct ubpf_vm *ubpf_esp32_create(void);

/**
 * @brief Register a BPF program with an ID for task creation
 * @param id Program ID (to be used by task_create helper)
 * @param code BPF bytecode
 * @param code_len Length of bytecode
 * @param name Name of the program (for logging/debugging)
 */
void ubpf_esp32_register_program(int id, const void *code, size_t code_len, const char *name);

/**
 * @brief Load and execute a BPF program once
 * @param vm VM instance
 * @param code BPF bytecode
 * @param code_len Length of bytecode
 * @param mem Memory to pass to BPF program (packet/context)
 * @param mem_len Length of memory
 * @return uint64_t BPF return value
 */
uint64_t ubpf_esp32_run(struct ubpf_vm *vm, const void *code, size_t code_len, void *mem, size_t mem_len);

/**
 * @brief Destroy VM
 */
void ubpf_esp32_destroy(struct ubpf_vm *vm);

#else
// BPF Side Definitions (for the .c files compiled to .o)

#define bpf_log(fmt, val) ((void (*)(const char *, int))UBPF_HELPER_LOG)(fmt, val)
#define bpf_delay_ms(ms) ((void (*)(int))UBPF_HELPER_DELAY_MS)(ms)
#define bpf_nvs_set(key, val) ((int (*)(const char *, int))UBPF_HELPER_NVS_SET)(key, val)
#define bpf_nvs_get(key) ((int (*)(const char *))UBPF_HELPER_NVS_GET)(key)
#define bpf_task_create(id) ((int (*)(int))UBPF_HELPER_TASK_CREATE)(id)

#endif

#endif // UBPF_ESP32_H

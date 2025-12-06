#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "ubpf.h"
#include "ubpf_esp32.h"
#include "scheduler.h"

// Mock NVS Storage (InMemory for simplicity, optionally saved to file)
#define MAX_NVS_KEYS 16
struct {
    char key[32];
    int value;
} nvs_store[MAX_NVS_KEYS];

static void nvs_load() {
    FILE *f = fopen("nvs_mock.txt", "r");
    if (!f) return;
    char key[32];
    int val;
    int i = 0;
    while(fscanf(f, "%31s %d", key, &val) == 2 && i < MAX_NVS_KEYS) {
        strncpy(nvs_store[i].key, key, 31);
        nvs_store[i].value = val;
        i++;
    }
    fclose(f);
}

static void nvs_save() {
    FILE *f = fopen("nvs_mock.txt", "w");
    if (!f) return;
    for(int i=0; i<MAX_NVS_KEYS; i++) {
        if (nvs_store[i].key[0] != 0) {
            fprintf(f, "%s %d\n", nvs_store[i].key, nvs_store[i].value);
        }
    }
    fclose(f);
}

// Program Registry
#define MAX_BPF_PROGRAMS 8
typedef struct {
    int id;
    const void *code;
    size_t len;
    const char *name;
} bpf_program_t;

static bpf_program_t programs[MAX_BPF_PROGRAMS];

// --- Helper Implementations ---

static uint64_t helper_log(uint64_t fmt_ptr, uint64_t val, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *fmt = (const char *)(uintptr_t)fmt_ptr;
    printf("[BPF-LOG] ");
    printf(fmt, (int)val);
    printf("\n");
    return 0;
}

static uint64_t helper_delay_ms(uint64_t ms, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    printf("[BPF-SYS] Task Delay %lu ms...\n", ms);
    sim_task_delay((int)ms);
    return 0;
}

static uint64_t helper_nvs_set(uint64_t key_ptr, uint64_t val, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *key = (const char *)(uintptr_t)key_ptr;
    // printf("[BPF-SYS] NVS Set %s = %lu\n", key, val);
    
    for(int i=0; i<MAX_NVS_KEYS; i++) {
        if (strcmp(nvs_store[i].key, key) == 0) {
            nvs_store[i].value = (int)val;
            nvs_save();
            return 0;
        }
    }
    // New key
    for(int i=0; i<MAX_NVS_KEYS; i++) {
        if (nvs_store[i].key[0] == 0) {
            strncpy(nvs_store[i].key, key, 31);
            nvs_store[i].value = (int)val;
            nvs_save();
            return 0;
        }
    }
    return -1; // Full
}

static uint64_t helper_nvs_get(uint64_t key_ptr, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *key = (const char *)(uintptr_t)key_ptr;
    for(int i=0; i<MAX_NVS_KEYS; i++) {
        if (strcmp(nvs_store[i].key, key) == 0) {
            return nvs_store[i].value;
        }
    }
    return 0; // Default
}

static void bpf_task_wrapper(void *pvParameters) {
    bpf_program_t *prog = (bpf_program_t *)pvParameters;
    printf("[BPF Task] Starting %s (ID %d)\n", prog->name, prog->id);
    
    // Host simulation loop
    while(1) {
        struct ubpf_vm *vm = ubpf_esp32_create();
        if (vm) {
            ubpf_esp32_run(vm, prog->code, prog->len, NULL, 0);
            ubpf_esp32_destroy(vm);
        }
        sim_task_delay(5000);
        
        if (sim_get_tick() > 60000) break; // Safety
    }
    printf("[BPF Task] Finished %s\n", prog->name);
}

static uint64_t helper_task_create(uint64_t id, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    for (int i = 0; i < MAX_BPF_PROGRAMS; i++) {
        if (programs[i].id == (int)id) {
            sim_task_create(bpf_task_wrapper, &programs[i]);
            return 0;
        }
    }
    printf("Program ID %d not found\n", (int)id);
    return -1;
}

// --- Public API ---

void ubpf_esp32_register_program(int id, const void *code, size_t code_len, const char *name) {
    for (int i = 0; i < MAX_BPF_PROGRAMS; i++) {
        if (programs[i].id == 0 || programs[i].id == id) {
            programs[i].id = id;
            programs[i].code = code;
            programs[i].len = code_len;
            programs[i].name = name;
            return;
        }
    }
}

static uint64_t relocation_handler(void *user_data, const uint8_t *data, uint64_t data_size, const char *symbol_name, uint64_t symbol_offset, uint64_t symbol_size) {
    void *mem = malloc(data_size);
    if (!mem) return 0;
    memcpy(mem, data, data_size);
    return (uint64_t)(uintptr_t)mem + symbol_offset;
}

void ubpf_esp32_init(void) {
    memset(nvs_store, 0, sizeof(nvs_store));
    nvs_load();
    memset(programs, 0, sizeof(programs));
}

struct ubpf_vm *ubpf_esp32_create(void) {
    struct ubpf_vm *vm = ubpf_create();
    if (!vm) return NULL;

    ubpf_register(vm, UBPF_HELPER_LOG, "log", helper_log);
    ubpf_register(vm, UBPF_HELPER_DELAY_MS, "delay_ms", helper_delay_ms);
    ubpf_register(vm, UBPF_HELPER_NVS_SET, "nvs_set", helper_nvs_set);
    ubpf_register(vm, UBPF_HELPER_NVS_GET, "nvs_get", helper_nvs_get);
    ubpf_register(vm, UBPF_HELPER_TASK_CREATE, "task_create", helper_task_create);
    
    ubpf_register_data_relocation(vm, NULL, relocation_handler);

    return vm;
}

uint64_t ubpf_esp32_run(struct ubpf_vm *vm, const void *code, size_t code_len, void *mem, size_t mem_len) {
    char *errmsg;
    if (ubpf_load_elf(vm, code, code_len, &errmsg) < 0) {
        fprintf(stderr, "Failed to load ELF: %s\n", errmsg);
        free(errmsg);
        return 0;
    }

    uint64_t ret;
    if (ubpf_exec(vm, mem, mem_len, &ret) < 0) {
        fprintf(stderr, "Failed to exec\n");
        return 0;
    }
    return ret;
}

void ubpf_esp32_destroy(struct ubpf_vm *vm) {
    ubpf_destroy(vm);
}
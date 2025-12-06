#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "ubpf.h"
#include "ubpf_esp32.h"

static const char *TAG = "ubpf_esp32";
static nvs_handle_t my_nvs_handle;

#define MAX_BPF_PROGRAMS 8
typedef struct {
    int id;
    const void *code;
    size_t len;
    const char *name;
} bpf_program_t;

static bpf_program_t programs[MAX_BPF_PROGRAMS];

// --- Helpers ---

static uint64_t helper_log(uint64_t fmt_ptr, uint64_t val, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *fmt = (const char *)(uintptr_t)fmt_ptr;
    printf("BPF: ");
    printf(fmt, (int)val);
    printf("\n");
    fflush(stdout);
    return 0;
}

static uint64_t helper_delay_ms(uint64_t ms, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    return 0;
}

static uint64_t helper_nvs_set(uint64_t key_ptr, uint64_t val, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *key = (const char *)(uintptr_t)key_ptr;
    esp_err_t err = nvs_set_i32(my_nvs_handle, key, (int32_t)val);
    if (err == ESP_OK) {
        nvs_commit(my_nvs_handle);
        return 0;
    }
    return -1;
}

static uint64_t helper_nvs_get(uint64_t key_ptr, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    const char *key = (const char *)(uintptr_t)key_ptr;
    int32_t val = 0;
    esp_err_t err = nvs_get_i32(my_nvs_handle, key, &val);
    if (err == ESP_OK) {
        return (uint64_t)val;
    }
    return 0; // Default
}

static void bpf_task_wrapper(void *pvParameters) {
    bpf_program_t *prog = (bpf_program_t *)pvParameters;
    ESP_LOGI(TAG, "Starting task for program %s (ID %d)", prog->name, prog->id);
    
    // Default loop behavior for demo
    while (1) {
        struct ubpf_vm *vm = ubpf_esp32_create();
        if (vm) {
            // We use NULL/0 for memory context for now
            ubpf_esp32_run(vm, prog->code, prog->len, NULL, 0);
            ubpf_esp32_destroy(vm);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static uint64_t helper_task_create(uint64_t id, uint64_t r2, uint64_t r3, uint64_t r4, uint64_t r5) {
    for (int i = 0; i < MAX_BPF_PROGRAMS; i++) {
        if (programs[i].id == (int)id) {
            xTaskCreate(bpf_task_wrapper, programs[i].name, 8192, &programs[i], 5, NULL);
            return 0;
        }
    }
    ESP_LOGE(TAG, "Program ID %d not found", (int)id);
    return -1;
}

static uint64_t relocation_handler(void *user_data, const uint8_t *data, uint64_t data_size, const char *symbol_name, uint64_t symbol_offset, uint64_t symbol_size) {
    void *mem = malloc(data_size);
    if (!mem) {
        ESP_LOGE(TAG, "Failed to allocate memory for relocation");
        return 0;
    }
    memcpy(mem, data, data_size);
    return (uint64_t)(uintptr_t)mem + symbol_offset;
}

// --- API ---

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
    ESP_LOGE(TAG, "Max BPF programs reached");
}

void ubpf_esp32_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    ESP_ERROR_CHECK(err);
    
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
        printf("[ubpf_esp32] Failed to load ELF: %s\n", errmsg);
        free(errmsg);
        return 0;
    }

    uint64_t ret;
    if (ubpf_exec(vm, mem, mem_len, &ret) < 0) {
        printf("[ubpf_esp32] Failed to exec\n");
        return 0;
    }
    return ret;
}

void ubpf_esp32_destroy(struct ubpf_vm *vm) {
    ubpf_destroy(vm);
}
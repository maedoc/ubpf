#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ubpf_esp32.h"
#include "ubpf.h"

extern const uint8_t producer_start[] asm("_binary_producer_o_start");
extern const uint8_t producer_end[]   asm("_binary_producer_o_end");

extern const uint8_t consumer_start[] asm("_binary_consumer_o_start");
extern const uint8_t consumer_end[]   asm("_binary_consumer_o_end");

extern const uint8_t init_start[] asm("_binary_init_o_start");
extern const uint8_t init_end[]   asm("_binary_init_o_end");

void app_main(void) {
    printf("BOOT: uBPF Demo Starting (Init Filter)...\n");
    ubpf_esp32_init();
    
    // Register Programs
    size_t prod_len = producer_end - producer_start;
    ubpf_esp32_register_program(1, producer_start, prod_len, "producer");

    size_t cons_len = consumer_end - consumer_start;
    ubpf_esp32_register_program(2, consumer_start, cons_len, "consumer");

    // Run Init Filter (which spawns tasks via task_create helper)
    struct ubpf_vm *vm = ubpf_esp32_create();
    if (vm) {
        size_t len = init_end - init_start;
        printf("DEBUG: Init BPF size: %zu bytes\n", len);
        
        uint64_t ret = ubpf_esp32_run(vm, init_start, len, NULL, 0);
        printf("DEBUG: Init BPF returned: %llu\n", (unsigned long long)ret);
        
        ubpf_esp32_destroy(vm);
    } else {
        printf("ERROR: Failed to create VM!\n");
    }
}
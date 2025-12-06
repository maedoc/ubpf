#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ubpf_esp32.h"
#include "scheduler.h"

// Helper to read file
static void *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *len = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *data = malloc(*len);
    fread(data, 1, *len, f);
    fclose(f);
    return data;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <init.o> <producer.o> <consumer.o>\n", argv[0]);
        return 1;
    }

    printf("Host Simulation Starting (Concurrent)...\n");
    
    // Init env
    ubpf_esp32_init();
    sim_init();

    // Load Code
    size_t init_len, prod_len, cons_len;
    void *init_code = read_file(argv[1], &init_len);
    void *prod_code = read_file(argv[2], &prod_len);
    void *cons_code = read_file(argv[3], &cons_len);

    if (!init_code || !prod_code || !cons_code) {
        fprintf(stderr, "Failed to read BPF files\n");
        return 1;
    }

    // Register Programs (matching IDs in init.c)
    ubpf_esp32_register_program(1, prod_code, prod_len, "Producer");
    ubpf_esp32_register_program(2, cons_code, cons_len, "Consumer");

    // Run Init
    struct ubpf_vm *vm = ubpf_esp32_create();
    if (vm) {
        printf("[Host] Running Init Filter...\n");
        ubpf_esp32_run(vm, init_code, init_len, NULL, 0);
        ubpf_esp32_destroy(vm);
    }

    // Run Scheduler
    sim_run();

    free(init_code);
    free(prod_code);
    free(cons_code);

    return 0;
}
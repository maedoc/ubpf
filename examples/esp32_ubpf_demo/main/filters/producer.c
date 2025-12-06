#include "ubpf_esp32.h"

char key_counter[] = "counter";
char fmt_log[] = "Producer: Set counter to %d\n";

// Producer: Increments "counter" in NVS
// Returns: New value
uint64_t entry(void *ctx, uint64_t ctx_len) {
    int counter = bpf_nvs_get(key_counter);
    counter++;
    bpf_nvs_set(key_counter, counter);
    bpf_log(fmt_log, counter);
    return counter;
}


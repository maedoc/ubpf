#include "ubpf_esp32.h"

char key_counter[] = "counter";
char fmt_log[] = "Consumer: Read counter = %d\n";

// Consumer: Reads "counter" from NVS
// Returns: Current value
uint64_t entry(void *ctx, uint64_t ctx_len) {
    int counter = bpf_nvs_get(key_counter);
    bpf_log(fmt_log, counter);
    return counter;
}


#include "ubpf_esp32.h"

int main() {
    // Avoid string literals to bypass relocation issues for now
    bpf_task_create(1);
    bpf_task_create(2);
    return 0;
}
#ifndef API_CONSISTENCY_TEST_H
#define API_CONSISTENCY_TEST_H

#include "ubpf_esp32.h"

// Static assertions to ensure API consistency between ESP32 and host
_Static_assert(UBPF_HELPER_LOG == 1, "UBPF_HELPER_LOG ID mismatch");
_Static_assert(UBPF_HELPER_DELAY_MS == 2, "UBPF_HELPER_DELAY_MS ID mismatch");
_Static_assert(UBPF_HELPER_NVS_SET == 3, "UBPF_HELPER_NVS_SET ID mismatch");
_Static_assert(UBPF_HELPER_NVS_GET == 4, "UBPF_HELPER_NVS_GET ID mismatch");
_Static_assert(UBPF_HELPER_TASK_CREATE == 5, "UBPF_HELPER_TASK_CREATE ID mismatch");

// Helper function signature verification
typedef uint64_t (*helper_func_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Verify helper signatures match expected pattern
_Static_assert(sizeof(helper_func_t) == sizeof(void*), "Helper function pointer size mismatch");

#endif // API_CONSISTENCY_TEST_H
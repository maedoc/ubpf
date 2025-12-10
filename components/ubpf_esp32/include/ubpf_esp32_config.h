#ifndef UBPF_ESP32_CONFIG_H
#define UBPF_ESP32_CONFIG_H

// Common configuration for both ESP32 and host implementations

#define MAX_BPF_PROGRAMS 8
#define MAX_NVS_KEYS 16
#define NVS_KEY_MAX_LEN 31

// Error codes
#define UBPF_ESP32_OK 0
#define UBPF_ESP32_ERR_NVS_FULL -1
#define UBPF_ESP32_ERR_NVS_NOT_FOUND -2
#define UBPF_ESP32_ERR_PROGRAM_NOT_FOUND -3
#define UBPF_ESP32_ERR_MAX_PROGRAMS -4

#endif // UBPF_ESP32_CONFIG_H
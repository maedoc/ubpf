// demo_filter.c - Example BPF filter for ubpf
// This filter demonstrates adaptive packet filtering with state tracking concepts
//
// CONCEPT: Real-world eBPF programs often maintain state across packets to:
// - Calculate moving averages, rates, or other metrics
// - Track connection state
// - Detect anomalies or outliers
// - Make adaptive filtering decisions
//
// In production eBPF, this state would be stored in BPF maps (e.g., PERCPU_ARRAY).
// This demo shows the filtering logic that would use such state.

#define STATIC static
#define BPF_MAP_LOOKUPelem(a, b) b  // Simplified - in real BPF: bpf_map_lookup_elem(&map, &key)

// Define types without libc (BPF target doesn't have libc)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Simple packet header structure (like a minimal ethernet + IP header)
struct packet_header {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
};

// NOTE: In a real eBPF program, you would define a BPF map like this:
//
// struct {
//     __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
//     __uint(max_entries, 1);
//     __type(key, uint32_t);
//     __type(value, uint64_t);
// } packet_stats SEC(".maps");
//
// And then update it with:
//   uint32_t key = 0;
//   uint64_t *count = bpf_map_lookup_elem(&packet_stats, &key);
//   if (count) { (*count)++; }
//
// For this demo, we use simple static variables to demonstrate the concept.
// These would be replaced with BPF map operations in production.

// Global state (in production: use BPF map or per-CPU variables)
// This simulates: packet count, sum of TOS values, and moving average
STATIC uint64_t packet_count = 0;
STATIC uint64_t sum_tos = 0;
STATIC uint32_t moving_avg_tos = 0;

// Entry point for the BPF program
// mem: pointer to packet data
// mem_len: length of packet data
// Returns: 0 = drop, 1 = accept
__attribute__((section(".text")))
int filter(void* mem, uint64_t mem_len) {
    // Ensure we have enough data for the header
    if (mem_len < sizeof(struct packet_header)) {
        return 0;  // Drop - packet too small
    }

    struct packet_header* pkt = (struct packet_header*)mem;

    // STATE TRACKING: Update counters on each packet
    // In production: use BPF map operations
    packet_count++;
    sum_tos += pkt->tos;

    // Calculate moving average of TOS field
    // Use integer division (no floating point in eBPF)
    // In production: add bounds checking to avoid division by zero
    moving_avg_tos = (uint32_t)(sum_tos / packet_count);

    // ADAPTIVE FILTERING: Base protocol check
    // Must accept TCP, UDP, or ICMP
    uint8_t accept_protocol = (pkt->protocol == 6 || pkt->protocol == 17 || pkt->protocol == 1);

    // ADAPTIVE FILTERING: TOS-based anomaly detection
    // After collecting enough samples (5 packets), use moving average
    // to filter out packets with TOS values that deviate significantly
    uint8_t tos_check = 0;
    if (packet_count >= 5) {
        // Calculate absolute difference from moving average
        uint32_t diff = (pkt->tos >= moving_avg_tos) ?
                        (pkt->tos - moving_avg_tos) :
                        (moving_avg_tos - pkt->tos);

        // Accept if TOS is within 64 units of average
        // This creates an adaptive filter that learns normal traffic patterns
        // and gradually filters out anomalous packets
        tos_check = (diff <= 64);
    } else {
        // For first few packets, be lenient (not enough data for accurate average)
        tos_check = (pkt->tos <= 192);
    }

    // Accept packet only if it passes both checks
    // This demonstrates stateful, adaptive packet filtering
    if (accept_protocol && tos_check) {
        return 1;  // Accept
    }

    return 0;  // Drop
}

// Helper functions for inspection (useful for monitoring/debugging)
// In production eBPF: expose via perf buffer, map lookup, or tracepoint
__attribute__((section("unwind"))) uint32_t get_moving_avg() {
    return moving_avg_tos;
}

__attribute__((section("unwind"))) uint64_t get_packet_count() {
    return packet_count;
}

__attribute__((section("unwind"))) uint64_t get_sum_tos() {
    return sum_tos;
}

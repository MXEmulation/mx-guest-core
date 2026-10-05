/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXGA_H
#define MXGA_H

#include <stddef.h>
#include <stdint.h>

#include "mx_versions.h"

#define MXGA_HEADER_BYTES 24u
#define MXGA_MAX_FRAME_BYTES (1024u * 1024u)
#define MXGA_STATS_HEADER_BYTES 72u
#define MXGA_STATS_CPU_BYTES 24u
#define MXGA_MAX_REPORTED_CPUS 1024u

#define MXGA_OP_HELLO 1u
#define MXGA_OP_HEARTBEAT 2u
#define MXGA_OP_COMMAND_RESULT 4u
#define MXGA_OP_SYSTEM_STATS 6u
#define MXGA_OP_INTEGRATION_STATUS 10u
#define MXGA_OP_SHUTDOWN 0x100u
#define MXGA_OP_RESTART 0x101u
#define MXGA_OP_INTEGRATION_WINDOW_ACTION 0x106u

#define MXGA_CAP_SHUTDOWN (1ull << 0)
#define MXGA_CAP_RESTART (1ull << 1)
#define MXGA_CAP_SYSTEM_STATS (1ull << 5)
#define MXGA_CAP_INTEGRATION (1ull << 8)

#define MXGA_INTEGRATION_STATUS_VERSION 4u
#define MXGA_INTEGRATION_STATUS_HEADER_BYTES 16u
#define MXGA_INTEGRATION_WINDOW_BYTES 52u
#define MXGA_INTEGRATION_SEGMENT_BYTES 36u
#define MXGA_INTEGRATION_MAX_WINDOWS 128u
#define MXGA_INTEGRATION_MAX_SEGMENTS 32u
#define MXGA_INTEGRATION_MAX_TEXT_BYTES 4096u
#define MXGA_INTEGRATION_MAX_ICON_DIMENSION 256u
#define MXGA_INTEGRATION_MXGPU_PRESENT (1u << 0)
#define MXGA_INTEGRATION_MXGPU_DRIVER_READY (1u << 1)
#define MXGA_INTEGRATION_DESKTOP_BRIDGE_READY (1u << 2)
#define MXGA_INTEGRATION_WINDOW_INVENTORY_READY (1u << 3)
#define MXGA_INTEGRATION_ICON_RESOLUTION_READY (1u << 4)
#define MXGA_INTEGRATION_REQUIRED_FLAGS                                                            \
    (MXGA_INTEGRATION_MXGPU_PRESENT | MXGA_INTEGRATION_MXGPU_DRIVER_READY |                        \
     MXGA_INTEGRATION_DESKTOP_BRIDGE_READY | MXGA_INTEGRATION_WINDOW_INVENTORY_READY |             \
     MXGA_INTEGRATION_ICON_RESOLUTION_READY)
#define MXGA_INTEGRATION_WINDOW_GEOMETRY_RELIABLE (1u << 0)
#define MXGA_INTEGRATION_ACTION_VERSION 1u
#define MXGA_INTEGRATION_ACTION_BYTES 20u
#define MXGA_INTEGRATION_ACTION_ACTIVATE 1u
#define MXGA_INTEGRATION_ACTION_CLOSE 2u

#define MXGA_COMMAND_STATUS_SUCCESS 0u

#define MXGA_CPU_FREQ_NONE 0u
#define MXGA_CPU_FREQ_CPUFREQ 1u
#define MXGA_CPU_FREQ_CPUINFO 2u
#define MXGA_CPU_FREQ_FIRMWARE 3u
#define MXGA_CPU_FREQ_PERFORMANCE_COUNTER 4u
#define MXGA_CPU_FREQ_CALIBRATION_LOOP 5u

enum mxga_status {
    MXGA_OK = 0,
    MXGA_ERR_LENGTH = 1,
    MXGA_ERR_MAGIC = 2,
    MXGA_ERR_VERSION = 3,
    MXGA_ERR_FLAGS = 4,
    MXGA_ERR_PAYLOAD = 5,
    MXGA_ERR_CAPACITY = 6,
    MXGA_ERR_UTF8 = 7,
    MXGA_ERR_GEOMETRY = 8
};

struct mxga_frame {
    uint16_t minor;
    uint16_t opcode;
    uint64_t sequence;
    const uint8_t *payload;
    uint32_t payload_len;
};

struct mxga_cpu_stat {
    uint32_t usage_permille;
    uint32_t current_khz;
    uint32_t minimum_khz;
    uint32_t maximum_khz;
    uint32_t frequency_source;
};

struct mxga_system_stats {
    uint64_t uptime_seconds;
    uint64_t memory_total_bytes;
    uint64_t memory_available_bytes;
    uint64_t memory_used_bytes;
    uint64_t swap_total_bytes;
    uint64_t swap_used_bytes;
    uint32_t load_average_permille[3];
    uint32_t process_count;
    uint32_t cpu_usage_permille;
    uint32_t cpu_count;
    const struct mxga_cpu_stat *cpus;
};

struct mxga_integration_segment {
    uint16_t scanout_id;
    int32_t x, y;
    uint32_t width, height;
    uint32_t destination_x, destination_y;
    uint32_t destination_width, destination_height;
};

struct mxga_integrated_window {
    uint64_t window_id;
    int32_t x, y;
    uint32_t width, height, flags;
    const uint8_t *title, *application_id, *icon_bgra;
    uint16_t title_bytes, application_id_bytes, icon_width, icon_height;
    uint32_t icon_bgra_bytes;
    uint16_t scanout_id, segment_count;
    uint32_t output_width, output_height;
    const struct mxga_integration_segment *segments;
};

struct mxga_integration_status {
    uint64_t generation;
    uint16_t flags;
    uint32_t window_count;
    const struct mxga_integrated_window *windows;
};

struct mxga_integration_action {
    uint16_t action;
    uint64_t generation, window_id;
};

int mxga_encode_frame(uint16_t minor, uint16_t opcode, uint64_t sequence, const uint8_t *payload,
                      uint32_t payload_len, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxga_decode_frame(const uint8_t *in, uint32_t len, struct mxga_frame *out);
int mxga_encode_capabilities(uint64_t capabilities, uint8_t *out, uint32_t cap, uint32_t *out_len);
int mxga_decode_capabilities(const uint8_t *payload, uint32_t len, uint64_t *capabilities);
int mxga_encode_command_result(uint16_t command_opcode, uint16_t status, uint8_t *out, uint32_t cap,
                               uint32_t *out_len);
int mxga_encode_system_stats(const struct mxga_system_stats *stats, uint8_t *out, uint32_t cap,
                             uint32_t *out_len);
int mxga_decode_system_stats(const uint8_t *payload, uint32_t len, struct mxga_system_stats *stats,
                             struct mxga_cpu_stat *cpus, uint32_t cpu_cap);
int mxga_encode_integration_status(const struct mxga_integration_status *status, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len);
/* Decoded text and icon views borrow payload; windows and segments are caller-owned. */
int mxga_decode_integration_status(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_status *status,
                                   struct mxga_integrated_window *windows, uint32_t window_cap,
                                   struct mxga_integration_segment *segments, uint32_t segment_cap);
int mxga_encode_integration_action(const struct mxga_integration_action *action, uint8_t *out,
                                   uint32_t cap, uint32_t *out_len);
int mxga_decode_integration_action(const uint8_t *payload, uint32_t len,
                                   struct mxga_integration_action *action);

#endif

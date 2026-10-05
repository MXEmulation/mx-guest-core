/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"

#include <stdio.h>
#include <string.h>

static int fail(const char *what)
{
    fprintf(stderr, "mxga test failed: %s\n", what);
    return 1;
}

int main(void)
{
    uint8_t caps[8];
    uint8_t frame[256];
    uint8_t stats_payload[72 + 24];
    uint8_t result[4];
    uint32_t caps_len = 0;
    uint32_t frame_len = 0;
    uint32_t stats_len = 0;
    uint32_t result_len = 0;
    uint64_t capabilities = 0;
    struct mxga_frame decoded;
    struct mxga_cpu_stat cpu;
    struct mxga_cpu_stat got_cpu;
    struct mxga_system_stats stats;
    struct mxga_system_stats got;
    uint64_t want = MXGA_CAP_SHUTDOWN | MXGA_CAP_RESTART | MXGA_CAP_SYSTEM_STATS;

    memset(&cpu, 0, sizeof cpu);
    cpu.usage_permille = 250;
    cpu.current_khz = 1500000;
    cpu.minimum_khz = 800000;
    cpu.maximum_khz = 2400000;
    cpu.frequency_source = MXGA_CPU_FREQ_CPUFREQ;
    memset(&stats, 0, sizeof stats);
    stats.uptime_seconds = 42;
    stats.memory_total_bytes = 8ull << 30;
    stats.memory_available_bytes = 3ull << 30;
    stats.memory_used_bytes = stats.memory_total_bytes - stats.memory_available_bytes;
    stats.swap_total_bytes = 1ull << 30;
    stats.swap_used_bytes = 4096;
    stats.load_average_permille[0] = 150;
    stats.load_average_permille[1] = 80;
    stats.load_average_permille[2] = 40;
    stats.process_count = 17;
    stats.cpu_usage_permille = 250;
    stats.cpu_count = 1;
    stats.cpus = &cpu;

    if (mxga_encode_capabilities(want, caps, sizeof caps, &caps_len) != MXGA_OK || caps_len != 8)
        return fail("capabilities");
    if (mxga_encode_frame(MXGA_PROTOCOL_MINOR, MXGA_OP_HELLO, 1, caps, caps_len, frame,
                          sizeof frame, &frame_len) != MXGA_OK)
        return fail("hello encode");
    if (mxga_decode_frame(frame, frame_len, &decoded) != MXGA_OK ||
        decoded.opcode != MXGA_OP_HELLO || decoded.sequence != 1)
        return fail("hello decode");
    if (mxga_decode_capabilities(decoded.payload, decoded.payload_len, &capabilities) != MXGA_OK ||
        capabilities != want)
        return fail("hello caps");
    if (mxga_encode_system_stats(&stats, stats_payload, sizeof stats_payload, &stats_len) !=
            MXGA_OK ||
        stats_len != sizeof stats_payload)
        return fail("stats encode");
    if (mxga_encode_frame(MXGA_PROTOCOL_MINOR, MXGA_OP_SYSTEM_STATS, 2, stats_payload, stats_len,
                          frame, sizeof frame, &frame_len) != MXGA_OK)
        return fail("stats frame");
    if (mxga_decode_frame(frame, frame_len, &decoded) != MXGA_OK ||
        decoded.opcode != MXGA_OP_SYSTEM_STATS)
        return fail("stats opcode");
    if (mxga_decode_system_stats(decoded.payload, decoded.payload_len, &got, &got_cpu, 1) !=
        MXGA_OK)
        return fail("stats decode");
    if (got.uptime_seconds != 42 || got.cpu_count != 1 || got_cpu.usage_permille != 250 ||
        got_cpu.frequency_source != MXGA_CPU_FREQ_CPUFREQ)
        return fail("stats fields");
    if (mxga_encode_frame(MXGA_PROTOCOL_MINOR, MXGA_OP_SHUTDOWN, 9, NULL, 0, frame, sizeof frame,
                          &frame_len) != MXGA_OK)
        return fail("shutdown encode");
    if (mxga_decode_frame(frame, frame_len, &decoded) != MXGA_OK ||
        decoded.opcode != MXGA_OP_SHUTDOWN || decoded.payload_len != 0)
        return fail("shutdown decode");
    if (mxga_encode_command_result(MXGA_OP_RESTART, MXGA_COMMAND_STATUS_SUCCESS, result,
                                   sizeof result, &result_len) != MXGA_OK)
        return fail("result");
    if (mxga_encode_frame(MXGA_PROTOCOL_MINOR, MXGA_OP_COMMAND_RESULT, 9, result, result_len, frame,
                          sizeof frame, &frame_len) != MXGA_OK)
        return fail("result frame");
    if (mxga_decode_frame(frame, frame_len, &decoded) != MXGA_OK ||
        decoded.opcode != MXGA_OP_COMMAND_RESULT)
        return fail("result decode");
    printf("mxga frames ok\n");
    return 0;
}

/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxio.h"
#include "mx_le.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void cap(uint8_t *config, uint32_t at, uint8_t next, uint8_t type,
                uint32_t offset, uint32_t length)
{
    config[at] = 9; config[at + 1] = next;
    config[at + 2] = type == MXIO_CAP_NOTIFY || type == MXIO_CAP_PCI_ACCESS ? 20 : 16;
    config[at + 3] = type;
    mx_w32(config, at + 8, offset); mx_w32(config, at + 12, length);
}

static void configuration(uint8_t config[256])
{
    memset(config, 0, 256);
    config[6] = 0x10; config[0x34] = 0x80;
    cap(config, 0x80, 0x50, MXIO_CAP_COMMON, 0, 64);
    cap(config, 0x50, 0x68, MXIO_CAP_NOTIFY, 0x3000, 8);
    mx_w32(config, 0x60, 4);
    cap(config, 0x68, 0xa0, MXIO_CAP_ISR, 0x1000, 4);
    cap(config, 0xa0, 0, MXIO_CAP_PCI_ACCESS, 0, 0);
}

int main(void)
{
    uint8_t config[256], bytes[16], previous[16];
    uint64_t bars[6] = {0x4000, 0, 0, 0, 0, 0};
    struct mxio_capabilities capabilities, sentinel;
    struct mxio_descriptor descriptor = {0x123456789abcdef0ull, 32, MXIO_DESCRIPTOR_NEXT, 63}, decoded;
    struct mxio_used_element used = {63, 1024}, decoded_used;
    struct mxio_ring_sizes sizes;
    uint16_t flags, index, head;
    configuration(config);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_OK);
    assert(!capabilities.has_device && capabilities.common.length == 64);
    assert(capabilities.notify.offset == 0x3000 && capabilities.notify_multiplier == 4);
    assert(capabilities.isr.offset == 0x1000);
    memset(&sentinel, 0xa5, sizeof(sentinel)); capabilities = sentinel;
    config[0xa1] = 0x80;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    assert(memcmp(&capabilities, &sentinel, sizeof(sentinel)) == 0);
    configuration(config); config[0x81] = 0x82;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    configuration(config); config[0x52] = 19;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    configuration(config); config[0x54] = 6;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_REGION);
    configuration(config); mx_w32(config, 0x5c, 0x1001);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_REGION);
    configuration(config); mx_w32(config, 0x60, 0);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_REGION);
    configuration(config); mx_w32(config, 0x58, 32);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_REGION);
    configuration(config); config[0x69] = 0xc0;
    cap(config, 0xc0, 0xa0, MXIO_CAP_DEVICE, 0x2000, 0);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_REGION);
    mx_w32(config, 0xcc, 8);
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_OK && capabilities.has_device);
    config[0xc3] = MXIO_CAP_COMMON;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    configuration(config); config[0x53] = 7;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    configuration(config); config[0xa1] = 0xfc; config[0xfc] = 9; config[0xfe] = 16;
    assert(mxio_capabilities_decode(config, 256, bars, &capabilities) == MXIO_ERR_CAPABILITY);
    assert(mxio_capabilities_decode(config, 255, bars, &capabilities) == MXIO_ERR_LENGTH);
    assert(mxio_ring_sizes(64, &sizes) == MXIO_OK);
    assert(sizes.descriptors == 1024 && sizes.available == 134 && sizes.used == 518);
    assert(mxio_ring_sizes(0, &sizes) == MXIO_ERR_ARGUMENT);
    assert(mxio_ring_sizes(3, &sizes) == MXIO_ERR_ARGUMENT);
    assert(mxio_ring_sizes(32768, &sizes) == MXIO_OK);
    assert(mxio_descriptor_encode(&descriptor, 64, bytes, sizeof(bytes)) == MXIO_OK);
    assert(bytes[0] == 0xf0 && bytes[7] == 0x12 && bytes[8] == 32 && bytes[14] == 63);
    assert(mxio_descriptor_decode(bytes, sizeof(bytes), 64, &decoded) == MXIO_OK);
    assert(decoded.address == descriptor.address && decoded.next == descriptor.next);
    memcpy(previous, bytes, sizeof(bytes)); descriptor.next = 64;
    assert(mxio_descriptor_encode(&descriptor, 64, bytes, sizeof(bytes)) == MXIO_ERR_INDEX);
    assert(memcmp(previous, bytes, sizeof(bytes)) == 0);
    descriptor.flags = MXIO_DESCRIPTOR_INDIRECT;
    assert(mxio_descriptor_encode(&descriptor, 64, bytes, sizeof(bytes)) == MXIO_ERR_FEATURE);
    descriptor.flags = 8;
    assert(mxio_descriptor_encode(&descriptor, 64, bytes, sizeof(bytes)) == MXIO_ERR_FLAGS);
    descriptor.flags = MXIO_DESCRIPTOR_WRITE; descriptor.address = UINT64_MAX;
    assert(mxio_descriptor_encode(&descriptor, 64, bytes, sizeof(bytes)) == MXIO_ERR_REGION);
    assert(mxio_descriptor_decode(bytes, 15, 64, &decoded) == MXIO_ERR_LENGTH);
    assert(mxio_used_encode(&used, 64, bytes, sizeof(bytes)) == MXIO_OK);
    assert(mxio_used_decode(bytes, 8, 64, &decoded_used) == MXIO_OK && decoded_used.length == used.length);
    mx_w32(bytes, 0, 64);
    assert(mxio_used_decode(bytes, 8, 64, &decoded_used) == MXIO_ERR_INDEX);
    assert(mxio_ring_header_encode(1, UINT16_MAX, bytes, sizeof(bytes)) == MXIO_OK);
    assert(mxio_ring_header_decode(bytes, 4, &flags, &index) == MXIO_OK && flags == 1 && index == UINT16_MAX);
    assert(mxio_ring_header_decode(bytes, 4, &flags, &flags) == MXIO_ERR_ARGUMENT);
    assert(mxio_ring_header_encode(2, 0, bytes, sizeof(bytes)) == MXIO_ERR_FLAGS);
    assert(mxio_available_encode(63, 64, bytes, sizeof(bytes)) == MXIO_OK);
    assert(mxio_available_decode(bytes, 2, 64, &head) == MXIO_OK && head == 63);
    assert(mxio_available_encode(64, 64, bytes, sizeof(bytes)) == MXIO_ERR_INDEX);
    puts("MXIO codec tests passed");
    return 0;
}

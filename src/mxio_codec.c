/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxio.h"
#include "mx_le.h"
#include <string.h>

static int queue_valid(uint16_t size)
{
    return size && !(size & (uint16_t)(size - 1));
}

static int regions_overlap(const struct mxio_region *a, const struct mxio_region *b)
{
    return a->bar == b->bar && (uint64_t)a->offset < (uint64_t)b->offset + b->length &&
           (uint64_t)b->offset < (uint64_t)a->offset + a->length;
}

int mxio_capabilities_decode(const uint8_t *config, uint32_t length,
                             const uint64_t bar_lengths[6], struct mxio_capabilities *out)
{
    struct mxio_capabilities result;
    uint8_t occupied[MXIO_PCI_CONFIG_BYTES] = {0};
    uint32_t offset, seen = 0;
    if (!config || !bar_lengths || !out) return MXIO_ERR_ARGUMENT;
    if (length != MXIO_PCI_CONFIG_BYTES) return MXIO_ERR_LENGTH;
    if ((config[0x0e] & 0x7fu) != 0 || !(mx_r16(config, 6) & 0x10u))
        return MXIO_ERR_CAPABILITY;
    memset(&result, 0, sizeof(result));
    offset = config[0x34];
    while (offset) {
        uint32_t extent = 2, i;
        uint8_t next;
        if (offset < 0x40u || offset > length - 2 || (offset & 3u) || occupied[offset])
            return MXIO_ERR_CAPABILITY;
        next = config[offset + 1];
        if (config[offset] == 0x09u) {
            uint8_t type;
            struct mxio_region region;
            if (offset > length - 4) return MXIO_ERR_CAPABILITY;
            extent = config[offset + 2]; type = config[offset + 3];
            if (extent < 4 || extent > length - offset) return MXIO_ERR_CAPABILITY;
            if (type >= MXIO_CAP_COMMON && type <= MXIO_CAP_PCI_ACCESS) {
                uint32_t bit = 1u << type;
                if ((seen & bit) || extent < 16 ||
                    ((type == MXIO_CAP_NOTIFY || type == MXIO_CAP_PCI_ACCESS) && extent < 20))
                    return MXIO_ERR_CAPABILITY;
                seen |= bit;
                region.bar = config[offset + 4];
                region.offset = mx_r32(config, offset + 8);
                region.length = mx_r32(config, offset + 12);
                if (region.bar >= 6) return MXIO_ERR_REGION;
                if (type != MXIO_CAP_PCI_ACCESS) {
                    if (!region.length || region.offset > bar_lengths[region.bar] ||
                        region.length > bar_lengths[region.bar] - region.offset)
                        return MXIO_ERR_REGION;
                    switch (type) {
                    case MXIO_CAP_COMMON:
                        if (region.length < MXIO_COMMON_BYTES || (region.offset & 3u))
                            return MXIO_ERR_REGION;
                        result.common = region; break;
                    case MXIO_CAP_NOTIFY:
                        result.notify_multiplier = mx_r32(config, offset + 16);
                        if (!result.notify_multiplier || (result.notify_multiplier & 1u) ||
                            (region.offset & 1u) || region.length < 2)
                            return MXIO_ERR_REGION;
                        result.notify = region; break;
                    case MXIO_CAP_ISR: result.isr = region; break;
                    case MXIO_CAP_DEVICE: result.device = region; result.has_device = 1; break;
                    default: break;
                    }
                }
            }
        } else if (config[offset] == 0x11u) {
            extent = 12;
            if (extent > length - offset) return MXIO_ERR_CAPABILITY;
        }
        for (i = 0; i < extent; ++i) {
            if (occupied[offset + i]) return MXIO_ERR_CAPABILITY;
            occupied[offset + i] = 1;
        }
        offset = next;
    }
    if ((seen & ((1u << MXIO_CAP_COMMON) | (1u << MXIO_CAP_NOTIFY) | (1u << MXIO_CAP_ISR))) !=
        ((1u << MXIO_CAP_COMMON) | (1u << MXIO_CAP_NOTIFY) | (1u << MXIO_CAP_ISR)))
        return MXIO_ERR_CAPABILITY;
    if (regions_overlap(&result.common, &result.notify) || regions_overlap(&result.common, &result.isr) ||
        regions_overlap(&result.notify, &result.isr) ||
        (result.has_device && (regions_overlap(&result.device, &result.common) ||
                               regions_overlap(&result.device, &result.notify) ||
                               regions_overlap(&result.device, &result.isr))))
        return MXIO_ERR_REGION;
    *out = result;
    return MXIO_OK;
}

int mxio_ring_sizes(uint16_t size, struct mxio_ring_sizes *out)
{
    if (!out || !queue_valid(size)) return MXIO_ERR_ARGUMENT;
    out->descriptors = (uint32_t)size * MXIO_DESCRIPTOR_BYTES;
    out->available = 6u + (uint32_t)size * 2u;
    out->used = 6u + (uint32_t)size * MXIO_USED_ELEMENT_BYTES;
    return MXIO_OK;
}

static int descriptor_valid(const struct mxio_descriptor *in, uint16_t size)
{
    if (!queue_valid(size)) return MXIO_ERR_ARGUMENT;
    if (in->flags & MXIO_DESCRIPTOR_INDIRECT) return MXIO_ERR_FEATURE;
    if (in->flags & ~(MXIO_DESCRIPTOR_NEXT | MXIO_DESCRIPTOR_WRITE)) return MXIO_ERR_FLAGS;
    if ((in->flags & MXIO_DESCRIPTOR_NEXT) && in->next >= size) return MXIO_ERR_INDEX;
    if (in->length && in->address > UINT64_MAX - in->length) return MXIO_ERR_REGION;
    return MXIO_OK;
}

int mxio_descriptor_encode(const struct mxio_descriptor *in, uint16_t size,
                           uint8_t *out, uint32_t capacity)
{
    struct mxio_descriptor value;
    int status;
    if (!in || !out) return MXIO_ERR_ARGUMENT;
    if (capacity < MXIO_DESCRIPTOR_BYTES) return MXIO_ERR_LENGTH;
    value = *in; status = descriptor_valid(&value, size);
    if (status != MXIO_OK) return status;
    mx_w64(out, 0, value.address); mx_w32(out, 8, value.length);
    mx_w16(out, 12, value.flags); mx_w16(out, 14, value.next);
    return MXIO_OK;
}

int mxio_descriptor_decode(const uint8_t *in, uint32_t length, uint16_t size,
                           struct mxio_descriptor *out)
{
    struct mxio_descriptor value;
    int status;
    if (!in || !out) return MXIO_ERR_ARGUMENT;
    if (length != MXIO_DESCRIPTOR_BYTES) return MXIO_ERR_LENGTH;
    value.address = mx_r64(in, 0); value.length = mx_r32(in, 8);
    value.flags = mx_r16(in, 12); value.next = mx_r16(in, 14);
    status = descriptor_valid(&value, size);
    if (status == MXIO_OK) *out = value;
    return status;
}

int mxio_used_encode(const struct mxio_used_element *in, uint16_t size,
                     uint8_t *out, uint32_t capacity)
{
    struct mxio_used_element value;
    if (!in || !out || !queue_valid(size)) return MXIO_ERR_ARGUMENT;
    if (capacity < MXIO_USED_ELEMENT_BYTES) return MXIO_ERR_LENGTH;
    value = *in;
    if (value.id >= size) return MXIO_ERR_INDEX;
    mx_w32(out, 0, value.id); mx_w32(out, 4, value.length);
    return MXIO_OK;
}

int mxio_used_decode(const uint8_t *in, uint32_t length, uint16_t size,
                     struct mxio_used_element *out)
{
    struct mxio_used_element value;
    if (!in || !out || !queue_valid(size)) return MXIO_ERR_ARGUMENT;
    if (length != MXIO_USED_ELEMENT_BYTES) return MXIO_ERR_LENGTH;
    value.id = mx_r32(in, 0); value.length = mx_r32(in, 4);
    if (value.id >= size) return MXIO_ERR_INDEX;
    *out = value;
    return MXIO_OK;
}

int mxio_ring_header_encode(uint16_t flags, uint16_t index, uint8_t *out, uint32_t capacity)
{
    if (!out) return MXIO_ERR_ARGUMENT;
    if (capacity < 4) return MXIO_ERR_LENGTH;
    if (flags & ~1u) return MXIO_ERR_FLAGS;
    mx_w16(out, 0, flags); mx_w16(out, 2, index);
    return MXIO_OK;
}

int mxio_ring_header_decode(const uint8_t *in, uint32_t length, uint16_t *flags, uint16_t *index)
{
    uint16_t f, i;
    if (!in || !flags || !index || flags == index) return MXIO_ERR_ARGUMENT;
    if (length != 4) return MXIO_ERR_LENGTH;
    f = mx_r16(in, 0); i = mx_r16(in, 2);
    if (f & ~1u) return MXIO_ERR_FLAGS;
    *flags = f; *index = i;
    return MXIO_OK;
}

int mxio_available_encode(uint16_t head, uint16_t size, uint8_t *out, uint32_t capacity)
{
    if (!out || !queue_valid(size)) return MXIO_ERR_ARGUMENT;
    if (capacity < 2) return MXIO_ERR_LENGTH;
    if (head >= size) return MXIO_ERR_INDEX;
    mx_w16(out, 0, head);
    return MXIO_OK;
}

int mxio_available_decode(const uint8_t *in, uint32_t length, uint16_t size, uint16_t *head)
{
    uint16_t value;
    if (!in || !head || !queue_valid(size)) return MXIO_ERR_ARGUMENT;
    if (length != 2) return MXIO_ERR_LENGTH;
    value = mx_r16(in, 0);
    if (value >= size) return MXIO_ERR_INDEX;
    *head = value;
    return MXIO_OK;
}

/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#ifndef MXIO_H
#define MXIO_H
#include <stdint.h>

#define MXIO_PCI_CONFIG_BYTES 256u
#define MXIO_CAP_COMMON 1u
#define MXIO_CAP_NOTIFY 2u
#define MXIO_CAP_ISR 3u
#define MXIO_CAP_DEVICE 4u
#define MXIO_CAP_PCI_ACCESS 5u
#define MXIO_COMMON_BYTES 64u
#define MXIO_REG_DEVICE_FEATURE_SELECT 0x00u
#define MXIO_REG_DEVICE_FEATURE 0x04u
#define MXIO_REG_DRIVER_FEATURE_SELECT 0x08u
#define MXIO_REG_DRIVER_FEATURE 0x0cu
#define MXIO_REG_CONFIG_VECTOR 0x10u
#define MXIO_REG_QUEUE_COUNT 0x12u
#define MXIO_REG_STATUS 0x14u
#define MXIO_REG_CONFIG_GENERATION 0x15u
#define MXIO_REG_QUEUE_SELECT 0x16u
#define MXIO_REG_QUEUE_SIZE 0x18u
#define MXIO_REG_QUEUE_VECTOR 0x1au
#define MXIO_REG_QUEUE_ENABLE 0x1cu
#define MXIO_REG_QUEUE_NOTIFY_OFFSET 0x1eu
#define MXIO_REG_QUEUE_DESCRIPTOR 0x20u
#define MXIO_REG_QUEUE_AVAILABLE 0x28u
#define MXIO_REG_QUEUE_USED 0x30u
#define MXIO_STATUS_ACKNOWLEDGE 1u
#define MXIO_STATUS_DRIVER 2u
#define MXIO_STATUS_DRIVER_OK 4u
#define MXIO_STATUS_FEATURES_OK 8u
#define MXIO_STATUS_NEEDS_RESET 0x40u
#define MXIO_STATUS_FAILED 0x80u
#define MXIO_FEATURE_INDIRECT (1ull << 28)
#define MXIO_FEATURE_EVENT_INDEX (1ull << 29)
#define MXIO_FEATURE_VERSION_1 (1ull << 32)
#define MXIO_NO_VECTOR 0xffffu
#define MXIO_AGENT_QUEUE_TO_HOST 0u
#define MXIO_AGENT_QUEUE_FROM_HOST 1u
#define MXIO_AGENT_QUEUE_COUNT 2u
#define MXIO_AGENT_QUEUE_MAX_SIZE 64u
#define MXIO_DESCRIPTOR_BYTES 16u
#define MXIO_USED_ELEMENT_BYTES 8u
#define MXIO_DESCRIPTOR_NEXT 1u
#define MXIO_DESCRIPTOR_WRITE 2u
#define MXIO_DESCRIPTOR_INDIRECT 4u
#define MXIO_AVAILABLE_NO_INTERRUPT 1u
#define MXIO_USED_NO_NOTIFY 1u

enum mxio_status {
    MXIO_OK, MXIO_ERR_ARGUMENT, MXIO_ERR_LENGTH, MXIO_ERR_CAPABILITY,
    MXIO_ERR_REGION, MXIO_ERR_FLAGS, MXIO_ERR_INDEX, MXIO_ERR_FEATURE
};
struct mxio_region { uint8_t bar; uint32_t offset, length; };
struct mxio_capabilities {
    struct mxio_region common, notify, isr, device;
    uint32_t notify_multiplier;
    uint8_t has_device;
};
struct mxio_descriptor { uint64_t address; uint32_t length; uint16_t flags, next; };
struct mxio_used_element { uint32_t id, length; };
struct mxio_ring_sizes { uint32_t descriptors, available, used; };

/* bar_lengths describe the assigned memory apertures; zero means unavailable. */
int mxio_capabilities_decode(const uint8_t *config, uint32_t length,
                             const uint64_t bar_lengths[6], struct mxio_capabilities *out);
int mxio_ring_sizes(uint16_t queue_size, struct mxio_ring_sizes *out);
/* Direct descriptors only; indirect chains require separate negotiated validation. */
int mxio_descriptor_encode(const struct mxio_descriptor *in, uint16_t queue_size,
                           uint8_t *out, uint32_t capacity);
int mxio_descriptor_decode(const uint8_t *in, uint32_t length, uint16_t queue_size,
                           struct mxio_descriptor *out);
int mxio_used_encode(const struct mxio_used_element *in, uint16_t queue_size,
                     uint8_t *out, uint32_t capacity);
int mxio_used_decode(const uint8_t *in, uint32_t length, uint16_t queue_size,
                     struct mxio_used_element *out);
int mxio_ring_header_encode(uint16_t flags, uint16_t index, uint8_t *out, uint32_t capacity);
int mxio_ring_header_decode(const uint8_t *in, uint32_t length, uint16_t *flags, uint16_t *index);
int mxio_available_encode(uint16_t head, uint16_t queue_size, uint8_t *out, uint32_t capacity);
int mxio_available_decode(const uint8_t *in, uint32_t length, uint16_t queue_size, uint16_t *head);
#endif

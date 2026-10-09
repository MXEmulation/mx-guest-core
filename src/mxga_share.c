/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"

#include "mx_le.h"

#include <string.h>

static int text_valid(const uint8_t *text, uint32_t bytes)
{
    uint32_t at = 0;
    if (bytes && !text)
        return MXGA_ERR_PAYLOAD;
    while (at < bytes) {
        uint32_t value = text[at++], remaining, minimum;
        if (value < 128u)
            continue;
        if (value >= 0xc2u && value <= 0xdfu) {
            value &= 0x1fu;
            remaining = 1;
            minimum = 0x80u;
        } else if (value >= 0xe0u && value <= 0xefu) {
            value &= 0x0fu;
            remaining = 2;
            minimum = 0x800u;
        } else if (value >= 0xf0u && value <= 0xf4u) {
            value &= 7u;
            remaining = 3;
            minimum = 0x10000u;
        } else
            return MXGA_ERR_UTF8;
        if (remaining > bytes - at)
            return MXGA_ERR_UTF8;
        while (remaining--) {
            uint32_t next = text[at++];
            if ((next & 0xc0u) != 0x80u)
                return MXGA_ERR_UTF8;
            value = (value << 6) | (next & 0x3fu);
        }
        if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
            return MXGA_ERR_UTF8;
    }
    return MXGA_OK;
}

static uint32_t text_length(const uint8_t *field, uint32_t width)
{
    uint32_t used = 0;
    while (used < width && field[used])
        used++;
    return used;
}

static int padding_clear(const uint8_t *field, uint32_t used, uint32_t width)
{
    while (used < width)
        if (field[used++])
            return 0;
    return 1;
}

static int fixed_text_valid(const uint8_t *text, uint32_t bytes, uint32_t width)
{
    if (bytes > width || (bytes && !text) || memchr(text ? text : (const uint8_t *)"", 0, bytes))
        return MXGA_ERR_PAYLOAD;
    return text_valid(text, bytes);
}

static void write_fixed(uint8_t *out, uint32_t offset, const uint8_t *text, uint32_t bytes,
                        uint32_t width)
{
    memset(out + offset, 0, width);
    if (bytes)
        memcpy(out + offset, text, bytes);
}

int mxga_encode_network_info(const struct mxga_network_interface *interfaces, uint32_t count,
                             uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint32_t total = MXGA_NETWORK_HEADER_BYTES, at, i, j;
    if (out_len)
        *out_len = 0;
    if (count > MXGA_NETWORK_MAX_INTERFACES || (count && !interfaces))
        return MXGA_ERR_PAYLOAD;
    for (i = 0; i < count; i++) {
        const struct mxga_network_interface *entry = &interfaces[i];
        int status;
        if ((entry->flags & ~MXGA_NETWORK_FLAGS_KNOWN) ||
            entry->address_count > MXGA_NETWORK_MAX_ADDRESSES ||
            (entry->address_count && !entry->addresses))
            return MXGA_ERR_PAYLOAD;
        status = text_valid(entry->name, text_length(entry->name, MXGA_NETWORK_NAME_BYTES));
        if (status != MXGA_OK)
            return status;
        for (j = 0; j < entry->address_count; j++) {
            const struct mxga_network_address *address = &entry->addresses[j];
            if (address->family == MXGA_NETWORK_FAMILY_IPV4 ? address->prefix_length > 32u
                : address->family == MXGA_NETWORK_FAMILY_IPV6 ? address->prefix_length > 128u
                                                              : 1)
                return MXGA_ERR_PAYLOAD;
        }
        total += MXGA_NETWORK_INTERFACE_BYTES + entry->address_count * MXGA_NETWORK_ADDRESS_BYTES;
    }
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    memset(out, 0, total);
    mx_w32(out, 0, count);
    at = MXGA_NETWORK_HEADER_BYTES;
    for (i = 0; i < count; i++) {
        const struct mxga_network_interface *entry = &interfaces[i];
        write_fixed(out, at, entry->name, text_length(entry->name, MXGA_NETWORK_NAME_BYTES),
                    MXGA_NETWORK_NAME_BYTES);
        memcpy(out + at + 16, entry->mac, MXGA_NETWORK_MAC_BYTES);
        mx_w16(out, at + 22, entry->flags);
        mx_w32(out, at + 24, entry->address_count);
        at += MXGA_NETWORK_INTERFACE_BYTES;
        for (j = 0; j < entry->address_count; j++) {
            const struct mxga_network_address *address = &entry->addresses[j];
            out[at] = address->family;
            out[at + 1] = address->prefix_length;
            memcpy(out + at + 4, address->address,
                   address->family == MXGA_NETWORK_FAMILY_IPV4 ? 4u : 16u);
            at += MXGA_NETWORK_ADDRESS_BYTES;
        }
    }
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_network_info(const uint8_t *payload, uint32_t len,
                             struct mxga_network_interface *interfaces, uint32_t interface_cap,
                             struct mxga_network_address *addresses, uint32_t address_cap,
                             uint32_t *count)
{
    uint32_t total, at = MXGA_NETWORK_HEADER_BYTES, used = 0, i, j;
    if (!payload || !count || len < MXGA_NETWORK_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    total = mx_r32(payload, 0);
    if (total > MXGA_NETWORK_MAX_INTERFACES || mx_r32(payload, 4))
        return MXGA_ERR_PAYLOAD;
    if (total && (!interfaces || interface_cap < total))
        return MXGA_ERR_CAPACITY;
    for (i = 0; i < total; i++) {
        struct mxga_network_interface *entry = &interfaces[i];
        uint32_t name_bytes, address_count;
        int status;
        if (len - at < MXGA_NETWORK_INTERFACE_BYTES)
            return MXGA_ERR_LENGTH;
        name_bytes = text_length(payload + at, MXGA_NETWORK_NAME_BYTES);
        if (!padding_clear(payload + at, name_bytes, MXGA_NETWORK_NAME_BYTES))
            return MXGA_ERR_PAYLOAD;
        status = text_valid(payload + at, name_bytes);
        if (status != MXGA_OK)
            return status;
        memcpy(entry->name, payload + at, MXGA_NETWORK_NAME_BYTES);
        memcpy(entry->mac, payload + at + 16, MXGA_NETWORK_MAC_BYTES);
        entry->flags = mx_r16(payload, at + 22);
        address_count = mx_r32(payload, at + 24);
        if ((entry->flags & ~MXGA_NETWORK_FLAGS_KNOWN) ||
            address_count > MXGA_NETWORK_MAX_ADDRESSES)
            return MXGA_ERR_PAYLOAD;
        if (address_count && (!addresses || address_cap - used < address_count))
            return MXGA_ERR_CAPACITY;
        at += MXGA_NETWORK_INTERFACE_BYTES;
        entry->address_count = address_count;
        entry->addresses = address_count ? addresses + used : NULL;
        for (j = 0; j < address_count; j++) {
            struct mxga_network_address *address = &addresses[used + j];
            uint8_t limit;
            if (len - at < MXGA_NETWORK_ADDRESS_BYTES)
                return MXGA_ERR_LENGTH;
            address->family = payload[at];
            address->prefix_length = payload[at + 1];
            limit = address->family == MXGA_NETWORK_FAMILY_IPV4   ? 32u
                    : address->family == MXGA_NETWORK_FAMILY_IPV6 ? 128u
                                                                  : 0u;
            if (!limit || address->prefix_length > limit || mx_r16(payload, at + 2) ||
                (address->family == MXGA_NETWORK_FAMILY_IPV4 &&
                 !padding_clear(payload + at + 8, 0, 12)))
                return MXGA_ERR_PAYLOAD;
            memcpy(address->address, payload + at + 4, 16);
            at += MXGA_NETWORK_ADDRESS_BYTES;
        }
        used += address_count;
    }
    if (at != len)
        return MXGA_ERR_LENGTH;
    *count = total;
    return MXGA_OK;
}

int mxga_encode_mount_share(const struct mxga_share_mount *mount, uint8_t *out, uint32_t cap,
                            uint32_t *out_len)
{
    int status;
    if (out_len)
        *out_len = 0;
    if (!mount || (mount->flags & ~MXGA_SHARE_FLAGS_KNOWN) || !mount->name_bytes)
        return MXGA_ERR_PAYLOAD;
    status = fixed_text_valid((const uint8_t *)mount->name, mount->name_bytes,
                              MXGA_SHARE_NAME_BYTES);
    if (status == MXGA_OK)
        status = fixed_text_valid((const uint8_t *)mount->mount_point, mount->mount_point_bytes,
                                  MXGA_SHARE_PATH_BYTES);
    if (status != MXGA_OK)
        return status;
    if (!out || cap < MXGA_SHARE_MOUNT_BYTES)
        return MXGA_ERR_LENGTH;
    mx_w32(out, 0, mount->share_id);
    mx_w32(out, 4, mount->flags);
    write_fixed(out, 8, (const uint8_t *)mount->name, mount->name_bytes, MXGA_SHARE_NAME_BYTES);
    write_fixed(out, 8 + MXGA_SHARE_NAME_BYTES, (const uint8_t *)mount->mount_point,
                mount->mount_point_bytes, MXGA_SHARE_PATH_BYTES);
    if (out_len)
        *out_len = MXGA_SHARE_MOUNT_BYTES;
    return MXGA_OK;
}

int mxga_decode_mount_share(const uint8_t *payload, uint32_t len, struct mxga_share_mount *mount)
{
    struct mxga_share_mount decoded;
    const uint8_t *name, *path;
    int status;
    if (!payload || !mount || len != MXGA_SHARE_MOUNT_BYTES)
        return MXGA_ERR_LENGTH;
    memset(&decoded, 0, sizeof decoded);
    decoded.share_id = mx_r32(payload, 0);
    decoded.flags = mx_r32(payload, 4);
    if (decoded.flags & ~MXGA_SHARE_FLAGS_KNOWN)
        return MXGA_ERR_FLAGS;
    name = payload + 8;
    path = payload + 8 + MXGA_SHARE_NAME_BYTES;
    decoded.name_bytes = text_length(name, MXGA_SHARE_NAME_BYTES);
    decoded.mount_point_bytes = text_length(path, MXGA_SHARE_PATH_BYTES);
    if (!decoded.name_bytes || !padding_clear(name, decoded.name_bytes, MXGA_SHARE_NAME_BYTES) ||
        !padding_clear(path, decoded.mount_point_bytes, MXGA_SHARE_PATH_BYTES))
        return MXGA_ERR_PAYLOAD;
    status = text_valid(name, decoded.name_bytes);
    if (status == MXGA_OK)
        status = text_valid(path, decoded.mount_point_bytes);
    if (status != MXGA_OK)
        return status;
    memcpy(decoded.name, name, decoded.name_bytes);
    memcpy(decoded.mount_point, path, decoded.mount_point_bytes);
    *mount = decoded;
    return MXGA_OK;
}

int mxga_encode_unmount_share(uint32_t share_id, uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;
    if (!out || cap < MXGA_SHARE_UNMOUNT_BYTES)
        return MXGA_ERR_LENGTH;
    mx_w32(out, 0, share_id);
    mx_w32(out, 4, 0);
    if (out_len)
        *out_len = MXGA_SHARE_UNMOUNT_BYTES;
    return MXGA_OK;
}

int mxga_decode_unmount_share(const uint8_t *payload, uint32_t len, uint32_t *share_id)
{
    if (!payload || !share_id || len != MXGA_SHARE_UNMOUNT_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r32(payload, 4))
        return MXGA_ERR_PAYLOAD;
    *share_id = mx_r32(payload, 0);
    return MXGA_OK;
}

static int share_status_valid(const struct mxga_share_status *share)
{
    int status;
    if (share->state > MXGA_SHARE_STATE_FAILED || (share->flags & ~MXGA_SHARE_FLAGS_KNOWN) ||
        !share->name_bytes ||
        (share->state == MXGA_SHARE_STATE_FAILED ? share->error >= 0 : share->error != 0))
        return MXGA_ERR_PAYLOAD;
    status = fixed_text_valid(share->name, share->name_bytes, MXGA_SHARE_NAME_BYTES);
    if (status != MXGA_OK)
        return status;
    return fixed_text_valid(share->mount_point, share->mount_point_bytes, MXGA_SHARE_PATH_BYTES);
}

int mxga_encode_share_status(const struct mxga_share_status *shares, uint32_t count, uint8_t *out,
                             uint32_t cap, uint32_t *out_len)
{
    uint32_t total, i;
    if (out_len)
        *out_len = 0;
    if (count > MXGA_SHARE_MAX || (count && !shares))
        return MXGA_ERR_PAYLOAD;
    for (i = 0; i < count; i++) {
        int status = share_status_valid(&shares[i]);
        if (status != MXGA_OK)
            return status;
    }
    total = MXGA_SHARE_STATUS_HEADER_BYTES + count * MXGA_SHARE_STATUS_RECORD_BYTES;
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    mx_w32(out, 0, count);
    mx_w32(out, 4, 0);
    for (i = 0; i < count; i++) {
        uint32_t base = MXGA_SHARE_STATUS_HEADER_BYTES + i * MXGA_SHARE_STATUS_RECORD_BYTES;
        mx_w32(out, base, shares[i].share_id);
        mx_w32(out, base + 4, shares[i].state);
        mx_w32(out, base + 8, (uint32_t)shares[i].error);
        mx_w32(out, base + 12, shares[i].flags);
        write_fixed(out, base + 16, shares[i].name, shares[i].name_bytes, MXGA_SHARE_NAME_BYTES);
        write_fixed(out, base + 16 + MXGA_SHARE_NAME_BYTES, shares[i].mount_point,
                    shares[i].mount_point_bytes, MXGA_SHARE_PATH_BYTES);
    }
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_share_status(const uint8_t *payload, uint32_t len,
                             struct mxga_share_status *shares, uint32_t share_cap, uint32_t *count)
{
    uint32_t total, i;
    if (!payload || !count || len < MXGA_SHARE_STATUS_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    total = mx_r32(payload, 0);
    if (total > MXGA_SHARE_MAX || mx_r32(payload, 4))
        return MXGA_ERR_PAYLOAD;
    if (len != MXGA_SHARE_STATUS_HEADER_BYTES + total * MXGA_SHARE_STATUS_RECORD_BYTES)
        return MXGA_ERR_LENGTH;
    if (total && (!shares || share_cap < total))
        return MXGA_ERR_CAPACITY;
    for (i = 0; i < total; i++) {
        const uint8_t *record =
            payload + MXGA_SHARE_STATUS_HEADER_BYTES + i * MXGA_SHARE_STATUS_RECORD_BYTES;
        struct mxga_share_status *share = &shares[i];
        int status;
        share->share_id = mx_r32(record, 0);
        share->state = mx_r32(record, 4);
        share->error = (int32_t)mx_r32(record, 8);
        share->flags = mx_r32(record, 12);
        share->name = record + 16;
        share->name_bytes = text_length(share->name, MXGA_SHARE_NAME_BYTES);
        share->mount_point = record + 16 + MXGA_SHARE_NAME_BYTES;
        share->mount_point_bytes = text_length(share->mount_point, MXGA_SHARE_PATH_BYTES);
        if (!padding_clear(share->name, share->name_bytes, MXGA_SHARE_NAME_BYTES) ||
            !padding_clear(share->mount_point, share->mount_point_bytes, MXGA_SHARE_PATH_BYTES))
            return MXGA_ERR_PAYLOAD;
        status = share_status_valid(share);
        if (status != MXGA_OK)
            return status;
    }
    *count = total;
    return MXGA_OK;
}

int mxga_fs_path_valid(const uint8_t *path, uint32_t bytes)
{
    uint32_t start = 0, i;
    int status;
    if (!bytes)
        return MXGA_OK;
    if (!path || bytes > MXGA_FS_MAX_PATH_BYTES || path[0] == '/')
        return MXGA_ERR_PAYLOAD;
    status = text_valid(path, bytes);
    if (status != MXGA_OK)
        return status;
    for (i = 0; i <= bytes; i++) {
        uint32_t size;
        if (i < bytes && path[i] != '/') {
            if (!path[i] || path[i] == '\\')
                return MXGA_ERR_PAYLOAD;
            continue;
        }
        size = i - start;
        if (!size || size > MXGA_FS_MAX_NAME_BYTES || (size == 1 && path[start] == '.') ||
            (size == 2 && path[start] == '.' && path[start + 1] == '.'))
            return MXGA_ERR_PAYLOAD;
        start = i + 1;
    }
    return MXGA_OK;
}

static int fs_handle_only(uint32_t operation)
{
    return operation == MXGA_FS_OP_READ || operation == MXGA_FS_OP_WRITE ||
           operation == MXGA_FS_OP_RELEASE || operation == MXGA_FS_OP_FSYNC ||
           operation == MXGA_FS_OP_TRUNCATE;
}

static int fs_root_allowed(uint32_t operation)
{
    return operation == MXGA_FS_OP_STATFS || operation == MXGA_FS_OP_GETATTR ||
           operation == MXGA_FS_OP_READDIR || operation == MXGA_FS_OP_SETATTR;
}

static int fs_request_valid(const struct mxga_fs_request *request)
{
    uint32_t op = request->operation;
    int open = op == MXGA_FS_OP_OPEN || op == MXGA_FS_OP_CREATE;
    uint32_t flag_mask = open ? MXGA_FS_OPEN_KNOWN : op == MXGA_FS_OP_SETATTR ? MXGA_FS_SETATTR_KNOWN : 0u;
    int moded = open || op == MXGA_FS_OP_MKDIR;
    int status;
    if (op < MXGA_FS_OP_STATFS || op > MXGA_FS_OP_READLINK)
        return MXGA_ERR_PAYLOAD;
    if (request->path_bytes > MXGA_FS_MAX_PATH_BYTES ||
        request->second_path_bytes > MXGA_FS_MAX_PATH_BYTES ||
        request->data_bytes > MXGA_FS_MAX_IO_BYTES)
        return MXGA_ERR_LENGTH;
    if ((request->path_bytes && !request->path) ||
        (request->second_path_bytes && !request->second_path) ||
        (request->data_bytes && !request->data))
        return MXGA_ERR_PAYLOAD;
    if (fs_handle_only(op) ? request->path_bytes != 0
                           : !request->path_bytes && !fs_root_allowed(op))
        return MXGA_ERR_PAYLOAD;
    if (op == MXGA_FS_OP_RENAME ? !request->second_path_bytes : request->second_path_bytes != 0)
        return MXGA_ERR_PAYLOAD;
    if (op == MXGA_FS_OP_SETATTR ? request->data_bytes != MXGA_FS_ATTRIBUTES_BYTES
                                 : op != MXGA_FS_OP_WRITE && request->data_bytes)
        return MXGA_ERR_PAYLOAD;
    if ((request->flags & ~flag_mask) || (moded ? request->mode > 07777u : request->mode != 0) ||
        (op == MXGA_FS_OP_READ && request->length > MXGA_FS_MAX_IO_BYTES))
        return MXGA_ERR_PAYLOAD;
    status = mxga_fs_path_valid(request->path, request->path_bytes);
    if (status == MXGA_OK)
        status = mxga_fs_path_valid(request->second_path, request->second_path_bytes);
    return status;
}

int mxga_encode_fs_request(const struct mxga_fs_request *request, uint8_t *out, uint32_t cap,
                           uint32_t *out_len)
{
    uint32_t total, at = MXGA_FS_REQUEST_HEADER_BYTES;
    int status;
    if (out_len)
        *out_len = 0;
    if (!request)
        return MXGA_ERR_PAYLOAD;
    status = fs_request_valid(request);
    if (status != MXGA_OK)
        return status;
    total = MXGA_FS_REQUEST_HEADER_BYTES + request->path_bytes + request->second_path_bytes +
            request->data_bytes;
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    mx_w64(out, 0, request->request_id);
    mx_w64(out, 8, request->handle);
    mx_w64(out, 16, request->offset);
    mx_w32(out, 24, request->share_id);
    mx_w32(out, 28, request->operation);
    mx_w32(out, 32, request->length);
    mx_w32(out, 36, request->flags);
    mx_w32(out, 40, request->mode);
    mx_w32(out, 44, request->path_bytes);
    mx_w32(out, 48, request->second_path_bytes);
    mx_w32(out, 52, request->data_bytes);
    if (request->path_bytes)
        memmove(out + at, request->path, request->path_bytes);
    at += request->path_bytes;
    if (request->second_path_bytes)
        memmove(out + at, request->second_path, request->second_path_bytes);
    at += request->second_path_bytes;
    if (request->data_bytes)
        memmove(out + at, request->data, request->data_bytes);
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_fs_request(const uint8_t *payload, uint32_t len, struct mxga_fs_request *request)
{
    struct mxga_fs_request decoded;
    uint64_t total;
    if (!payload || !request || len < MXGA_FS_REQUEST_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    memset(&decoded, 0, sizeof decoded);
    decoded.request_id = mx_r64(payload, 0);
    decoded.handle = mx_r64(payload, 8);
    decoded.offset = mx_r64(payload, 16);
    decoded.share_id = mx_r32(payload, 24);
    decoded.operation = mx_r32(payload, 28);
    decoded.length = mx_r32(payload, 32);
    decoded.flags = mx_r32(payload, 36);
    decoded.mode = mx_r32(payload, 40);
    decoded.path_bytes = mx_r32(payload, 44);
    decoded.second_path_bytes = mx_r32(payload, 48);
    decoded.data_bytes = mx_r32(payload, 52);
    if (decoded.path_bytes > MXGA_FS_MAX_PATH_BYTES ||
        decoded.second_path_bytes > MXGA_FS_MAX_PATH_BYTES ||
        decoded.data_bytes > MXGA_FS_MAX_IO_BYTES)
        return MXGA_ERR_LENGTH;
    total = (uint64_t)MXGA_FS_REQUEST_HEADER_BYTES + decoded.path_bytes +
            decoded.second_path_bytes + decoded.data_bytes;
    if (total != len)
        return MXGA_ERR_LENGTH;
    decoded.path = decoded.path_bytes ? payload + MXGA_FS_REQUEST_HEADER_BYTES : NULL;
    decoded.second_path = decoded.second_path_bytes
                              ? payload + MXGA_FS_REQUEST_HEADER_BYTES + decoded.path_bytes
                              : NULL;
    decoded.data = decoded.data_bytes ? payload + len - decoded.data_bytes : NULL;
    *request = decoded;
    return MXGA_OK;
}

int mxga_encode_fs_response(const struct mxga_fs_response *response, uint8_t *out, uint32_t cap,
                            uint32_t *out_len)
{
    uint32_t total;
    if (out_len)
        *out_len = 0;
    if (!response || response->status > 0 ||
        response->data_bytes > MXGA_FS_MAX_RESPONSE_DATA_BYTES ||
        (response->data_bytes && !response->data))
        return MXGA_ERR_PAYLOAD;
    total = MXGA_FS_RESPONSE_HEADER_BYTES + response->data_bytes;
    if (!out || cap < total)
        return MXGA_ERR_LENGTH;
    if (response->data_bytes)
        memmove(out + MXGA_FS_RESPONSE_HEADER_BYTES, response->data, response->data_bytes);
    mx_w64(out, 0, response->request_id);
    mx_w64(out, 8, response->handle);
    mx_w32(out, 16, (uint32_t)response->status);
    mx_w32(out, 20, response->operation);
    mx_w32(out, 24, response->data_bytes);
    mx_w32(out, 28, 0);
    if (out_len)
        *out_len = total;
    return MXGA_OK;
}

int mxga_decode_fs_response(const uint8_t *payload, uint32_t len,
                            struct mxga_fs_response *response)
{
    struct mxga_fs_response decoded;
    if (!payload || !response || len < MXGA_FS_RESPONSE_HEADER_BYTES ||
        len - MXGA_FS_RESPONSE_HEADER_BYTES > MXGA_FS_MAX_RESPONSE_DATA_BYTES)
        return MXGA_ERR_LENGTH;
    decoded.request_id = mx_r64(payload, 0);
    decoded.handle = mx_r64(payload, 8);
    decoded.status = (int32_t)mx_r32(payload, 16);
    decoded.operation = mx_r32(payload, 20);
    decoded.data_bytes = mx_r32(payload, 24);
    if (decoded.data_bytes != len - MXGA_FS_RESPONSE_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r32(payload, 28) || decoded.status > 0)
        return MXGA_ERR_PAYLOAD;
    decoded.data = decoded.data_bytes ? payload + MXGA_FS_RESPONSE_HEADER_BYTES : NULL;
    *response = decoded;
    return MXGA_OK;
}

int mxga_encode_fs_attributes(const struct mxga_fs_attributes *attributes, uint8_t *out,
                              uint32_t cap, uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;
    if (!attributes || attributes->atime_ns >= 1000000000u ||
        attributes->mtime_ns >= 1000000000u || attributes->ctime_ns >= 1000000000u)
        return MXGA_ERR_PAYLOAD;
    if (!out || cap < MXGA_FS_ATTRIBUTES_BYTES)
        return MXGA_ERR_LENGTH;
    mx_w64(out, 0, attributes->ino);
    mx_w64(out, 8, attributes->size);
    mx_w64(out, 16, attributes->blocks);
    mx_w64(out, 24, attributes->atime_s);
    mx_w64(out, 32, attributes->mtime_s);
    mx_w64(out, 40, attributes->ctime_s);
    mx_w32(out, 48, attributes->atime_ns);
    mx_w32(out, 52, attributes->mtime_ns);
    mx_w32(out, 56, attributes->ctime_ns);
    mx_w32(out, 60, attributes->mode);
    mx_w32(out, 64, attributes->uid);
    mx_w32(out, 68, attributes->gid);
    mx_w32(out, 72, attributes->nlink);
    mx_w32(out, 76, 0);
    if (out_len)
        *out_len = MXGA_FS_ATTRIBUTES_BYTES;
    return MXGA_OK;
}

int mxga_decode_fs_attributes(const uint8_t *data, uint32_t len,
                              struct mxga_fs_attributes *attributes)
{
    if (!data || !attributes || len != MXGA_FS_ATTRIBUTES_BYTES)
        return MXGA_ERR_LENGTH;
    if (mx_r32(data, 76))
        return MXGA_ERR_PAYLOAD;
    attributes->ino = mx_r64(data, 0);
    attributes->size = mx_r64(data, 8);
    attributes->blocks = mx_r64(data, 16);
    attributes->atime_s = mx_r64(data, 24);
    attributes->mtime_s = mx_r64(data, 32);
    attributes->ctime_s = mx_r64(data, 40);
    attributes->atime_ns = mx_r32(data, 48);
    attributes->mtime_ns = mx_r32(data, 52);
    attributes->ctime_ns = mx_r32(data, 56);
    attributes->mode = mx_r32(data, 60);
    attributes->uid = mx_r32(data, 64);
    attributes->gid = mx_r32(data, 68);
    attributes->nlink = mx_r32(data, 72);
    return MXGA_OK;
}

int mxga_encode_fs_statfs(const struct mxga_fs_statfs *statfs, uint8_t *out, uint32_t cap,
                          uint32_t *out_len)
{
    if (out_len)
        *out_len = 0;
    if (!statfs)
        return MXGA_ERR_PAYLOAD;
    if (!out || cap < MXGA_FS_STATFS_BYTES)
        return MXGA_ERR_LENGTH;
    mx_w64(out, 0, statfs->block_size);
    mx_w64(out, 8, statfs->blocks);
    mx_w64(out, 16, statfs->blocks_free);
    mx_w64(out, 24, statfs->blocks_available);
    mx_w64(out, 32, statfs->files);
    mx_w64(out, 40, statfs->files_free);
    mx_w64(out, 48, statfs->name_max);
    if (out_len)
        *out_len = MXGA_FS_STATFS_BYTES;
    return MXGA_OK;
}

int mxga_decode_fs_statfs(const uint8_t *data, uint32_t len, struct mxga_fs_statfs *statfs)
{
    if (!data || !statfs || len != MXGA_FS_STATFS_BYTES)
        return MXGA_ERR_LENGTH;
    statfs->block_size = mx_r64(data, 0);
    statfs->blocks = mx_r64(data, 8);
    statfs->blocks_free = mx_r64(data, 16);
    statfs->blocks_available = mx_r64(data, 24);
    statfs->files = mx_r64(data, 32);
    statfs->files_free = mx_r64(data, 40);
    statfs->name_max = mx_r64(data, 48);
    return MXGA_OK;
}

static int dirent_name_valid(const uint8_t *name, uint32_t bytes)
{
    if (!name || !bytes || bytes > MXGA_FS_MAX_NAME_BYTES || memchr(name, '/', bytes) ||
        memchr(name, 0, bytes) || (bytes == 1 && name[0] == '.') ||
        (bytes == 2 && name[0] == '.' && name[1] == '.'))
        return MXGA_ERR_PAYLOAD;
    return text_valid(name, bytes);
}

int mxga_encode_fs_dirent(const struct mxga_fs_dirent *entry, uint8_t *out, uint32_t cap,
                          uint32_t *out_len)
{
    int status;
    if (out_len)
        *out_len = 0;
    if (!entry)
        return MXGA_ERR_PAYLOAD;
    status = dirent_name_valid(entry->name, entry->name_bytes);
    if (status != MXGA_OK)
        return status;
    if (!out || cap < MXGA_FS_DIRENT_HEADER_BYTES + entry->name_bytes)
        return MXGA_ERR_LENGTH;
    mx_w64(out, 0, entry->inode);
    mx_w32(out, 8, entry->mode);
    mx_w32(out, 12, entry->name_bytes);
    memcpy(out + MXGA_FS_DIRENT_HEADER_BYTES, entry->name, entry->name_bytes);
    if (out_len)
        *out_len = MXGA_FS_DIRENT_HEADER_BYTES + entry->name_bytes;
    return MXGA_OK;
}

int mxga_decode_fs_dirent(const uint8_t *data, uint32_t len, uint32_t *cursor,
                          struct mxga_fs_dirent *entry)
{
    uint32_t at, name_bytes;
    int status;
    if (!data || !cursor || !entry || *cursor > len || len - *cursor < MXGA_FS_DIRENT_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    at = *cursor;
    name_bytes = mx_r32(data, at + 12);
    if (name_bytes > len - at - MXGA_FS_DIRENT_HEADER_BYTES)
        return MXGA_ERR_LENGTH;
    status = dirent_name_valid(data + at + MXGA_FS_DIRENT_HEADER_BYTES, name_bytes);
    if (status != MXGA_OK)
        return status;
    entry->inode = mx_r64(data, at);
    entry->mode = mx_r32(data, at + 8);
    entry->name = data + at + MXGA_FS_DIRENT_HEADER_BYTES;
    entry->name_bytes = name_bytes;
    *cursor = at + MXGA_FS_DIRENT_HEADER_BYTES + name_bytes;
    return MXGA_OK;
}

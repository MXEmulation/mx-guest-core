/* SPDX-License-Identifier: MIT */
/* SPDX-FileCopyrightText: 2026 Zak Noble-Clarke */
#include "mxga.h"
#include <stdio.h>
#include <string.h>

static uint8_t large[MXGA_MAX_FRAME_BYTES];
static int check(int success, const char *name)
{
    if (!success) fprintf(stderr, "clipboard test failed: %s\n", name);
    return success;
}
int main(void)
{
    uint8_t encoded[64];
    uint32_t bytes = 0;
    const uint8_t text[] = {'a',0,0xc2,0xa2,0xe2,0x82,0xac,0xf0,0x9d,0x84,0x9e};
    const uint8_t bad[][4] = {{0x80,0,0,0},{0xc0,0x80,0,0},{0xe0,0x80,0x80,0},
        {0xed,0xa0,0x80,0},{0xf4,0x90,0x80,0x80},{0xf5,0x80,0x80,0x80},{0xe2,0x82,0,0}};
    const uint32_t lengths[] = {1,2,3,3,4,4,2};
    struct mxga_clipboard got, unchanged;
    unsigned i;
    if (!check(mxga_encode_clipboard(UINT64_C(0x0807060504030201),UINT64_MAX,text,sizeof(text),encoded,sizeof(encoded),&bytes)==MXGA_OK,
        "encode") || !check(bytes==16+sizeof(text) && encoded[0]==1 && encoded[7]==8 && encoded[8]==255,"wire") ||
        !check(mxga_decode_clipboard(encoded,bytes,&got)==MXGA_OK && got.origin==UINT64_C(0x0807060504030201) &&
            got.generation==UINT64_MAX && got.text_bytes==sizeof(text) && !memcmp(got.text,text,sizeof(text)),"decode")) return 1;
    if (!check(mxga_encode_clipboard(0,0,NULL,0,encoded,16,&bytes)==MXGA_OK && bytes==16 &&
        mxga_decode_clipboard(encoded,bytes,&got)==MXGA_OK && got.text_bytes==0,"clear and zero metadata")) return 1;
    memset(&unchanged,0,sizeof(unchanged)); unchanged.origin=57;
    for (i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i) {
        got=unchanged;
        if (!check(mxga_encode_clipboard(1,2,bad[i],lengths[i],encoded,sizeof(encoded),&bytes)==MXGA_ERR_UTF8 && bytes==0,"malformed encode")) return 1;
        memset(encoded,0,16); memcpy(encoded+16,bad[i],lengths[i]);
        if (!check(mxga_decode_clipboard(encoded,16+lengths[i],&got)==MXGA_ERR_UTF8 && !memcmp(&got,&unchanged,sizeof(got)),"malformed decode unchanged")) return 1;
    }
    memset(large,'x',sizeof(large));
    if (!check(mxga_encode_clipboard(1,2,large,MXGA_CLIPBOARD_MAX_TEXT_BYTES,large,sizeof(large),&bytes)==MXGA_OK &&
        bytes==MXGA_MAX_FRAME_BYTES-MXGA_HEADER_BYTES && mxga_decode_clipboard(large,bytes,&got)==MXGA_OK,"maximum and overlap") ||
        !check(mxga_encode_clipboard(1,2,large,MXGA_CLIPBOARD_MAX_TEXT_BYTES+1,large,sizeof(large),&bytes)==MXGA_ERR_LENGTH,"oversize") ||
        !check(mxga_decode_clipboard(large,15,&got)==MXGA_ERR_LENGTH,"short") ||
        !check(mxga_decode_clipboard(large,MXGA_MAX_FRAME_BYTES,&got)==MXGA_ERR_LENGTH,"oversize decode") ||
        !check(mxga_encode_clipboard(1,2,text,sizeof(text),encoded,16,&bytes)==MXGA_ERR_LENGTH,"capacity") ||
        !check(mxga_encode_clipboard(1,2,NULL,1,encoded,sizeof(encoded),&bytes)==MXGA_ERR_PAYLOAD,"null text")) return 1;
    {
        union { struct mxga_clipboard alignment; uint8_t data[64]; } storage;
        uint8_t saved[64];
        uint32_t untouched=73;
        memset(&storage,0x5a,sizeof(storage)); memcpy(saved,storage.data,sizeof(saved));
        if (!check(mxga_encode_clipboard(1,2,bad[0],1,storage.data,sizeof(storage.data),&untouched)==MXGA_ERR_UTF8 &&
            !memcmp(saved,storage.data,sizeof(saved)) && untouched==0,"invalid text leaves output unchanged")) return 1;
        untouched=73;
        if (!check(mxga_encode_clipboard(1,2,text,sizeof(text),storage.data,sizeof(storage.data),(uint32_t *)storage.data)==MXGA_ERR_PAYLOAD &&
            !memcmp(saved,storage.data,sizeof(saved)),"encoded length aliases output unchanged") ||
            !check(mxga_encode_clipboard(1,2,storage.data,4,encoded,sizeof(encoded),(uint32_t *)storage.data)==MXGA_ERR_PAYLOAD &&
            !memcmp(saved,storage.data,sizeof(saved)),"encoded length aliases text unchanged") ||
            !check(mxga_decode_clipboard(storage.data,16,&storage.alignment)==MXGA_ERR_PAYLOAD &&
            !memcmp(saved,storage.data,sizeof(saved)),"decoded metadata aliases payload unchanged") ||
            !check(mxga_encode_clipboard(1,2,(const uint8_t *)(uintptr_t)(UINTPTR_MAX-1),4,encoded,sizeof(encoded),&untouched)==MXGA_ERR_PAYLOAD &&
            untouched==73,"wrapped text span") ||
            !check(mxga_encode_clipboard(1,2,text,sizeof(text),(uint8_t *)(uintptr_t)(UINTPTR_MAX-1),16,&untouched)==MXGA_ERR_PAYLOAD &&
            untouched==73,"wrapped output span") ||
            !check(mxga_encode_clipboard(1,2,text,sizeof(text),encoded,sizeof(encoded),(uint32_t *)(uintptr_t)(UINTPTR_MAX-1))==MXGA_ERR_PAYLOAD,"wrapped length span") ||
            !check(mxga_decode_clipboard((const uint8_t *)(uintptr_t)(UINTPTR_MAX-1),16,&got)==MXGA_ERR_PAYLOAD,"wrapped payload span") ||
            !check(mxga_decode_clipboard(encoded,16,(struct mxga_clipboard *)(uintptr_t)(UINTPTR_MAX-1))==MXGA_ERR_PAYLOAD,"wrapped decoded span") ||
            !check(mxga_encode_clipboard(1,2,text,sizeof(text),encoded,sizeof(encoded),NULL)==MXGA_OK,"optional length")) return 1;
    }
    puts("clipboard codec tests passed"); return 0;
}

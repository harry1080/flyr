/**
 * build.c
 *
 * Copyright (C) 2018 zznop, zznop0x90@gmail.com
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include "build.h"
#include "utils.h"
#include "conversion.h"

static int realloc_data_buffer(dud_t *ctx, size_t size)
{
    uint8_t *tmp = NULL;

    if (!ctx->buffer.data) {
        ctx->buffer.data = (uint8_t *)malloc(size);
        if (!ctx->buffer.data) {
            duderr("Out of memory");
            return FAILURE;
        }

        ctx->buffer.ptr = ctx->buffer.data;
    } else {
        tmp = realloc(ctx->buffer.data, ctx->buffer.size + size);
        if (!ctx->buffer.data) {
            duderr("Out of memory");
            return FAILURE;
        }

        ctx->buffer.data = tmp;
    }

    ctx->buffer.size += size;
    return SUCCESS;
}

static int consume_hexstr(struct json_value_t *block_json_value, dud_t *ctx)
{
    size_t data_size = 0;
    const char *pos = NULL;
    size_t i = 0;
    const char *value;

    value = json_object_get_string(json_object(block_json_value), "value");
    if (!value) {
        duderr("Failed to read data from block");
        return FAILURE;
    }

    if (value[strspn(value, "0123456789abcdefABCDEF")]) {
        duderr("Input data is not a valid hex string");
        return FAILURE;
    }

    data_size = strlen(value) / 2;
    if (realloc_data_buffer(ctx, data_size))
        return FAILURE;

    pos = value;
    for (i = 0; i < data_size; i++, ctx->buffer.ptr++) {
        sscanf(pos, "%2hhx", ctx->buffer.ptr);
        pos += 2;
    }

    return SUCCESS;
}

static enum endianess get_endianess(struct json_value_t *block_json_value)
{
    const char *value = json_object_get_string(json_object(block_json_value), "endianess");
    if (!value)
        return BIG_ENDIAN;

    if (strstr(value, "little"))
        return LITTLE_ENDIAN;
    else if (strstr(value, "big"))
        return BIG_ENDIAN;

    duderr("Erroneous endian specification: %s", value);
    return ERRONEOUS_ENDIAN;
}

static int consume_dword(struct json_value_t *block_json_value, dud_t *ctx)
{
    uint32_t dword;
    const char *value;
    enum endianess endian = get_endianess(block_json_value);
    if (endian == ERRONEOUS_ENDIAN) {
        return FAILURE;
    }

    value = json_object_get_string(json_object(block_json_value), "value");
    if (!value) {
        duderr("Failed to retrieve dword value from block");
        return FAILURE;
    }

    dword = hex_string_to_dword(value, endian);
    if (realloc_data_buffer(ctx, sizeof(dword)))
        return FAILURE;

    memcpy(ctx->buffer.ptr, &dword, sizeof(dword));
    ctx->buffer.ptr += sizeof(dword);

    return SUCCESS;
}

static int consume_word(struct json_value_t *block_json_value, dud_t *ctx)
{
    uint16_t word;
    const char *value;
    enum endianess endian = get_endianess(block_json_value);
    if (endian == ERRONEOUS_ENDIAN) {
        return FAILURE;
    }

    value = json_object_get_string(json_object(block_json_value), "value");
    if (!value) {
        duderr("Failed to retrieve dword value from block");
        return FAILURE;
    }

    word = hex_string_to_word(value, endian);
    if (realloc_data_buffer(ctx, sizeof(word)))
        return FAILURE;

    memcpy(ctx->buffer.ptr, &word, sizeof(word));
    ctx->buffer.ptr += sizeof(word);

    return SUCCESS;
}

static int handle_block(struct json_value_t *block_json_value, dud_t *ctx)
{
    const char *type = json_object_get_string(json_object(block_json_value), "type");
    if (!type) {
        duderr("Failed to retrieve block type");
        return FAILURE;
    }

    if (strstr(type, "hex"))
        return consume_hexstr(block_json_value, ctx);
    else if (strstr(type, "dword"))
        return consume_dword(block_json_value, ctx);
    else if (strstr(type, "word"))
        return consume_word(block_json_value, ctx);

    duderr("Unsupported block type: %s\n", type);
    return FAILURE;
}

int iterate_blocks(dud_t *ctx)
{
    struct json_value_t *block_json_value = NULL;

    for (ctx->blocks->idx = 0; ctx->blocks->idx < ctx->blocks->count; ctx->blocks->idx++) {
        block_json_value = json_object_get_value_at(
                json_object(ctx->blocks->json_value), ctx->blocks->idx);
        if (!block_json_value) {
            duderr("Failed to retrieve next JSON block (idx: %lu", ctx->blocks->idx);
            return FAILURE;
        }

        if (handle_block(block_json_value, ctx) == FAILURE)
            return FAILURE;
    }

    return SUCCESS;
}
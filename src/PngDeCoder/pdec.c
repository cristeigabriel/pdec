#include "pdec.h"
#include <stdlib.h> // malloc

/* Utility */

// https://marc.info/?l=flac-dev&m=129000489125347s
static inline uint32_t
byteswap_uint32(uint32_t x)
{
#if PDEC_REVERSE_ENDIANNESS == 1
    x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0x00FF00FF);
    return (x >> 16) | (x << 16);
#else
    return x;
#endif
}

static inline uint64_t
byteswap_uint64(uint64_t x)
{
#if PDEC_REVERSE_ENDIANNESS == 1
    return ((uint64_t)byteswap_uint32((uint32_t)x) << 32) |
           byteswap_uint32((uint32_t)(x >> 32));
#else
    return x;
#endif
}

/* Prototype bodies */

bool
pdec_is_valid_header(const PDEC_ImageBytemap image)
{
    const PDEC_Header header = *(PDEC_Header *)image;
    return byteswap_uint64(header.v_value) == k_png_header.v_value;
}

/* */

bool
pdec_is_valid_ihdr(const PDEC_ImageBytemap image)
{
    const PDEC_IHDR ihdr =
        *(PDEC_IHDR *)(image + sizeof(PDEC_Header) + sizeof(uint32_t));
    return byteswap_uint32(ihdr.v_value) == k_png_ihdr.v_value;
}

/* */

uint32_t
pdec_get_ihdr_data_chunk_size(const PDEC_ImageBytemap image)
{
    return byteswap_uint32(*(uint32_t *)(image + sizeof(PDEC_Header)));
}

/* */

uint32_t
pdec_get_ihdr_data_chunk_distance()
{
    return sizeof(PDEC_Header) + sizeof(uint32_t) + sizeof(PDEC_IHDR);
}

/* */

PDEC_IHDR_DataChunk
pdec_get_ihdr_data_chunk(const PDEC_ImageBytemap image)
{
    PDEC_IHDR_DataChunk chunk = {0};

    const uint32_t chunk_distance = pdec_get_ihdr_data_chunk_distance();

    chunk.m_w = byteswap_uint32(*(uint32_t *)(image + chunk_distance));
    chunk.m_h = byteswap_uint32(*(uint32_t *)(image + chunk_distance + 4));
    chunk.m_bit_depth          = *(uint8_t *)(image + chunk_distance + 8);
    chunk.m_color_type         = *(uint8_t *)(image + chunk_distance + 9);
    chunk.m_compression_method = *(uint8_t *)(image + chunk_distance + 10);
    chunk.m_filter_method      = *(uint8_t *)(image + chunk_distance + 11);
    chunk.m_interlace_method   = *(uint8_t *)(image + chunk_distance + 12);

    return chunk;
}

/* */

ptrdiff_t
pdec_get_first_crc_data_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_ihdr_data_chunk_distance() +
           pdec_get_ihdr_data_chunk_size(image);
}

/* */

uint32_t
pdec_get_first_crc_data(const PDEC_ImageBytemap image)
{
    return byteswap_uint32(
        *(uint32_t *)(image + pdec_get_first_crc_data_distance(image)));
}

/* */

ptrdiff_t
pdec_get_iccp_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_first_crc_data_distance(image) + sizeof(uint64_t);
}

/* */

bool
pdec_is_present_iccp(const PDEC_ImageBytemap image)
{
    const PDEC_iCCP iccp =
        *(PDEC_iCCP *)(image + pdec_get_iccp_distance(image));
    return byteswap_uint32(iccp.v_value) == k_png_iccp.v_value;
}

/* */

ptrdiff_t
pdec_get_iccp_padding(const PDEC_ImageBytemap image)
{
    if (pdec_is_present_iccp(image))
    {
        return sizeof(PDEC_iCCP);
    }

    return 0;
}

/* */

bool
pdec_is_iccp_data_chunk_present(const PDEC_ImageBytemap image)
{
    return !pdec_is_present_srgb(image);
}

/* */

uint32_t
pdec_get_iccp_profile_name_length(const PDEC_ImageBytemap image)
{
    if (!pdec_is_present_iccp(image) || !pdec_is_iccp_data_chunk_present(image))
    {
        return PDEC_INVALID_UINT32;
    }

    const ptrdiff_t position =
        pdec_get_iccp_distance(image) + pdec_get_iccp_padding(image);

    uint32_t size = 0;

    while (((uint8_t *)(image + position))[size++] != 0)
    {
    }

    /* against spec */

    if (size > 80)
    {
        return PDEC_INVALID_UINT32;
    }

    return size;
}

/* */

uint32_t
pdec_get_iccp_compressed_profile_length(const PDEC_ImageBytemap image)
{
    /* this is a hack but I found no sane documentation on this so I will do the
     * needful as is */

    if (!pdec_is_present_iccp(image) || !pdec_is_iccp_data_chunk_present(image))
    {
        return PDEC_INVALID_UINT32;
    }

    const ptrdiff_t position =
        pdec_get_iccp_distance(image) + pdec_get_iccp_padding(image) +
        pdec_get_iccp_profile_name_length(image) + sizeof(uint8_t);

    uint32_t size = 0;

    while (byteswap_uint32(*(uint32_t *)(image + position + (++size))) !=
           k_png_idat.v_value)
    {
    }

    /* once we're at IDAT, we go back 4 bytes to skip the 4 bytes which
     * represent the upcoming data chunk's size */

    return (size - 4);
}

/* */

PDEC_iCCP_DataChunk
pdec_get_iccp_data_chunk(const PDEC_ImageBytemap image)
{
    PDEC_iCCP_DataChunk result = {0};

    if (!pdec_is_iccp_data_chunk_present(image))
    {
        goto return_label;
    }

    {
        /* contains null-terminator byte */
        uint32_t len = pdec_get_iccp_profile_name_length(image);

        result.m_profile_name = (char *)malloc(len);

        const ptrdiff_t position =
            pdec_get_iccp_distance(image) + pdec_get_iccp_padding(image);

        for (uint32_t i = 0; i < len; ++i)
        {
            result.m_profile_name[i] = *(uint8_t *)(image + position + i);
        }

        result.m_compression_method = *(uint8_t *)(image + position + len);
    }

    {
        uint32_t len = pdec_get_iccp_compressed_profile_length(image);

        result.m_compressed_profile = (uint8_t *)malloc(len);

        const ptrdiff_t position =
            pdec_get_iccp_distance(image) + pdec_get_iccp_padding(image) +
            pdec_get_iccp_profile_name_length(image) + sizeof(uint8_t);

        for (uint32_t i = 0; i < len; ++i)
        {
            result.m_compressed_profile[i] = *(uint8_t *)(image + position + i);
        }
    }

return_label:
    return result;
}

/* */

uint32_t
pdec_get_iccp_data_chunk_size(const PDEC_ImageBytemap image)
{
    if (!pdec_is_present_iccp(image) || !pdec_is_iccp_data_chunk_present(image))
    {
        return PDEC_INVALID_UINT32;
    }

    return pdec_get_iccp_profile_name_length(image) + sizeof(uint8_t) +
           pdec_get_iccp_compressed_profile_length(image);
}

/* */

PDEC_FreeState
pdec_iccp_free_chunk_content(const PDEC_iCCP_DataChunk *chunk)
{
    bool profile_name       = chunk->m_profile_name != NULL;
    bool compressed_profile = chunk->m_compressed_profile != NULL;

    if (profile_name && compressed_profile)
    {
        free(chunk->m_profile_name);
        free(chunk->m_compressed_profile);
        return PDEC_FREE_STATE_YES;
    }
    else if (profile_name && !compressed_profile)
    {
        free(chunk->m_profile_name);
        return PDEC_FREE_STATE_PARTIAL;
    }
    else if (!profile_name && compressed_profile)
    {
        free(chunk->m_compressed_profile);
        return PDEC_FREE_STATE_PARTIAL;
    }

    return PDEC_FREE_STATE_NO;
}

/* */

ptrdiff_t
pdec_get_srgb_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_first_crc_data_distance(image) + sizeof(uint64_t) +
           pdec_get_iccp_padding(image);
}

/* */

bool
pdec_is_present_srgb(const PDEC_ImageBytemap image)
{
    const PDEC_sRGB srgb =
        *(PDEC_sRGB *)(image + pdec_get_srgb_distance(image));
    return byteswap_uint32(srgb.v_value) == k_png_srgb.v_value;
}

/* */

ptrdiff_t
pdec_get_rendering_intent_distance(const PDEC_ImageBytemap image)
{
    if (!pdec_is_present_srgb(image))
    {
        return -EILSEQ;
    }

    return pdec_get_srgb_distance(image) + sizeof(PDEC_sRGB);
}

/* */

uint8_t
pdec_get_rendering_intent(const PDEC_ImageBytemap image)
{
    const ptrdiff_t rendering_intent_distance =
        pdec_get_rendering_intent_distance(image);

    if (rendering_intent_distance == -EILSEQ)
    {
        return PDEC_INVALID_UINT8;
    }

    return *(uint8_t *)(image + rendering_intent_distance);
}

/* */

ptrdiff_t
pdec_get_idat_distance(const PDEC_ImageBytemap image)
{
    /* again, hack, but scales */

    ptrdiff_t position = pdec_get_srgb_distance(image);

    if (pdec_is_present_iccp(image))
    {
        const uint32_t len = pdec_get_iccp_data_chunk_size(image);
        position +=
            sizeof(PDEC_iCCP) + ((len == PDEC_INVALID_UINT32) ? 0 : len);
    }

    if (pdec_is_present_srgb(image))
    {
        /* sRGB identifier appears after iCCP, also carries rendering intent */

        position += sizeof(PDEC_sRGB) + sizeof(uint8_t);
    }

    /* if we're still not there */

    if (byteswap_uint32(*(uint32_t *)(image + position)) != k_png_idat.v_value)
    {
        /* this would work on itself alone but it's too much branching and
         * bitwise math to run continuously for no reason */

        while (byteswap_uint32(*(uint32_t *)(image + (++position))) !=
               k_png_idat.v_value)
        {
        }
    }

    return position;
}

/* */

uint32_t
pdec_get_idat_data_chunk_size(const PDEC_ImageBytemap image)
{
    return byteswap_uint32(*(uint32_t *)(image + pdec_get_idat_distance(image) -
                                         sizeof(PDEC_IDAT)));
}

/* */

uint8_t *
pdec_get_idat_data_chunk(const PDEC_ImageBytemap image)
{
    uint32_t len = pdec_get_idat_data_chunk_size(image);

    uint8_t *data = (uint8_t *)malloc(len);

    const ptrdiff_t position =
        pdec_get_idat_distance(image) + sizeof(PDEC_IDAT);

    for (uint32_t i = 0; i < len; ++i)
    {
        data[i] = *(uint8_t *)(image + position + i);
    }

    return data;
}

/* */

ptrdiff_t
pdec_get_last_crc_data_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_idat_distance(image) + sizeof(PDEC_IDAT) +
           pdec_get_idat_data_chunk_size(image);
}

/* */

uint32_t
pdec_get_last_crc_data(const PDEC_ImageBytemap image)
{
    return byteswap_uint32(
        *(uint32_t *)(image + pdec_get_last_crc_data_distance(image)));
}

/* */

ptrdiff_t
pdec_get_iend_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_last_crc_data_distance(image) + sizeof(PDEC_CRC_Data) +
           sizeof(uint32_t);
}

/* */

bool
pdec_is_valid_iend(const PDEC_ImageBytemap image)
{
    const PDEC_IEND iend =
        *(PDEC_IEND *)(image + pdec_get_iend_distance(image));
    return byteswap_uint32(iend.v_value) == k_png_iend.v_value;
}

/* */

ptrdiff_t
pdec_get_eof_distance(const PDEC_ImageBytemap image)
{
    return pdec_get_iend_distance(image) + sizeof(PDEC_IEND);
}

/* */

bool
pdec_is_valid_eof(const PDEC_ImageBytemap image)
{
    const PDEC_CRC_Data eof =
        *(PDEC_CRC_Data *)(image + pdec_get_eof_distance(image));
    return byteswap_uint32(eof.v_value) == k_png_eof.v_value;
}

/* */

PDEC_Status
pdec_new_ctx(PDEC_Context *ctx, const PDEC_ImageBytemap image)
{
    if (ctx != NULL)
    {
        return PDEC_STATUS_INVALID;
    }

    const bool valid_header = pdec_is_valid_header(image);
    const bool valid_ihdr   = pdec_is_valid_ihdr(image);
    const bool valid_eof    = pdec_is_valid_eof(image);

    if (!valid_header || !valid_ihdr || !valid_eof)
    {
        return PDEC_STATUS_INVALID;
    }

    ctx = (PDEC_Context *)malloc(sizeof *ctx);

    ctx->m_ihdr_data_chunk =
        (PDEC_IHDR_DataChunk *)malloc(sizeof *ctx->m_ihdr_data_chunk);
    *ctx->m_ihdr_data_chunk     = pdec_get_ihdr_data_chunk(image);
    ctx->m_ihdr_data_chunk_size = (ctx->m_ihdr_data_chunk != NULL)
                                      ? pdec_get_ihdr_data_chunk_size(image)
                                      : PDEC_INVALID_UINT32;

    const bool present_iccp = pdec_is_present_iccp(image);

    if (present_iccp)
    {
        ctx->m_iccp_data_chunk =
            (PDEC_iCCP_DataChunk *)malloc(sizeof *ctx->m_iccp_data_chunk);
        *ctx->m_iccp_data_chunk = pdec_get_iccp_data_chunk(image);
    }

    ctx->m_iccp_data_chunk_size = (present_iccp)
                                      ? pdec_get_iccp_data_chunk_size(image)
                                      : PDEC_INVALID_UINT32;

    const bool present_srgb = pdec_is_present_srgb(image);

    if (present_srgb)
    {
        ctx->m_srgb_data_chunk.m_rendering_intent =
            pdec_get_rendering_intent(image);
    }

    ctx->m_srgb_data_chunk_size = (uint32_t)present_srgb;

    ctx->m_crc_codes.m_first = pdec_get_first_crc_data(image);
    ctx->m_crc_codes.m_last  = pdec_get_last_crc_data(image);

    return PDEC_STATUS_VALID;
}

/* */

PDEC_FreeState
pdec_free_ctx(PDEC_Context *ctx)
{
    if (!ctx)
    {
        return PDEC_FREE_STATE_NO;
    }

    const bool ihdr_data_chunk = (ctx->m_ihdr_data_chunk != NULL);
    const bool iccp_data_chunk = (ctx->m_iccp_data_chunk != NULL);
    const bool idat_data_chunk = (ctx->m_idat_data_chunk != NULL);

    free(ctx);

    if (!ihdr_data_chunk && !iccp_data_chunk && !idat_data_chunk)
    {
        return PDEC_FREE_STATE_NO;
    }

    free(ctx->m_ihdr_data_chunk);
    pdec_iccp_free_chunk_content(ctx->m_iccp_data_chunk);
    free(ctx->m_iccp_data_chunk);
    free(ctx->m_idat_data_chunk);

    return ((ihdr_data_chunk & iccp_data_chunk & idat_data_chunk) == 0)
               ? PDEC_FREE_STATE_PARTIAL
               : PDEC_FREE_STATE_YES;
}
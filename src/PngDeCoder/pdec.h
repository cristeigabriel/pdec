/**
 * @file pdec.h
 * @author Cristei Gabriel-Marian (cristei.g772@gmail.com)
 * @brief PNG decoder.
 * @date 2021-10-31
 *
 * @copyright Copyright (c) 2021
 *
 */

/* PDEC assumes you're working over a bitmap which is loaded in memory, so by
 * default, it reverses endianness. To prevent this, set PDEC_REVERSE_ENDIANNESS
 * to 0. */

#pragma once

/* Includes */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Types */

typedef union
{
    uint64_t v_value;

    /* expect reversed endianness... */
    uint8_t v_array[8];
} PDEC_Header;

typedef union
{
    uint32_t v_value;

    /* expect reversed endianness... */
    uint8_t v_array[4];
} PDEC_IHDR, PDEC_iCCP, PDEC_sRGB, PDEC_IDAT, PDEC_IEND, PDEC_CRC_Data;

typedef struct
{
    uint32_t m_w;
    uint32_t m_h;
    uint8_t  m_bit_depth;
    uint8_t  m_color_type;
    uint8_t  m_compression_method;
    uint8_t  m_filter_method;
    uint8_t  m_interlace_method;
} PDEC_IHDR_DataChunk;

typedef struct
{
    /* 1-79 bytes, dynamic, this includes null terminator */
    char *m_profile_name;

    uint8_t m_compression_method;

    /* n bytes, dynamic, has no null terminator */
    uint8_t *m_compressed_profile;
} PDEC_iCCP_DataChunk;

typedef enum
{
    PDEC_FREE_STATE_NO = -1,
    PDEC_FREE_STATE_PARTIAL,
    PDEC_FREE_STATE_YES
} PDEC_FreeState;

typedef uintptr_t PDEC_ImageBytemap;

/* Extern */
#ifndef PDEC_EXTERN

#ifdef __cplusplus
#define PDEC_EXTERN extern "C"
#else
#define PDEC_EXTERN extern
#endif

#else
#error "PDEC_EXTERN already defined"
#endif

/* Prototypes */

/* this should guarantee the presence of critical chunks except optional ones */
PDEC_EXTERN bool
pdec_is_valid_header(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_valid_ihdr(const PDEC_ImageBytemap image);

PDEC_EXTERN uint32_t
get_ihdr_data_chunk_size(const PDEC_ImageBytemap image);

PDEC_EXTERN uint32_t
get_ihdr_data_chunk_distance();

PDEC_EXTERN PDEC_IHDR_DataChunk
get_ihdr_data_chunk(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_first_crc_data_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN uint32_t
pdec_get_first_crc_data(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_iccp_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_present_iccp(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_iccp_padding(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_iccp_data_chunk_present(const PDEC_ImageBytemap image);

/* includes null-terminator byte, returns PDEC_INVALID_UINT32 on failure */
PDEC_EXTERN uint32_t
pdec_get_iccp_profile_name_length(const PDEC_ImageBytemap image);

/* returns PDEC_INVALID_UINT32 on failure */
PDEC_EXTERN uint32_t
pdec_get_iccp_compressed_profile_length(const PDEC_ImageBytemap image);

/* PDEC_iCCP_DataChunk::m_profile_name and
 * PDEC_iCCP_DataChunk::m_compressed_profile are always dynamically allocated,
 * call pdec_iccp_free_chunk_content after you're done */
PDEC_EXTERN PDEC_iCCP_DataChunk
pdec_get_iccp_data_chunk(const PDEC_ImageBytemap image);

/* returns PDEC_INVALID_UINT32 upon failure */
PDEC_EXTERN uint32_t
pdec_get_iccp_data_chunk_size(const PDEC_ImageBytemap image);

PDEC_EXTERN PDEC_FreeState
pdec_iccp_free_chunk_content(const PDEC_iCCP_DataChunk *chunk);

PDEC_EXTERN ptrdiff_t
pdec_get_srgb_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_present_srgb(const PDEC_ImageBytemap image);

/* returns -EILSEQ upon failure */
PDEC_EXTERN ptrdiff_t
pdec_get_rendering_intent_distance(const PDEC_ImageBytemap image);

/* returns PDEC_INVALID_UINT32 upon failure */
PDEC_EXTERN uint32_t
pdec_get_rendering_intent(const PDEC_ImageBytemap image);

/* inherently guarantees presence of IDAT if works, for now
 * note: I have no idea how multiple IDATs may appear, even though the standard
 * specifies this possibility... */
PDEC_EXTERN ptrdiff_t
pdec_get_idat_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN uint32_t
pdec_get_idat_data_chunk_size(const PDEC_ImageBytemap image);

/* allocates, idiomatically free */
PDEC_EXTERN uint8_t *
            pdec_get_idat_data_chunk(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_last_crc_data_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN uint32_t
pdec_get_last_crc_data(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_iend_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_valid_iend(const PDEC_ImageBytemap image);

PDEC_EXTERN ptrdiff_t
pdec_get_eof_distance(const PDEC_ImageBytemap image);

PDEC_EXTERN bool
pdec_is_valid_eof(const PDEC_ImageBytemap image);

/* Constants */

/* magic number */
const static PDEC_Header k_png_header = {.v_value = 0x89504E470D0A1A0A};

const static PDEC_IHDR k_png_ihdr = {.v_value = 0x49484452};

/* optional */
const static PDEC_iCCP k_png_iccp = {.v_value = 0x69434350};

/* optional */
const static PDEC_sRGB k_png_srgb = {.v_value = 0x73524742};

const static PDEC_IDAT     k_png_idat = {.v_value = 0x49444154};
const static PDEC_IEND     k_png_iend = {.v_value = 0x49454E44};
const static PDEC_CRC_Data k_png_eof  = {.v_value = 0xAE426082};

#define PDEC_INVALID_UINT32 (uint32_t)(-1)
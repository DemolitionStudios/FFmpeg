/*
 * Vidvox Hap encoder
 * Copyright (C) 2015 Vittorio Giovara <vittorio.giovara@gmail.com>
 * Copyright (C) 2015 Tom Butterworth <bangnoise@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Hap encoder
 *
 * Fourcc: Hap1, Hap5, HapY
 *
 * https://github.com/Vidvox/hap/blob/master/documentation/HapVideoDRAFT.md
 */

#include <stdint.h>
#include "snappy-c.h"
#include "gdeflate-c.h"

#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "bytestream.h"
#include "hap.h"
#include "internal.h"
#include "texturedsp.h"

#define HAP_SNAPPY_MAX_CHUNKS 64
#define HAP_SNAPPY_FIXED_CHUNK_SIZE 65536

#define GDEFLATE_COMPRESSION_LEVEL GDeflateMinimumCompressionLevel // GDeflateMaximumCompressionLevel
#define GDEFLATE_NUM_THREADS 32

enum HapHeaderLength {
    /* Short header: four bytes with a 24 bit size value */
    HAP_HDR_SHORT = 4,
    /* Long header: eight bytes with a 32 bit size value */
    HAP_HDR_LONG = 8,
};

static bool hap_is_fixed_chunk_size(HapContext* ctx)
{
    return ctx->opt_chunk_count < 0;
}

static int compress_texture(AVCodecContext *avctx, uint8_t *out, int out_length, const AVFrame *f)
{
    HapContext *ctx = avctx->priv_data;
    int i, j;

    if (ctx->tex_size > out_length)
        return AVERROR_BUFFER_TOO_SMALL;

    for (j = 0; j < avctx->height; j += 4) {
        for (i = 0; i < avctx->width; i += 4) {
            uint8_t *p = f->data[0] + i * 4 + j * f->linesize[0];
            const int step = ctx->tex_fun(out, f->linesize[0], p);
            out += step;
        }
    }

    return 0;
}

/* section_length does not include the header */
static void hap_write_section_header(PutByteContext *pbc,
                                     enum HapHeaderLength header_length,
                                     int section_length,
                                     enum HapSectionType section_type)
{
    /* The first three bytes are the length of the section (not including the
     * header) or zero if using an eight-byte header.
     * For an eight-byte header, the length is in the last four bytes.
     * The fourth byte stores the section type. */
    bytestream2_put_le24(pbc, header_length == HAP_HDR_LONG ? 0 : section_length);
    bytestream2_put_byte(pbc, section_type);

    if (header_length == HAP_HDR_LONG) {
        bytestream2_put_le32(pbc, section_length);
    }
}

static int hap_compress_frame_snappy(AVCodecContext *avctx, uint8_t *dst)
{
    HapContext *ctx = avctx->priv_data;
    int i, final_size = 0;

    size_t uncompressed_size_remaining = ctx->tex_size;
    for (i = 0; i < ctx->chunk_count; i++) {
        HapChunk *chunk = &ctx->chunks[i];
        uint8_t *chunk_src, *chunk_dst;
        int ret;

        if (i == 0) {
            chunk->compressed_offset = 0;
        } else {
            chunk->compressed_offset = ctx->chunks[i-1].compressed_offset
                                     + ctx->chunks[i-1].compressed_size;
        }
        if (hap_is_fixed_chunk_size(ctx)) {
            chunk->uncompressed_size = FFMIN(HAP_SNAPPY_FIXED_CHUNK_SIZE, uncompressed_size_remaining);
            //av_log(avctx, AV_LOG_WARNING, "chunk %d; size: %d; offset: %d\n", i, chunk->uncompressed_size, chunk->uncompressed_offset);
            chunk->uncompressed_offset = i * chunk->uncompressed_size; // All chunks are same size, except for the last
            uncompressed_size_remaining -= chunk->uncompressed_size;
        } else {
            chunk->uncompressed_size = ctx->tex_size / ctx->chunk_count;
            chunk->uncompressed_offset = i * chunk->uncompressed_size;
        }
        chunk->compressed_size = ctx->max_compressed;
        chunk_src = ctx->tex_buf + chunk->uncompressed_offset;
        chunk_dst = dst + chunk->compressed_offset;

        /* Compress with snappy too, write directly on packet buffer. */
        ret = snappy_compress(chunk_src, chunk->uncompressed_size,
                              chunk_dst, &chunk->compressed_size);
        if (ret != SNAPPY_OK) {
            av_log(avctx, AV_LOG_ERROR, "Snappy compress error: %d; max_compressed: %d\n", ret, ctx->max_compressed);
            return AVERROR_BUG;
        }

        /* If there is no gain from snappy, just use the raw texture. */
        if (chunk->compressed_size >= chunk->uncompressed_size && !hap_is_fixed_chunk_size(ctx)) {
            av_log(avctx, AV_LOG_VERBOSE,
                   "Snappy buffer bigger than uncompressed (%"SIZE_SPECIFIER" >= %"SIZE_SPECIFIER" bytes).\n",
                   chunk->compressed_size, chunk->uncompressed_size);
            memcpy(chunk_dst, chunk_src, chunk->uncompressed_size);
            chunk->compressor = HAP_COMP_NONE;
            chunk->compressed_size = chunk->uncompressed_size;
        } else {
            chunk->compressor = HAP_COMP_SNAPPY;
        }

        final_size += chunk->compressed_size;
    }

    return final_size;
}

static int hap_compress_frame_gdeflate(AVCodecContext* avctx, uint8_t* dst)
{
    HapContext* ctx = avctx->priv_data;
    int i, final_size = ctx->max_compressed;

    /// TODO: fix decreasing fps
    /* GDeflate compression directly to the packet buffer. */
    //av_log(avctx, AV_LOG_WARNING, "GDeflate max size: %d\n", final_size);
    bool ok = gdeflate_compress(dst, &final_size, ctx->tex_buf, ctx->tex_size, GDEFLATE_COMPRESSION_LEVEL, 0, GDEFLATE_NUM_THREADS);
    //av_log(avctx, AV_LOG_WARNING, "GDeflate final size: %d\n", final_size);
    if (!ok) {
        av_log(avctx, AV_LOG_ERROR, "GDeflate compress error.\n");
        return AVERROR_BUG;
    }

    return final_size;
}

static int hap_decode_instructions_length(HapContext *ctx)
{
    /*    Second-Stage Compressor Table (one byte per entry)
     *  + Chunk Size Table (four bytes per entry)
     *  + headers for both sections (short versions)
     *  = chunk_count + (4 * chunk_count) + 4 + 4 */
    return (5 * ctx->chunk_count) + 8;
}

static int hap_header_length(HapContext *ctx)
{
    /* Top section header (long version) */
    int length = HAP_HDR_LONG;

    if (ctx->chunk_count > 1) {
        /* Decode Instructions header (short) + Decode Instructions Container */
        length += HAP_HDR_SHORT + hap_decode_instructions_length(ctx);
    }

    return length;
}

static void hap_write_frame_header(HapContext *ctx, uint8_t *dst, int frame_length)
{
    PutByteContext pbc;
    int i;

    bytestream2_init_writer(&pbc, dst, frame_length);
    if (ctx->chunk_count == 1) {
        /* Write a simple header */
        hap_write_section_header(&pbc, HAP_HDR_LONG, frame_length - 8,
                                 ctx->chunks[0].compressor | ctx->opt_tex_fmt);
    } else {
        /* Write a complex header with Decode Instructions Container */
        hap_write_section_header(&pbc, HAP_HDR_LONG, frame_length - 8,
                                 HAP_COMP_COMPLEX | ctx->opt_tex_fmt);
        hap_write_section_header(&pbc, HAP_HDR_SHORT, hap_decode_instructions_length(ctx),
                                 HAP_ST_DECODE_INSTRUCTIONS);
        hap_write_section_header(&pbc, HAP_HDR_SHORT, ctx->chunk_count,
                                 HAP_ST_COMPRESSOR_TABLE);

        for (i = 0; i < ctx->chunk_count; i++) {
            bytestream2_put_byte(&pbc, ctx->chunks[i].compressor >> 4);
        }

        hap_write_section_header(&pbc, HAP_HDR_SHORT, ctx->chunk_count * 4,
                                 HAP_ST_SIZE_TABLE);

        for (i = 0; i < ctx->chunk_count; i++) {
            bytestream2_put_le32(&pbc, ctx->chunks[i].compressed_size);
        }
    }
}

static int hap_encode(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame, int *got_packet)
{
    HapContext *ctx = avctx->priv_data;
    int header_length = hap_header_length(ctx);
    int final_data_size, ret;
    int pktsize = FFMAX(ctx->tex_size, ctx->max_compressed * ctx->chunk_count) + header_length;

    /* Allocate maximum size packet, shrink later. */
    ret = ff_alloc_packet2(avctx, pkt, pktsize, header_length);
    if (ret < 0)
        return ret;

    if (ctx->opt_compressor == HAP_COMP_NONE) {
        /* DXTC compression directly to the packet buffer. */
        ret = compress_texture(avctx, pkt->data + header_length, pkt->size - header_length, frame);
        if (ret < 0)
            return ret;

        ctx->chunks[0].compressor = HAP_COMP_NONE;
        final_data_size = ctx->tex_size;
    } else {
        /* DXTC compression. */
        ret = compress_texture(avctx, ctx->tex_buf, ctx->tex_size, frame);
        if (ret < 0)
            return ret;

        if (ctx->opt_compressor == HAP_COMP_SNAPPY) {
            /* Compress the frame using Snappy */
            final_data_size = hap_compress_frame_snappy(avctx, pkt->data + header_length);
            if (final_data_size < 0)
                return final_data_size;
        }
        else if (ctx->opt_compressor == HAP_COMP_GDEFLATE) {
            /* Compress the frame using GDeflate */
            final_data_size = hap_compress_frame_gdeflate(avctx, pkt->data + header_length);
            if (final_data_size < 0)
                return final_data_size;

            ctx->chunks[0].compressor = HAP_COMP_GDEFLATE;
        } else {
            return -1;
        }
    } 

    /* Write header at the start. */
    hap_write_frame_header(ctx, pkt->data, final_data_size + header_length);

    av_shrink_packet(pkt, final_data_size + header_length);
    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    return 0;
}

static av_cold int hap_init(AVCodecContext *avctx)
{
    HapContext *ctx = avctx->priv_data;
    int ratio;
    int corrected_chunk_count;
    int ret = av_image_check_size(avctx->width, avctx->height, 0, avctx);

    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Invalid video size %dx%d.\n",
               avctx->width, avctx->height);
        return ret;
    }

    if (avctx->width % 4 || avctx->height % 4) {
        av_log(avctx, AV_LOG_ERROR, "Video size %dx%d is not multiple of 4.\n",
               avctx->width, avctx->height);
        return AVERROR_INVALIDDATA;
    }

    ff_texturedspenc_init(&ctx->dxtc);
    ff_bc7enc16_init(&ctx->bc7c, BC7ENC16_TRUE /* perceptual */, BC7ENC16_MAX_PARTITIONS1 /* max_partitions_to_scan */, 0 /* uber_level */);

    switch (ctx->opt_tex_fmt) {
    case HAP_FMT_RGBDXT1:
        ratio = 8;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '1');
        avctx->bits_per_coded_sample = 24;
        ctx->tex_fun = ctx->dxtc.dxt1_block;
        break;
    case HAP_FMT_RGBADXT5:
        ratio = 4;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '5');
        avctx->bits_per_coded_sample = 32;
        ctx->tex_fun = ctx->dxtc.dxt5_block;
        break;
    case HAP_FMT_YCOCGDXT5:
        ratio = 4;
        avctx->codec_tag = MKTAG('H', 'a', 'p', 'Y');
        avctx->bits_per_coded_sample = 24;
        ctx->tex_fun = ctx->dxtc.dxt5ys_block;
        break;
    case HAP_FMT_BPTC:
        ratio = 4;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '7');
        avctx->bits_per_coded_sample = 32;
        ctx->tex_fun = ctx->bc7c.bc7enc16_block;
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Invalid format %02X\n", ctx->opt_tex_fmt);
        return AVERROR_INVALIDDATA;
    }

    /* Texture compression ratio is constant, so can we computer
     * beforehand the final size of the uncompressed buffer. */
    ctx->tex_size   = FFALIGN(avctx->width,  TEXTURE_BLOCK_W) *
                      FFALIGN(avctx->height, TEXTURE_BLOCK_H) * 4 / ratio;

    switch (ctx->opt_compressor) {
    case HAP_COMP_NONE:
        /* No benefit chunking uncompressed data */
        corrected_chunk_count = 1;

        ctx->max_compressed = ctx->tex_size;
        ctx->tex_buf = NULL;
        break;
    case HAP_COMP_SNAPPY:
        if (hap_is_fixed_chunk_size(ctx)) {
            corrected_chunk_count = (ctx->tex_size + HAP_SNAPPY_FIXED_CHUNK_SIZE - 1) / HAP_SNAPPY_FIXED_CHUNK_SIZE;
            ctx->max_compressed = snappy_max_compressed_length(HAP_SNAPPY_FIXED_CHUNK_SIZE);
            av_log(avctx, AV_LOG_WARNING, "Snappy chunks fixed size; count: %d; tex_size: %d\n", corrected_chunk_count, ctx->tex_size);
        }
        else {
            /* Round the chunk count to divide evenly on DXT block edges */
            corrected_chunk_count = av_clip(ctx->opt_chunk_count, 1, HAP_SNAPPY_MAX_CHUNKS);
            while ((ctx->tex_size / (64 / ratio)) % corrected_chunk_count != 0) {
                corrected_chunk_count--;
            }
            ctx->max_compressed = snappy_max_compressed_length(ctx->tex_size / corrected_chunk_count);
        }

        ctx->tex_buf = av_malloc(ctx->tex_size);
        if (!ctx->tex_buf) {
            return AVERROR(ENOMEM);
        }
        break;
    case HAP_COMP_GDEFLATE:
        /* GDeflate uses internal 64Kb chunks */
        corrected_chunk_count = 1;

        ctx->max_compressed = gdeflate_compress_bound(ctx->tex_size);
        ctx->tex_buf = av_malloc(ctx->tex_size);
        if (!ctx->tex_buf) {
            return AVERROR(ENOMEM);
        }

        gdeflate_init_thread_pool(GDEFLATE_NUM_THREADS);
        break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Invalid compresor %02X\n", ctx->opt_compressor);
        return AVERROR_INVALIDDATA;
    }
    if (corrected_chunk_count != ctx->opt_chunk_count && !hap_is_fixed_chunk_size(ctx)) {
        av_log(avctx, AV_LOG_INFO, "%d chunks requested but %d used.\n",
                                    ctx->opt_chunk_count, corrected_chunk_count);
    }
    ret = ff_hap_set_chunk_count(ctx, corrected_chunk_count, 1);
    if (ret != 0)
        return ret;

    return 0;
}

static av_cold int hap_close(AVCodecContext *avctx)
{
    HapContext *ctx = avctx->priv_data;

    ff_hap_free_context(ctx);

    return 0;
}

/* TODO */
/*
  1. GPU encoder : GPU snappy / gdeflate(cuda) + GPU texture compression
  FFmpeg or CC based : https://github.com/disguise-one/hap-encoder-adobe-cc
  https://notchlc.notch.one/: encode speed baseline
  By utilizing the full power of the GPU to massively accelerate the encoding process,
  you can expect to encode 1080p24 at a rate of 5.7 mins of footage in 1 minute of encoding(on a consumer - grade PC).

      https ://github.com/GPUOpen-Tools/compressonator/tree/master/cmp_core/shaders
      or
      https ://github.com/richgel999/bc7enc_rdo
      https ://github.com/BinomialLLC/bc7e
      https ://github.com/richgel999/bc7enc
      https ://github.com/richgel999/bc7enc16
      https ://www.phoronix.com/news/OSS-Game-Industry-Concerns
      https ://twitter.com/richgel999/status/1454580043607334915
      https ://github.com/walbourn/directx-sdk-samples/tree/main/BC6HBC7EncoderCS
      https ://github.com/mvji/SPX-GC

  2. Try lossless texture compression
  3. NotchLC capture shaders with PIX
  4. Uncompressed YUV format - good for gdeflate probably (planar)
  */

#define OFFSET(x) offsetof(HapContext, x)
#define FLAGS     AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "format", NULL, OFFSET(opt_tex_fmt), AV_OPT_TYPE_INT, { .i64 = HAP_FMT_RGBDXT1 }, HAP_FMT_RGBDXT1, HAP_FMT_YCOCGDXT5, FLAGS, "format" },
        { "hap",       "Hap 1 (DXT1 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBDXT1   }, 0, 0, FLAGS, "format" },
        { "hap_alpha", "Hap Alpha (DXT5 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBADXT5  }, 0, 0, FLAGS, "format" },
        { "hap_q",     "Hap Q (DXT5-YCoCg textures)", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_FMT_YCOCGDXT5 }, 0, 0, FLAGS, "format" },
        { "hap_r",     "Hap R (BC7 textures)", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_FMT_BPTC }, 0, 0, FLAGS, "format" },
    { "chunks", "chunk count", OFFSET(opt_chunk_count), AV_OPT_TYPE_INT, {.i64 = 1 }, -1, HAP_SNAPPY_MAX_CHUNKS, FLAGS, },
    { "compressor", "second-stage compressor", OFFSET(opt_compressor), AV_OPT_TYPE_INT, { .i64 = HAP_COMP_SNAPPY }, HAP_COMP_NONE, HAP_COMP_GDEFLATE, FLAGS, "compressor" },
        { "none",       "None", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_COMP_NONE }, 0, 0, FLAGS, "compressor" },
        { "snappy",     "Snappy", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_COMP_SNAPPY }, 0, 0, FLAGS, "compressor" },
        { "gdeflate",   "GDeflate", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_COMP_GDEFLATE }, 0, 0, FLAGS, "compressor" },
    { NULL },
};

static const AVClass hapenc_class = {
    .class_name = "Hap encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_hap_encoder = {
    .name           = "hap",
    .long_name      = NULL_IF_CONFIG_SMALL("Vidvox Hap"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HAP,
    .priv_data_size = sizeof(HapContext),
    .priv_class     = &hapenc_class,
    .init           = hap_init,
    .encode2        = hap_encode,
    .close          = hap_close,
    .pix_fmts       = (const enum AVPixelFormat[]) {
        AV_PIX_FMT_RGBA, AV_PIX_FMT_NONE,
    },
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE |
                      FF_CODEC_CAP_INIT_CLEANUP,
};

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

#define USE_DIRECTXTEX 0

#include <stdint.h>
#include <float.h>

#include "snappy-c.h"
#include "gdeflate-c.h"
#include "bc7e_ispc.h"
#if USE_DIRECTXTEX
	#include "DirectXTex-c.h"
#endif
#include "GPURealTimeBC6H-c.h"

#include "libavutil/frame.h"
#include "libavutil/imgutils.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"

#include "avcodec.h"
#include "bytestream.h"
#include "codec_internal.h"
#include "encode.h"
#include "hap.h"
#include "texturedsp.h"

#define HAP_SNAPPY_MAX_CHUNKS 64
#define HAP_SNAPPY_FIXED_CHUNK_SIZE 65536

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

// FPS
// 
// BC7, 1080p
// Default: 1.3
// Compressonator CPU: 3.3
// Compressonator HPC: 3.3 
// Compressonator DXC: 1.1

// DXT1, 1080p
// Default: 27
// Compressonator CPU: 16
// Compressonator HPC: 16
// Compressonator DXC: 

static AVFrame* toRGBAF32(HapContext* ctx, AVFrame* frame)
{
	int width = frame->width;
	int height = frame->height;
	static AVFrame* frameRGBAF32 = NULL;

	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBAF32, width, height, 1);
	if (frameRGBAF32 == NULL) {
		frameRGBAF32 = av_frame_alloc();
		frameRGBAF32->format = AV_PIX_FMT_RGBAF32;
		frameRGBAF32->width = width;
		frameRGBAF32->height = height;

		uint8_t* dataBuffer = (uint8_t*)av_malloc(numBytes);
		frameRGBAF32->data[0] = dataBuffer;
		av_image_fill_arrays(frameRGBAF32->data, frameRGBAF32->linesize, dataBuffer, AV_PIX_FMT_RGBAF32, width, height, 1);
	}

	// Note: alpha channel is ignored in bc6 encoding, so we don't set it at all
	int i, j;
	if (frame->format == AV_PIX_FMT_RGBA) {
		float normalization_factor = 255.f;
		if (ctx->opt_hap_h_normalization_factor != 0.f) {
			normalization_factor = ctx->opt_hap_h_normalization_factor;
		}

		for (j = 0; j < height; j += 1) {
			for (i = 0; i < width; i += 1) {
				const uint8_t* ptrRGBA = frame->data[0] + j * frame->linesize[0] + i * 4;
				int offset = j * frameRGBAF32->linesize[0] + i * 4 * sizeof(float);

				float* ptr = (float*)(frameRGBAF32->data[0] + offset);
				ptr[0] = ptrRGBA[0] / normalization_factor;
				ptr[1] = ptrRGBA[1] / normalization_factor;
				ptr[2] = ptrRGBA[2] / normalization_factor;

				// Debug
				// all 1.0 = white
				// all 0.0/0.5 = black
				// all 0.8 = gray
				//ptr[0] = 0.8;
				//ptr[1] = 0.8;
				//ptr[2] = 0.8;
			}
		}
		
		// Test int values for correct float representation
		//for (int i = 0; ; i += 100) {
		//	int j = -i;
		//	float x = *((float*)(&i));
		//	if (x > 100 && x < 255)
		//		av_log(NULL, AV_LOG_ERROR, "i: %d, x: %f\n", i, x);
		//	x = *((float*)(&j));
		//	if (x > 100 && x < 255)
		//		av_log(NULL, AV_LOG_ERROR, "i: %d, x: %f\n", j, x);
		//}
		
		////i: 1132334300, x: 254.050232
		//float* p = (float*)frameRGBAF32->data[0];
		//int val = 1132334300;
		//float h = 65520.f;
		//for (int i = 0; i < numBytes / 4; ++i) {
		//	//memcpy((float*)frameRGBAF32->data[0] + i, &val, 4);
		//	p[i] = h;
		//	//av_log(NULL, AV_LOG_ERROR, "i: %d, x: %f\n", *((int*)(&p[i])), p[i]);
		//}
	} else if (frame->format == AV_PIX_FMT_GBRAPF32) {
		float normalization_factor = 1.f;
		if (ctx->opt_hap_h_normalization_factor != 0.f) {
			normalization_factor = ctx->opt_hap_h_normalization_factor;
		}

		for (j = 0; j < height; j += 1) {
			for (i = 0; i < width; i += 1) {
				int offset = j * frameRGBAF32->linesize[0] + i * 4 * sizeof(float);
				int offsetPlanar = j * frame->linesize[0] + i * sizeof(float);

				/// TODO: figure out if it's stated somewhere that ffmpeg automatically applies gamma to linear values stored inside EXR, but it seems so
				// Convert to sRGB
				//const float gamma = 1 / 2.2f; powf(value, gamma)

				/// TODO: figure out why R / B channels are swapped
				float* ptr = (float*)(frameRGBAF32->data[0] + offset);
				ptr[0] = *((float*)(frame->data[2] + offsetPlanar)) / normalization_factor; // G
				ptr[1] = *((float*)(frame->data[0] + offsetPlanar)) / normalization_factor; // R ????
				ptr[2] = *((float*)(frame->data[1] + offsetPlanar)) / normalization_factor; // B ????
			}
		}
	} else {
		av_log(NULL, AV_LOG_ERROR, "Only supported AV_PIX_FMT_RGBA and AV_PIX_FMT_GBRAPF32 for hap_h\n");
	}
	return frameRGBAF32;
}

static int compress_texture(AVCodecContext *avctx, uint8_t *out, int out_length, const AVFrame *f)
{
    HapContext *ctx = avctx->priv_data;
    int i, j;

    if (ctx->tex_size > out_length)
        return AVERROR_BUFFER_TOO_SMALL;

	/// TODO: cpu isn't used at 100% now even with 64 threads. Wtf??
	if (ctx->opt_tex_fmt == HAP_FMT_BPTC) {
		if (f->format != AV_PIX_FMT_RGBA) {
			av_log(avctx, AV_LOG_ERROR, "AV_PIX_FMT_RGBA source format required for Hap R\n");
			return AVERROR_INVALIDDATA;
		}

#if !USE_DIRECTXTEX
		// https://github.com/GPUOpen-Tools/compressonator/blob/815d1b6fa01223cdbeb3e399e56b44e5c10fcdd7/cmp_compressonatorlib/buffer/codecbuffer_rgba8888.cpp
		/// TODO: make a special "color space" for it. so we transform directly from 420p->blocks
		/// TODO: or in-place conversion to save memory while using threads, cache only transformed 4-pixel rows
		/// TODO: + maybe use memory pool for the cached 4-pixel rows
		uint8_t* blocks = (uint8_t*)av_malloc(f->linesize[0] * avctx->height);
		uint8_t* blocks_ptr = blocks;
		for (j = 0; j < avctx->height; j += 4) {
			for (i = 0; i < avctx->width; i += 4) {
				const uint8_t* p = f->data[0] + i * 4 + j * f->linesize[0];
				const int block_size = 16 * 4;
				const int block_row_size = 4 * 4;

				memcpy(blocks_ptr, p, block_row_size);
				memcpy(blocks_ptr + block_row_size,   p + f->linesize[0],   block_row_size);
				memcpy(blocks_ptr + block_row_size*2, p + f->linesize[0]*2, block_row_size);
				memcpy(blocks_ptr + block_row_size*3, p + f->linesize[0]*3, block_row_size);

				blocks_ptr += block_size;
			}
		}

		int num_blocks = avctx->width * avctx->height / 16;
		bc7e_compress_blocks(num_blocks, out, blocks, &ctx->bc7e_params);

		av_free(blocks);
#else
		// DirectXTex test
		struct Image srcImage, dstImage;
		srcImage.width = avctx->width;
		srcImage.height = avctx->height;
		srcImage.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcImage.rowPitch = avctx->width * sizeof(uint8_t) * 4;
		srcImage.slicePitch = avctx->width * avctx->height * sizeof(uint8_t) * 4;
		float* blocks = (uint8_t*)av_malloc(srcImage.slicePitch);
		float* blocks_ptr = blocks;
		for (j = 0; j < avctx->height; j += 1) {
			for (i = 0; i < avctx->width; i += 1) {
				blocks_ptr[0] = ((float*)f->data[0])[j * avctx->width + i];
				blocks_ptr[1] = ((float*)f->data[1])[j * avctx->width + i];
				blocks_ptr[2] = ((float*)f->data[2])[j * avctx->width + i];
				blocks_ptr[3] = ((float*)f->data[3])[j * avctx->width + i];
				blocks_ptr += 4;
			}
		}
		//srcImage.pixels = f->data[0];
		srcImage.pixels = blocks;
		uint32_t format = DXGI_FORMAT_BC7_UNORM;
		uint32_t flags = TEX_COMPRESS_PARALLEL;
		HRESULT result = DirectXTex_Compress(&srcImage, format, flags, TEX_THRESHOLD_DEFAULT_V, &dstImage);
		if (FAILED(result))
		{
			av_log(avctx, AV_LOG_ERROR, "DirectXTex_Compress failed\n");
			return AVERROR_BUG;
		}

		av_log(avctx, AV_LOG_ERROR, "Input image size %dx%d rowPitch %d.\n",
			srcImage.width, srcImage.height, srcImage.rowPitch);
		av_log(avctx, AV_LOG_ERROR, "Output bc7 image size %dx%d rowPitch %d slicePitch %d format %d.\n",
			dstImage.width, dstImage.height, dstImage.rowPitch, dstImage.slicePitch, dstImage.format);
		av_log(avctx, AV_LOG_ERROR, "out_length %d, .\n",
			out_length);

		memcpy(out, dstImage.pixels, out_length);

		DirectXTex_FreeOutputImage(&dstImage);
		av_free(blocks);
#endif
	} else if (ctx->opt_tex_fmt == HAP_FMT_BPTC_FU) {
#if USE_DIRECTXTEX
		// DirectXTex test (didn't get it to work and very slow)
		
		// TODO: reorder
		// TODO: try artificial image with bar
		// TODO: check if we really get converted to float frame by size
		// TODO: try directxtex without ffmpeg

		struct Image srcImage, dstImage;
		srcImage.width = avctx->width;
		srcImage.height = avctx->height;
		srcImage.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srcImage.rowPitch = avctx->width * sizeof(float) * 4;
		srcImage.slicePitch = avctx->width * avctx->height * sizeof(float) * 4;
		float* blocks = (uint8_t*)av_malloc(srcImage.slicePitch);
		float* blocks_ptr = blocks;
		for (j = 0; j < avctx->height; j += 1) {
			for (i = 0; i < avctx->width; i += 1) {
				blocks_ptr[0] = ((float*)f->data[0])[j * avctx->width + i];
				blocks_ptr[1] = ((float*)f->data[1])[j * avctx->width + i];
				blocks_ptr[2] = ((float*)f->data[2])[j * avctx->width + i];
				blocks_ptr[3] = ((float*)f->data[3])[j * avctx->width + i];
				blocks_ptr += 4;
			}
		}
		//srcImage.pixels = f->data[0];
		srcImage.pixels = blocks;
		uint32_t format = DXGI_FORMAT_BC6H_UF16;
		uint32_t flags = TEX_COMPRESS_PARALLEL;
		HRESULT result = DirectXTex_Compress(&srcImage, format, flags, TEX_THRESHOLD_DEFAULT_V, &dstImage);
		if (FAILED(result))
		{
			av_log(avctx, AV_LOG_ERROR, "DirectXTex_Compress failed\n");
			return AVERROR_BUG;
		}

		av_log(avctx, AV_LOG_ERROR, "Input image size %dx%d rowPitch %d.\n",
			srcImage.width, srcImage.height, srcImage.rowPitch);
		av_log(avctx, AV_LOG_ERROR, "Output bc6h image size %dx%d rowPitch %d slicePitch %d format %d.\n",
			dstImage.width, dstImage.height, dstImage.rowPitch, dstImage.slicePitch, dstImage.format);
		av_log(avctx, AV_LOG_ERROR, "out_length %d, .\n",
			out_length);

		memcpy(out, dstImage.pixels, out_length);

		DirectXTex_FreeOutputImage(&dstImage);
		av_free(blocks);
#else
		AVFrame* rgbaf32frame = toRGBAF32(ctx, f);
		if (!rgbaf32frame) {
			av_log(avctx, AV_LOG_ERROR, "toRGBAF32 failed\n");
			return AVERROR_INVALIDDATA;
		}

		GPURealTimeBC6H_Image srcImage;
		srcImage.width = avctx->width;
		srcImage.height = avctx->height;
		srcImage.data = rgbaf32frame->data[0];
		srcImage.dataSize = srcImage.width * avctx->height * sizeof(float) * 4;

		uint32_t srcFormat = GPURealTimeBC6H_ImageFormat_RGBA32F;
		GPURealTimeBC6H_Image dstImage;
		/// TODO: fix on Nvidia (unable to create texture)
		/// Access from different thread due to frame threading should be ok
		bool ok = GPURealTimeBC6H_Compress(&srcImage, srcFormat, &dstImage);
		if (!ok) {
			av_log(avctx, AV_LOG_ERROR, "GPURealTimeBC6H_Compress error");
			return AVERROR_BUG;
		}
		/// TODO: make no memcpy (provide buffer)
		memcpy(out, dstImage.data, out_length);

		GPURealTimeBC6H_FreeImage(&dstImage);
#endif
	} else {
		if (f->format != AV_PIX_FMT_RGBA)
			return AVERROR_INVALIDDATA;

		ctx->enc.tex_data.out = out;
		ctx->enc.frame_data.in = f->data[0];
		ctx->enc.stride = f->linesize[0];
		avctx->execute2(avctx, ff_texturedsp_compress_thread, &ctx->enc, NULL, ctx->enc.slice_count);
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
	int i;
	size_t final_size = ctx->max_compressed;

    /* GDeflate compression directly to the packet buffer. */
    //av_log(avctx, AV_LOG_WARNING, "GDeflate max size: %d\n", final_size);
    bool ok = gdeflate_compress(dst, &final_size, ctx->tex_buf, ctx->tex_size, ctx->opt_gdeflate_level, 0, GDEFLATE_NUM_THREADS);
    //av_log(avctx, AV_LOG_WARNING, "GDeflate final size: %d\n", final_size);
    if (!ok) {
        av_log(avctx, AV_LOG_ERROR, "GDeflate compress error.\n");
        return AVERROR_BUG;
    }

    return (int)final_size;
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
    ret = ff_alloc_packet(avctx, pkt, pktsize);
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
    *got_packet = 1;
    return 0;
}

static av_cold int hap_init(AVCodecContext *avctx)
{
    HapContext *ctx = avctx->priv_data;
    int corrected_chunk_count;
	int preset;
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
    
    switch (ctx->opt_tex_fmt) {
    case HAP_FMT_RGBDXT1:
        ctx->enc.tex_ratio = 8;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '1');
        avctx->bits_per_coded_sample = 24;
        ctx->enc.tex_funct = ctx->dxtc.dxt1_block;
        break;
    case HAP_FMT_RGBADXT5:
        ctx->enc.tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '5');
        avctx->bits_per_coded_sample = 32;
        ctx->enc.tex_funct = ctx->dxtc.dxt5_block;
        break;
    case HAP_FMT_YCOCGDXT5:
        ctx->enc.tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', 'Y');
        avctx->bits_per_coded_sample = 24;
        ctx->enc.tex_funct = ctx->dxtc.dxt5ys_block;
        break;
    case HAP_FMT_BPTC:
        ctx->enc.tex_ratio = 16;
        avctx->codec_tag = MKTAG('H', 'a', 'p', '7');
        avctx->bits_per_coded_sample = 32;

		bc7e_compress_block_init();
		if (ctx->opt_texture_quality_hap_r == 0)
		{
			av_log(avctx, AV_LOG_ERROR, "Using bc7e_compress_block_params_init_ultrafast preset\n");
			bc7e_compress_block_params_init_ultrafast(&ctx->bc7e_params, true /* perceptual */);
		}
		else if (ctx->opt_texture_quality_hap_r == 1)
		{
			av_log(avctx, AV_LOG_ERROR, "Using bc7e_compress_block_params_init_fast preset\n");
			bc7e_compress_block_params_init_fast(&ctx->bc7e_params, true /* perceptual */);
		}
		else if (ctx->opt_texture_quality_hap_r == 2)
		{
			av_log(avctx, AV_LOG_ERROR, "Using bc7e_compress_block_params_init_slow preset\n");
			bc7e_compress_block_params_init_slow(&ctx->bc7e_params, true /* perceptual */);
		}
        break;
	case HAP_FMT_BPTC_FU:
		ctx->enc.tex_ratio = 16;
		avctx->codec_tag = MKTAG('H', 'a', 'p', 'H');
		avctx->bits_per_coded_sample = 32;

		preset = ctx->opt_texture_quality_hap_h == 0 ? GPURealTimeBC6H_Preset_Speed : GPURealTimeBC6H_Preset_Quality;
		if (preset == GPURealTimeBC6H_Preset_Speed)
			av_log(avctx, AV_LOG_ERROR, "Using GPURealTimeBC6H_Preset_Speed preset\n");
		else
			av_log(avctx, AV_LOG_ERROR, "Using GPURealTimeBC6H_Preset_Quality preset\n");
		GPURealTimeBC6H_Initialize(preset);
		break;
    default:
        av_log(avctx, AV_LOG_ERROR, "Invalid format %02X\n", ctx->opt_tex_fmt);
        return AVERROR_INVALIDDATA;
    }
    ctx->enc.raw_ratio = 16;
    ctx->enc.slice_count = av_clip(avctx->thread_count, 1, avctx->height / TEXTURE_BLOCK_H);

    /* Texture compression ratio is constant, so can we compute
     * beforehand the final size of the uncompressed buffer. */
    ctx->tex_size   = avctx->width  / TEXTURE_BLOCK_W *
                      avctx->height / TEXTURE_BLOCK_H * ctx->enc.tex_ratio;

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
        while ((ctx->tex_size / ctx->enc.tex_ratio) % corrected_chunk_count != 0) {
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

	if (ctx->opt_tex_fmt == HAP_FMT_BPTC_FU) {
		GPURealTimeBC6H_Release();
	}

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
	  https://github.com/aras-p/bc7e-on-gpu
      https ://www.phoronix.com/news/OSS-Game-Industry-Concerns
      https ://twitter.com/richgel999/status/1454580043607334915
      https ://github.com/walbourn/directx-sdk-samples/tree/main/BC6HBC7EncoderCS
      https ://github.com/mvji/SPX-GC

  2. Try lossless texture compression: uncompressed YUV format - good for gdeflate probably (planar)
  3. Fix decreasing fps (initial probe loads 5s of input video - so it's expected)
  4. NotchLC capture shaders with PIX
  */

#define OFFSET(x) offsetof(HapContext, x)
#define FLAGS     AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "format", NULL, OFFSET(opt_tex_fmt), AV_OPT_TYPE_INT, { .i64 = HAP_FMT_RGBDXT1 }, HAP_FMT_RGTC1, HAP_FMT_YCOCGDXT5, FLAGS, "format" },
        { "hap",       "Hap 1 (DXT1 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBDXT1   }, 0, 0, FLAGS, "format" },
        { "hap_alpha", "Hap Alpha (DXT5 textures)", 0, AV_OPT_TYPE_CONST, { .i64 = HAP_FMT_RGBADXT5  }, 0, 0, FLAGS, "format" },
        { "hap_q",     "Hap Q (DXT5-YCoCg textures)", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_FMT_YCOCGDXT5 }, 0, 0, FLAGS, "format" },
        { "hap_r",     "Hap R (BC7 textures)", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_FMT_BPTC }, 0, 0, FLAGS, "format" },
		{ "hap_h",     "Hap HDR (BC6H textures)", 0, AV_OPT_TYPE_CONST, {.i64 = HAP_FMT_BPTC_FU }, 0, 0, FLAGS, "format" },
	{ "chunks", "chunk count", OFFSET(opt_chunk_count), AV_OPT_TYPE_INT, {.i64 = 1 }, -1, HAP_SNAPPY_MAX_CHUNKS, FLAGS, },
	{ "gdeflate_level", "GDeflate compression level", OFFSET(opt_gdeflate_level), AV_OPT_TYPE_INT, {.i64 = GDeflateMinimumCompressionLevel }, GDeflateMinimumCompressionLevel, GDeflateMaximumCompressionLevel, FLAGS, },
	{ "texture_quality_hap_r", "Texture encoding quality for Hap R (0=prefer speed, 1=balanced, 2=prefer quality)", OFFSET(opt_texture_quality_hap_r), AV_OPT_TYPE_INT, {.i64 = 0 }, 0, 2, FLAGS, },
	{ "texture_quality_hap_h", "Texture encoding quality for Hap H (0=prefer speed, 1=prefer quality)", OFFSET(opt_texture_quality_hap_h), AV_OPT_TYPE_INT, {.i64 = 1 }, 0, 1, FLAGS, },
	{ "hap_h_normalization_factor", "Hap H input normalization factor (0.0 = use 1.0 for EXR input and 255.0 for RGBA input)", OFFSET(opt_hap_h_normalization_factor), AV_OPT_TYPE_FLOAT, {.dbl = 0.0 }, -FLT_MAX, FLT_MAX, FLAGS, },
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

const FFCodec ff_hap_encoder = {
    .p.name         = "hap",
    CODEC_LONG_NAME("Vidvox Hap"),
    .p.type         = AVMEDIA_TYPE_VIDEO,
    .p.id           = AV_CODEC_ID_HAP,
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_FRAME_THREADS | AV_CODEC_CAP_INTRA_ONLY,
    .priv_data_size = sizeof(HapContext),
    .p.priv_class   = &hapenc_class,
    .init           = hap_init,
    FF_CODEC_ENCODE_CB(hap_encode),
    .close          = hap_close,
    .p.pix_fmts     = (const enum AVPixelFormat[]) {
        AV_PIX_FMT_RGBA, AV_PIX_FMT_GBRAPF32, AV_PIX_FMT_NONE,
    },
    .caps_internal  = FF_CODEC_CAP_INIT_CLEANUP,
};

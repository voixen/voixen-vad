/*
 * Stripped-down MPEG Audio Decoder based on libmpg123.  
 *
 * Initially written by Michael Hipp, see also AUTHORS and README.
 *  
 *      Copyright (c) 1999-2000 Mark Taylor
 *      Copyright (c) 2003 Olcios
 *      Copyright (c) 2008 Robert Hegemann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * MPEG audio decoder interface.
 * Created by Patrick Levin <pal@voixen.com>
 */
#include <assert.h>
#define hip_global_struct mpstr_tag
#include "mpadec.h" 
#include "mpadec_internal.h"

#define HIP_SIGNATURE	0xDEADC0DE

int hip_decode_init(hip_t hip)
{
	if (hip == NULL) {
		return sizeof(hip_global_flags);
	}

    InitMP3(hip);

	((PMPSTR)hip)->signature = HIP_SIGNATURE;

    return 0;
}

int hip_decode_exit(hip_t hip)
{
    if (hip) {
        ExitMP3(hip);
    }

    return 0;
}

int hip_validate(hip_t hip)
{
	return hip ? (((PMPSTR)hip)->signature - HIP_SIGNATURE) : 0;
}

/* copy mono samples */
#define COPY_MONO(DST_TYPE, SRC_TYPE)                                                           \
    DST_TYPE *pcm_l = (DST_TYPE *)pcm_l_raw;                                                    \
    SRC_TYPE const *p_samples = (SRC_TYPE const *)p;                                            \
    for (i = 0; i < processed_samples; i++)                                                     \
      *pcm_l++ = (DST_TYPE)(*p_samples++);

/* copy stereo samples */
#define COPY_STEREO(DST_TYPE, SRC_TYPE)                                                         \
    DST_TYPE *pcm_l = (DST_TYPE *)pcm_l_raw, *pcm_r = (DST_TYPE *)pcm_r_raw;                    \
    SRC_TYPE const *p_samples = (SRC_TYPE const *)p;                                            \
    for (i = 0; i < processed_samples; i++) {                                                   \
      *pcm_l++ = (DST_TYPE)(*p_samples++);                                                      \
      *pcm_r++ = (DST_TYPE)(*p_samples++);                                                      \
    }

/*
 * For lame_decode:  return code
 * -1     error
 *  0     ok, but need more data before outputing any samples
 *  n     number of samples output.  either 576 or 1152 depending on MP3 file.
 */
static int
decode1_headersB_clipchoice(PMPSTR pmp, unsigned char *buffer, int len,
                            char pcm_l_raw[], char pcm_r_raw[], mp3data_struct * mp3data,
                            int *enc_delay, int *enc_padding,
                            char *p, size_t psize, int decoded_sample_size,
                            int (*decodeMP3_ptr) (PMPSTR, unsigned char *, int, char *, int,
                            int *))
{
    static const int smpls[2][4] = {
        /* Layer   I    II   III */
        {0, 384, 1152, 1152}, /* MPEG-1     */
        {0, 384, 1152, 576} /* MPEG-2(.5) */
    };

    int     processed_bytes;
    int     processed_samples; /* processed samples per channel */
    int     ret;
    int     i;

    mp3data->header_parsed = 0;

    ret = (*decodeMP3_ptr) (pmp, buffer, len, p, (int) psize, &processed_bytes);
    /* three cases:  
     * 1. headers parsed, but data not complete
     *       pmp->header_parsed==1 
     *       pmp->framesize=0           
     *       pmp->fsizeold=size of last frame, or 0 if this is first frame
     *
     * 2. headers, data parsed, but ancillary data not complete
     *       pmp->header_parsed==1 
     *       pmp->framesize=size of frame           
     *       pmp->fsizeold=size of last frame, or 0 if this is first frame
     *
     * 3. frame fully decoded:  
     *       pmp->header_parsed==0 
     *       pmp->framesize=0           
     *       pmp->fsizeold=size of frame (which is now the last frame)
     *
     */
    if (pmp->header_parsed || pmp->fsizeold > 0 || pmp->framesize > 0) {
        mp3data->header_parsed = 1;
        mp3data->stereo = pmp->fr.stereo;
        mp3data->samplerate = freqs[pmp->fr.sampling_frequency];
        mp3data->mode = pmp->fr.mode;
        mp3data->mode_ext = pmp->fr.mode_ext;
        mp3data->framesize = smpls[pmp->fr.lsf][pmp->fr.lay];
		mp3data->layer = pmp->fr.lay;
		mp3data->version = pmp->fr.lsf + pmp->fr.mpeg25;

        /* free format, we need the entire frame before we can determine
         * the bitrate.  If we haven't gotten the entire frame, bitrate=0 */
        if (pmp->fsizeold > 0) /* works for free format and fixed, no overrun, temporal results are < 400.e6 */
            mp3data->bitrate = 8 * (4 + pmp->fsizeold) * mp3data->samplerate /
                (1.e3 * mp3data->framesize) + 0.5;
        else if (pmp->framesize > 0)
            mp3data->bitrate = 8 * (4 + pmp->framesize) * mp3data->samplerate /
                (1.e3 * mp3data->framesize) + 0.5;
        else
            mp3data->bitrate = tabsel_123[pmp->fr.lsf][pmp->fr.lay - 1][pmp->fr.bitrate_index];



        if (pmp->num_frames > 0) {
            /* Xing VBR header found and num_frames was set */
            mp3data->totalframes = pmp->num_frames;
            mp3data->nsamp = mp3data->framesize * pmp->num_frames;
            *enc_delay = pmp->enc_delay;
            *enc_padding = pmp->enc_padding;
        }
    }

    switch (ret) {
    case MP3_OK:
        switch (pmp->fr.stereo) {
        case 1:
            processed_samples = processed_bytes / decoded_sample_size;
            if (decoded_sample_size == sizeof(short)) {
                COPY_MONO(short, short)
            }
            else {
                COPY_MONO(sample_t, sample_t)
            }
            break;
        case 2:
            processed_samples = (processed_bytes / decoded_sample_size) >> 1;
            if (decoded_sample_size == sizeof(short)) {
                COPY_STEREO(short, short)
            }
            else {
                COPY_STEREO(sample_t, sample_t)
            }
            break;
        default:
            processed_samples = -1;
            assert(0);
            break;
        }
        break;

    case MP3_NEED_MORE:
        processed_samples = 0;
        break;

    case MP3_ERR:
        processed_samples = -1;
        break;

    default:
        processed_samples = -1;
        assert(0);
        break;
    }

    return processed_samples;
}

#define OUTSIZE_CLIPPED   (4096*sizeof(short))

/* we forbid input with more than 1152 samples per channel for output in the unclipped mode */
#define OUTSIZE_UNCLIPPED (1152*2*sizeof(sample_t))

int
hip_decode1_unclipped(hip_t hip, unsigned char *buffer, size_t len, sample_t pcm_l[], sample_t pcm_r[])
{
    mp3data_struct mp3data;
    return hip_decode1_headers_unclipped(hip, buffer, len, pcm_l, pcm_r, &mp3data);
}

int
hip_decode1_headers_unclipped(hip_t hip, unsigned char *buffer,
								size_t len, sample_t pcm_l[], sample_t pcm_r[], mp3data_struct * mp3data)
{
    static char out[OUTSIZE_UNCLIPPED];
    int     enc_delay, enc_padding;

    if (hip) {
        return decode1_headersB_clipchoice(hip, buffer, len, (char *) pcm_l, (char *) pcm_r, mp3data,
                                           &enc_delay, &enc_padding, out, OUTSIZE_UNCLIPPED,
                                           sizeof(sample_t), decodeMP3_unclipped);
    }
    return 0;
}

/*
 * For hip_decode:  return code
 *  -1     error
 *   0     ok, but need more data before outputing any samples
 *   n     number of samples output.  Will be at most one frame of
 *         MPEG data.  
 */

int
hip_decode1_headers(hip_t hip, unsigned char *buffer,
                     size_t len, short pcm_l[], short pcm_r[], mp3data_struct * mp3data)
{
    int     enc_delay, enc_padding;
    return hip_decode1_headersB(hip, buffer, len, pcm_l, pcm_r, mp3data, &enc_delay, &enc_padding);
}


int
hip_decode1(hip_t hip, unsigned char *buffer, size_t len, short pcm_l[], short pcm_r[])
{
    mp3data_struct mp3data;
    return hip_decode1_headers(hip, buffer, len, pcm_l, pcm_r, &mp3data);
}


/*
 * For hip_decode:  return code
 *  -1     error
 *   0     ok, but need more data before outputing any samples
 *   n     number of samples output.  a multiple of 576 or 1152 depending on MP3 file.
 */

int
hip_decode_headers(hip_t hip, unsigned char *buffer,
                    size_t len, short pcm_l[], short pcm_r[], mp3data_struct * mp3data)
{
    int     ret;
    int     totsize = 0;     /* number of decoded samples per channel */

    for (;;) {
        switch (ret = hip_decode1_headers(hip, buffer, len, pcm_l + totsize, pcm_r + totsize, mp3data)) {
        case -1:
            return ret;
        case 0:
            return totsize;
        default:
            totsize += ret;
            len = 0;    /* future calls to decodeMP3 are just to flush buffers */
            break;
        }
    }
}


int
hip_decode(hip_t hip, unsigned char *buffer, size_t len, short pcm_l[], short pcm_r[])
{
    mp3data_struct mp3data;
    return hip_decode_headers(hip, buffer, len, pcm_l, pcm_r, &mp3data);
}


int
hip_decode1_headersB(hip_t hip, unsigned char *buffer,
                      size_t len,
                      short pcm_l[], short pcm_r[], mp3data_struct * mp3data,
                      int *enc_delay, int *enc_padding)
{
    static char out[OUTSIZE_CLIPPED];
    if (hip) {
        return decode1_headersB_clipchoice(hip, buffer, len, (char *) pcm_l, (char *) pcm_r, mp3data,
                                           enc_delay, enc_padding, out, OUTSIZE_CLIPPED,
                                           sizeof(short), decodeMP3);
    }
    return -1;
}


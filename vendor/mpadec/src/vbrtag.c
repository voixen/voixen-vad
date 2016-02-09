/*
 * Stripped-down MPEG Audio Decoder based on libmpg123.  
 *
 * Initially written by Michael Hipp, see also AUTHORS and README.
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
 * VBR Tag-detection; removed all writing operations and added type
 * annotations to indicate the deviating return value convetion.
 * Created by Patrick Levin <pal@voixen.com>
 */
#include <stddef.h>
#include "mpadec_internal.h"

static const char VBRTag0[] = { "Xing" };
static const char VBRTag1[] = { "Info" };

const int bitrate_table[3][16] = {
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, -1}, /* MPEG 2 */
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1}, /* MPEG 1 */
    {0, 8, 16, 24, 32, 40, 48, 56, 64, -1, -1, -1, -1, -1, -1, -1}, /* MPEG 2.5 */
};

const int samplerate_table[3][4] = {
    {22050, 24000, 16000, -1}, /* MPEG 2 */
    {44100, 48000, 32000, -1}, /* MPEG 1 */
    {11025, 12000, 8000, -1}, /* MPEG 2.5 */
};

/*-------------------------------------------------------------*/
static int
ExtractI4(const unsigned char *buf)
{
    int     x;
    /* big endian extract */
    x = buf[0];
    x <<= 8;
    x |= buf[1];
    x <<= 8;
    x |= buf[2];
    x <<= 8;
    x |= buf[3];
    return x;
}

/* check for magic strings*/
static BOOL
IsVbrTag(const unsigned char *buf)
{
    int     isTag0, isTag1;

    isTag0 = ((buf[0] == VBRTag0[0]) && (buf[1] == VBRTag0[1]) && (buf[2] == VBRTag0[2])
              && (buf[3] == VBRTag0[3]));
    isTag1 = ((buf[0] == VBRTag1[0]) && (buf[1] == VBRTag1[1]) && (buf[2] == VBRTag1[2])
              && (buf[3] == VBRTag1[3]));

    return AS_BOOL(isTag0 || isTag1);
}

BOOL
GetVbrTag(VBRTAGDATA * pTagData, const unsigned char *buf)
{
    int     i, head_flags;
    int     h_bitrate, h_id, h_mode, h_sr_index, h_layer;
    int     enc_delay, enc_padding;

    /* get Vbr header data */
    pTagData->flags = 0;

    /* get selected MPEG header data */
    h_layer = (buf[1] >> 1) & 3;
    if ( h_layer != 0x01 ) {
        /* the following code assumes Layer-3, so give up here */
        return FALSE;
    }
    h_id = (buf[1] >> 3) & 1;
    h_sr_index = (buf[2] >> 2) & 3;
    h_mode = (buf[3] >> 6) & 3;
    h_bitrate = ((buf[2] >> 4) & 0xf);
    h_bitrate = bitrate_table[h_id][h_bitrate];

    /* check for FFE syncword */
    if ((buf[1] >> 4) == 0xE)
        pTagData->samprate = samplerate_table[2][h_sr_index];
    else
        pTagData->samprate = samplerate_table[h_id][h_sr_index];

    /*  determine offset of header */
    if (h_id) {
        /* mpeg1 */
        if (h_mode != 3)
            buf += (32 + 4);
        else
            buf += (17 + 4);
    }
    else {
        /* mpeg2 */
        if (h_mode != 3)
            buf += (17 + 4);
        else
            buf += (9 + 4);
    }

    if (!IsVbrTag(buf))
        return FALSE;

    buf += 4;

    pTagData->h_id = h_id;

    head_flags = pTagData->flags = ExtractI4(buf);
    buf += 4;           /* get flags */

    if (head_flags & FRAMES_FLAG) {
        pTagData->frames = ExtractI4(buf);
        buf += 4;
    }

    if (head_flags & BYTES_FLAG) {
        pTagData->bytes = ExtractI4(buf);
        buf += 4;
    }

    if (head_flags & TOC_FLAG) {
        if (pTagData->toc != NULL) {
            for (i = 0; i < NUMTOCENTRIES; i++)
                pTagData->toc[i] = buf[i];
        }
        buf += NUMTOCENTRIES;
    }

    pTagData->vbr_scale = -1;

    if (head_flags & VBR_SCALE_FLAG) {
        pTagData->vbr_scale = ExtractI4(buf);
        buf += 4;
    }

    pTagData->headersize = ((h_id + 1) * 72000 * h_bitrate) / pTagData->samprate;

    buf += 21;
    enc_delay = buf[0] << 4;
    enc_delay += buf[1] >> 4;
    enc_padding = (buf[1] & 0x0F) << 8;
    enc_padding += buf[2];
    /* check for reasonable values (this may be an old Xing header, */
    /* not a INFO tag) */
    if (enc_delay < 0 || enc_delay > 3000)
        enc_delay = -1;
    if (enc_padding < 0 || enc_padding > 3000)
        enc_padding = -1;

    pTagData->enc_delay = enc_delay;
    pTagData->enc_padding = enc_padding;

    return TRUE;           /* success */
}


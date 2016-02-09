/*
 * bitstream.c: some common bitstream operations
 *
 * Copyright (C) 1999-2010 The L.A.M.E. project
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
 */
#include <assert.h>
#include <memory.h>
#include <stddef.h>
#include "mpadec_internal.h"

    /* *INDENT-OFF* */
const int tabsel_123 [2] [3] [16] = {
   { {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,},
     {0,32,48,56, 64, 80, 96,112,128,160,192,224,256,320,384,},
     {0,32,40,48, 56, 64, 80, 96,112,128,160,192,224,256,320,} },

   { {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,},
     {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,} }
};

const long freqs[9] = { 44100, 48000, 32000,
                        22050, 24000, 16000,
                        11025, 12000,  8000 };

    /* *INDENT-ON* */

real    muls[27][64];

#define HDRCMPMASK 0xfffffd00

#define MAX_INPUT_FRAMESIZE 4096

BOOL
head_check(unsigned long head, int check_layer)
{
    /*
       look for a valid header.  
       if check_layer > 0, then require that
       nLayer = check_layer.  
     */

    /* bits 13-14 = layer 3 */
    int     nLayer = 4 - ((head >> 17) & 3);

    if ((head & 0xffe00000) != 0xffe00000) {
        /* syncword */
        return FALSE;
    }

    if (nLayer == 4)
        return FALSE;

    if (check_layer > 0 && nLayer != check_layer)
        return FALSE;

    if (((head >> 12) & 0xf) == 0xf) {
        /* bits 16,17,18,19 = 1111  invalid bitrate */
        return FALSE;
    }
    if (((head >> 10) & 0x3) == 0x3) {
        /* bits 20,21 = 11  invalid sampling freq */
        return FALSE;
    }
    if ((head & 0x3) == 0x2)
        /* invalid emphasis */
        return FALSE;
    return TRUE;
}

/*
 * decode a header and write the information
 * into the frame structure
 */
int
decode_header(PMPSTR mp, struct frame *fr, unsigned long newhead)
{
    if (newhead & (1 << 20)) {
        fr->lsf = (newhead & (1 << 19)) ? 0x0 : 0x1;
        fr->mpeg25 = 0;
    }
    else {
        fr->lsf = 1;
        fr->mpeg25 = 1;
    }


    fr->lay = 4 - ((newhead >> 17) & 3);
    if (((newhead >> 10) & 0x3) == 0x3) {
        assert(0 && "hip: Stream error");
    }
    if (fr->mpeg25) {
        fr->sampling_frequency = 6 + ((newhead >> 10) & 0x3);
    }
    else
        fr->sampling_frequency = ((newhead >> 10) & 0x3) + (fr->lsf * 3);

    fr->error_protection = ((newhead >> 16) & 0x1) ^ 0x1;

    if (fr->mpeg25)     /* allow Bitrate change for 2.5 ... */
        fr->bitrate_index = ((newhead >> 12) & 0xf);

    fr->bitrate_index = ((newhead >> 12) & 0xf);
    fr->padding = ((newhead >> 9) & 0x1);
    fr->extension = ((newhead >> 8) & 0x1);
    fr->mode = ((newhead >> 6) & 0x3);
    fr->mode_ext = ((newhead >> 4) & 0x3);
    fr->copyright = ((newhead >> 3) & 0x1);
    fr->original = ((newhead >> 2) & 0x1);
    fr->emphasis = newhead & 0x3;

    fr->stereo = (fr->mode == MPG_MD_MONO) ? 1 : 2;

    switch (fr->lay) {
    case 1:
        fr->framesize = (long) tabsel_123[fr->lsf][0][fr->bitrate_index] * 12000;
        fr->framesize /= freqs[fr->sampling_frequency];
        fr->framesize = ((fr->framesize + fr->padding) << 2) - 4;
        fr->down_sample = 0;
        fr->down_sample_sblimit = SBLIMIT >> (fr->down_sample);
        break;

    case 2:
        fr->framesize = (long) tabsel_123[fr->lsf][1][fr->bitrate_index] * 144000;
        fr->framesize /= freqs[fr->sampling_frequency];
        fr->framesize += fr->padding - 4;
        fr->down_sample = 0;
        fr->down_sample_sblimit = SBLIMIT >> (fr->down_sample);
        break;

    case 3:
        if (fr->framesize > MAX_INPUT_FRAMESIZE) {
            fr->framesize = MAX_INPUT_FRAMESIZE;
            return (0);
        }


        if (fr->bitrate_index == 0)
            fr->framesize = 0;
        else {
            fr->framesize = (long) tabsel_123[fr->lsf][2][fr->bitrate_index] * 144000;
            fr->framesize /= freqs[fr->sampling_frequency] << (fr->lsf);
            fr->framesize = fr->framesize + fr->padding - 4;
        }
        break;
    default:
        return (0);
    }

    return 1;
}


unsigned int
getbits(PMPSTR mp, int number_of_bits)
{
    unsigned long rval;

    if (number_of_bits <= 0 || !mp->wordpointer)
        return 0;

    {
        rval = mp->wordpointer[0];
        rval <<= 8;
        rval |= mp->wordpointer[1];
        rval <<= 8;
        rval |= mp->wordpointer[2];
        rval <<= mp->bitindex;
        rval &= 0xffffff;

        mp->bitindex += number_of_bits;

        rval >>= (24 - number_of_bits);

        mp->wordpointer += (mp->bitindex >> 3);
        mp->bitindex &= 7;
    }
    return rval;
}

unsigned int
getbits_fast(PMPSTR mp, int number_of_bits)
{
    unsigned long rval;

    {
        rval = mp->wordpointer[0];
        rval <<= 8;
        rval |= mp->wordpointer[1];
        rval <<= mp->bitindex;
        rval &= 0xffff;
        mp->bitindex += number_of_bits;

        rval >>= (16 - number_of_bits);

        mp->wordpointer += (mp->bitindex >> 3);
        mp->bitindex &= 7;
    }
    return rval;
}

unsigned char
get_leq_8_bits(PMPSTR mp, unsigned int number_of_bits)
{
    assert(number_of_bits <= 8);
    return (unsigned char) getbits_fast(mp, number_of_bits);
}

unsigned short
get_leq_16_bits(PMPSTR mp, unsigned int number_of_bits)
{
    assert(number_of_bits <= 16);
    return (unsigned short) getbits_fast(mp, number_of_bits);
}

int
set_pointer(PMPSTR mp, long backstep)
{
    unsigned char *bsbufold;

    if (mp->fsizeold < 0 && backstep > 0) {
        return MP3_ERR;
    }
    bsbufold = mp->bsspace[1 - mp->bsnum] + 512;
    mp->wordpointer -= backstep;
    if (backstep)
        memcpy(mp->wordpointer, bsbufold + mp->fsizeold - backstep, (size_t) backstep);
    mp->bitindex = 0;
    return MP3_OK;
}

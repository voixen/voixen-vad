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
 * Internal MPEG audio decoder functions and structures.
 * Created by Patrick Levin <pal@voixen.com>
 */

#ifndef MP3CODEC_INTERNAL_H
#define MP3CODEC_INTERNAL_H

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2    1.41421356237309504880
#endif

#ifndef FALSE
#define         FALSE                   0
#endif
#ifndef TRUE
#define         TRUE                    1
#endif

#ifndef BOOL
#define			BOOL					int
#endif

#define AS_BOOL(expr) (!!(expr)) 

#define real 	float

#define         SBLIMIT                 32
#define         SSLIMIT                 18

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

#define MAXFRAMESIZE 2880

/* AF: ADDED FOR LAYER1/LAYER2 */
#define         SCALE_BLOCK             12

/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

typedef real sample_t;

struct al_table2 {
    short   bits;
    short   d;
};

struct frame {
    int     stereo;
    int     single;          /* single channel (monophonic) */
    int     lsf;             /* 0 = MPEG-1, 1 = MPEG-2/2.5 */
    int     mpeg25;          /* 1 = MPEG-2.5, 0 = MPEG-1/2 */
    int     header_change;
    int     lay;             /* Layer */
    int     error_protection; /* 1 = CRC-16 code following header */
    int     bitrate_index;
    int     sampling_frequency; /* sample rate of decompressed audio in Hz */
    int     padding;
    int     extension;
    int     mode;			/* 00 = Stereo, 01 = Joint Stereo, 10 = Dual Mono, 11 = Mono */
    int     mode_ext;		/* Layer I&II: subband selection (4,8,12,16), Layer III: bit 0 = Intensity Stereo on/off, bit 1 = MS stereo on/off */ 
    int     copyright;
    int     original;
    int     emphasis;
    int     framesize;       /* computed framesize */

    /* AF: ADDED FOR LAYER1/LAYER2 */
    int     II_sblimit;
    struct al_table2 const *alloc;
    int     down_sample_sblimit;
    int     down_sample;
};

struct gr_info_s {
    int     scfsi;
    unsigned part2_3_length;
    unsigned big_values;
    unsigned scalefac_compress;
    unsigned block_type;
    unsigned mixed_block_flag;
    unsigned table_select[3];
    unsigned subblock_gain[3];
    unsigned maxband[3];
    unsigned maxbandl;
    unsigned maxb;
    unsigned region1start;
    unsigned region2start;
    unsigned preflag;
    unsigned scalefac_scale;
    unsigned count1table_select;
    real   *full_gain[3];
    real   *pow2gain;
};

struct III_sideinfo {
    unsigned main_data_begin;
    unsigned private_bits;
    struct {
        struct gr_info_s gr[2];
    } ch[2];
};

/* -----------------------------------------------------------
 * A Vbr header may be present in the ancillary
 * data field of the first frame of an mp3 bitstream
 * The Vbr header (optionally) contains
 *      frames      total number of audio frames in the bitstream
 *      bytes       total number of bytes in the bitstream
 *      toc         table of contents
 *
 * toc (table of contents) gives seek points
 * for random access
 * the ith entry determines the seek point for
 * i-percent duration
 * seek point in bytes = (toc[i]/256.0) * total_bitstream_bytes
 * e.g. half duration seek point = (toc[50]/256.0) * total_bitstream_bytes
 */

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008

#define NUMTOCENTRIES 100

/*structure to receive extracted header */
/* toc may be NULL*/
typedef struct {
    int     h_id;            /* from MPEG header, 0=MPEG2, 1=MPEG1 */
    int     samprate;        /* determined from MPEG header */
    int     flags;           /* from Vbr header data */
    int     frames;          /* total bit stream frames from Vbr header data */
    int     bytes;           /* total bit stream bytes from Vbr header data */
    int     vbr_scale;       /* encoded vbr scale from Vbr header data */
    unsigned char toc[NUMTOCENTRIES]; /* may be NULL if toc not desired */
    int     headersize;      /* size of VBR header, in bytes */
    int     enc_delay;       /* encoder delay */
    int     enc_padding;     /* encoder paddign added at end of stream */
} VBRTAGDATA;

struct buf {
    unsigned char *pnt;
    long    size;
    long    pos;
    struct buf *next;
    struct buf *prev;
};

struct framebuf {
    struct buf *buf;
    long    pos;
    struct frame *next;
    struct frame *prev;
};

/* TODO: replace dynamic linked list with local buffer - worst case buffer size
		 is 9 frames look-ahead for MPEG Layer III bit reservoirs */
typedef struct mpstr_tag {
    struct buf *head, *tail; /* buffer linked list pointers, tail points to oldest buffer */
    int     vbr_header;      /* 1 if valid Xing vbr header detected */
    int     num_frames;      /* set if vbr header present */
    int     enc_delay;       /* set if vbr header present */
    int     enc_padding;     /* set if vbr header present */
    /* header_parsed, side_parsed and data_parsed must be all set 1
       before the full frame has been parsed */
    int     header_parsed;   /* 1 = header of current frame has been parsed */
    int     side_parsed;     /* 1 = header of sideinfo of current frame has been parsed */
    int     data_parsed;
    int     free_format;     /* 1 = free format frame */
    int     old_free_format; /* 1 = last frame was free format */
    int     bsize;
    int     framesize;
    int     ssize;           /* number of bytes used for side information, including 2 bytes for CRC-16 if present */
    int     dsize;
    int     fsizeold;        /* size of previous frame, -1 for first */
    int     fsizeold_nopadding;
    struct frame fr;         /* holds the parameters decoded from the header */
    struct III_sideinfo sideinfo;
    unsigned char bsspace[2][MAXFRAMESIZE + 1024]; /* bit stream space used ???? */ /* MAXFRAMESIZE */
    real    hybrid_block[2][2][SBLIMIT * SSLIMIT];
    int     hybrid_blc[2];
    unsigned long header;
    int     bsnum;
    real    synth_buffs[2][2][0x110];
    int     synth_bo;
    int     sync_bitstream;  /* 1 = bitstream is yet to be synchronized */

    int     bitindex;
    unsigned char *wordpointer;
	int		signature;		/* client signature for heap corruption detection */ 
} MPSTR, *PMPSTR;

#define MP3_ERR -1
#define MP3_OK  0
#define MP3_NEED_MORE 1

/* bitstream vars */
extern const int tabsel_123[2][3][16];
extern const long freqs[9];
extern real muls[27][64];

/* tabinit vars */
extern real decwin[512 + 32];
extern real *pnts[5];

/* common protos */
BOOL    head_check(unsigned long head, int check_layer);
int     decode_header(PMPSTR mp, struct frame *fr, unsigned long newhead);
unsigned int getbits(PMPSTR mp, int number_of_bits);
unsigned int getbits_fast(PMPSTR mp, int number_of_bits);
unsigned char get_leq_8_bits(PMPSTR mp, unsigned int number_of_bits);
unsigned short get_leq_16_bits(PMPSTR mp, unsigned int number_of_bits);
int     set_pointer(PMPSTR mp, long backstep);

/* tabinit protos */
void    make_decode_tables(long scale);

/* decode protos */
int     synth_1to1_mono(PMPSTR mp, real * bandPtr, unsigned char *out, int *pnt);
int     synth_1to1(PMPSTR mp, real * bandPtr, int channel, unsigned char *out, int *pnt);

int     synth_1to1_mono_unclipped(PMPSTR mp, real * bandPtr, unsigned char *out, int *pnt);
int     synth_1to1_unclipped(PMPSTR mp, real * bandPtr, int channel, unsigned char *out, int *pnt);

/* dct64 protos */
void    dct64(real * a, real * b, real * c);

/* layer1 protos */
void    hip_init_tables_layer1(void);
int     decode_layer1_sideinfo(PMPSTR mp);
int     decode_layer1_frame(PMPSTR mp, unsigned char *pcm_sample, int *pcm_point);

/* layer2 protos */
void    hip_init_tables_layer2(void);
int     decode_layer2_sideinfo(PMPSTR mp);
int     decode_layer2_frame(PMPSTR mp, unsigned char *pcm_sample, int *pcm_point);

/* layer3 protos */
void    hip_init_tables_layer3(void);
int     decode_layer3_sideinfo(PMPSTR mp);
int     decode_layer3_frame(PMPSTR mp, unsigned char *pcm_sample, int *pcm_point,
                  int (*synth_1to1_mono_ptr) (PMPSTR, real *, unsigned char *, int *),
                  int (*synth_1to1_ptr) (PMPSTR, real *, int, unsigned char *, int *));

/* vbrtag protos */
BOOL	GetVbrTag(VBRTAGDATA * pTagData, const unsigned char *buf);

/* mpadec protos */

int     InitMP3(PMPSTR mp);
int     decodeMP3(PMPSTR mp, unsigned char *inmemory, int inmemsize, char *outmemory,
                  int outmemsize, int *done);
void    ExitMP3(PMPSTR mp);
/* added decodeMP3_unclipped to support returning raw floating-point values of samples. The representation
of the floating-point numbers is defined in mpadec_internal.h as #define real. It is 32-bit float by default. 
No more than 1152 samples per channel are allowed. */
int     decodeMP3_unclipped(PMPSTR mp, unsigned char *inmemory, int inmemsize, char *outmemory,
                            int outmemsize, int *done);
#endif


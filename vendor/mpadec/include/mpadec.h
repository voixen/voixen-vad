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
 * Public MPEG audio decoder interface.
 * Created by Patrick Levin <pal@voixen.com>
 */

#ifndef MPADEC_H
#define MPADEC_H

/* for size_t typedef */
#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(WIN32) || defined(_WIN32)
#undef CDECL
#define CDECL __cdecl
#else
#define CDECL
#endif

struct hip_global_struct;
typedef struct hip_global_struct hip_global_flags;
typedef hip_global_flags *hip_t; 

#define MPA_MODE_STEREO           0	/* MPA frame type stereo */
#define MPA_MODE_JOINT_STEREO     1	/* MPA frame type joint stereo */
#define MPA_MODE_DUAL_CHANNEL     2	/* MPA frame type dual channel (2 independent mono channels) */
#define MPA_MODE_MONO             3	/* MPA frame type monaural (single channel, left only) */

#define MPA_VERSION_MPEG1		  0 /* MPEG1 frame version 			   */
#define MPA_VERSION_MPEG2		  1	/* MPEG2 frame version			   */
#define MPA_VERSION_MPEG25		  2	/* MPEG2.5 frame version 		   */

#define MPA_LAYER_I				  1 /* MPEG Audio Layer I 			   */
#define MPA_LAYER_II			  2 /* MPEG Audio Layer II 			   */
#define MPA_LAYER_III			  3 /* MPEG Audio Layer III 		   */

/*
 * Frequency sub-bands can be applied separately for each frame. 	
 * In case the psycho-accoustic model decides to increase the energy of
 * a given sub-band, the selected frequency bank is stored in the mode
 * extension bits of the encoded frame.
 *
 * Sub-band selection allows for better dynamics in the decoded audio. 
 */

#define MPA_SUBBAND_4_31		  0 /* MPEG Layer I,II subband 4..31   */
#define MPA_SUBBAND_8_31		  1 /* MPEG Layer I,II subband 8..31   */
#define MPA_SUBBAND_12_31		  2 /* MPEG Layer I,II subband 12..31  */
#define MPA_SUBBAND_16_31		  3 /* MPEG Layer I,II subband 16..31  */

/*
 *	Joint Stereo reduces the bit rate by merging some frequency ranges  
 *	from multiple channels before quantisation. This results in a loss
 *	of channel separation for these bands.
 *
 *	Intensity stereo is a psycho-accoustic model that uses a merged 
 *	low frequency band channel while preserving side channel information
 *  for panning cues.
 *
 *  M/S or mid- & side channel uses the combinded M = L+R mid channel for
 *  quantisation and keeps difference S = L - R as side channel data.
 *  M/S stereo is therefore non-destructive in nature, as the coding is
 *  a reversible, bijective transformation => L=(M+S)/2 and R=(M-S)/2.
 *
 *  Depending on the encoder, both or none of the modes can be active
 *  in any given Joint Stereo frame. Typically encoders switch between
 *  M/S and full L/R stereo modes.
 */

#define MPA_STEREO_MODE_OFF		  0 /* MPEG Layer III full stereo  	   */  
#define MPA_STEREO_MODE_INTESNITY 1 /* MPEG Layer III intensity stereo */
#define MPA_STEREO_MODE_MS_STEREO 2	/* MPEG Layer III m/s stereo 	   */
#define MPA_STEREO_MODE_BOTH	  3 /* MPEG Layer III m/s + intensity  */

/*
 *	MPEG audio frame information. 
 *
 *	Can be obtained via hip_decode_headers(), hip_decode1_headers()
 *	and hip_decode1_headersB().
 *
 *	This information will be parsed for each audio frame.
 */
typedef struct _mp3data_struct {
  int header_parsed;   /* 1 if header was parsed and following data was
                          computed                                       */
  int version;		   /* MPEG frame version (MPA_VERSION_XXX)			 */
  int layer;		   /* MPEG audio layer (MPA_LAYER_XXX)				 */
  int stereo;          /* number of channels                             */
  int samplerate;      /* sample rate in Hz                              */
  int bitrate;         /* bitrate in kilobits per second                 */
  int mode;            /* mp3 frame type (MPA_MODE_xxxx)                 */
  int mode_ext;        /* Subband (MPA_SUBBAND_XXX) or stereo mode		   
						  (MPA_STEREO_MODE_XXX) if Layer III Joint Stereo*/
  int framesize;       /* number of SAMPLES per mp3 frame                */

  /* this data is only computed if mpglib detects a Xing VBR header */
  unsigned long nsamp; /* number of SAMPLES in mp3 file.                 */
  int totalframes;     /* total number of frames in mp3 file             */
} mp3data_struct;

/*********************************************************************
 * Initialise the MPEG Audio decoder library.
 *
 *  res = hip_decode_init(gfp);
 *
 * input:
 *    gfp          : Memory buffer for the decoder state or NULL
 *
 * output:
 *    res :  -1    : Initialisation error
 *            0    : Initialsiation succeeded
 *           >0    : Size of the decoder state in bytes, if 'hip'
 *					 was NULL
 *
 * Decoder state is managed by the client (e.g. NodeJS) in order to
 * make sure GC and MT can be supported without changes to the
 * underlying library.
 *
 * Typical usage:
 * hit_t hip;
 * required = hip_decode_init(NULL);
 * hip = (hip) <allocate 'required' bytes>;
 * error = hip_decode_init(hip);
 *********************************************************************/
int CDECL hip_decode_init(hip_t gfp);

/*********************************************************************
 * Clean-up the MPEG Audio decoder library state.
 *
 *  res = hip_decode_exit(gfp);
 *
 * input:
 *    gfp          : Memory buffer for the decoder state or NULL
 *
 * output:
 *    res : <>0    : Invalid argument (e.g. not a decoder state)
 *            0    : Clean-up successfully finished
 *
 * Decoder state memory is managed by the client (e.g. NodeJS), so
 * no client memory is free'd. Clients should call the method before
 * the state buffer is GC'd or becomes otherwise invalid.
 *
 *********************************************************************/
int CDECL hip_decode_exit(hip_t gfp);

/*********************************************************************
 * Validate the MPEG Audio decoder library state.
 *
 *  res = hip_validate(gfp);
 *
 * input:
 *    gfp          : Memory buffer for a valid decoder state
 *
 * output:
 *    res : <>0    : Invalid argument (e.g. not a decoder state)
 *            0    : Decoder state seems valid
 *
 * Use this function to prevent garbage from being passed to the API.
 * Please note that
 * - the validation method is by no means bullet proof and only
 *   capable of detecting obviously invalid or corrupted decoder states
 * - the API itself does NOT call this method; its use is up to the 
 *   client's discretion
 * - NULL is considered to be a "valid" decoder state, as it can be
 *	 passed safely to every API function 
 *********************************************************************/
int CDECL hip_validate(hip_t gfp);

/*********************************************************************
 * Utility macro that resets the decoder state.
 * This is useful for seeking (especially in VBR files) and for
 * forced re-sync (e.g. to an interrupted HTTP-stream).   
 *********************************************************************/
#define hip_decode_reset	hip_decode_init 

/*********************************************************************
 * Input one or more mp3 frame(s), output (maybe) pcm data.
 *
 *  nout = hip_decode(hip, mp3buf,len,pcm_l,pcm_r);
 *
 * input:
 *    len          :  Number of bytes of mp3 data in mp3buf
 *    mp3buf[len]  :  mp3 data to be decoded
 *
 * output:
 *    nout:  -1    : Decoding error
 *            0    : Need more data before we can complete the decode
 *           >0    : Returned 'nout' samples worth of data in pcm_l,pcm_r
 *    pcm_l[nout]  : left channel data or single monaural channel data
 *    pcm_r[nout]  : right channel data
 *
 *********************************************************************/
int CDECL hip_decode( hip_t           gfp
                    , unsigned char * mp3buf
                    , size_t          len
                    , short           pcm_l[]
                    , short           pcm_r[]
                    );

/*********************************************************************
 * Same as hip_decode, and also returns mp3 header data.
 *
 *********************************************************************/
int CDECL hip_decode_headers( hip_t           gfp
                            , unsigned char*  mp3buf
                            , size_t          len
                            , short           pcm_l[]
                            , short           pcm_r[]
                            , mp3data_struct* mp3data
                            );

/*********************************************************************
 * Same as hip_decode, but returns at most one frame.
 *
 * In order to get all decoded frames from the passed input data,
 * call the function again, until no more samples are returned:
 * 
 * int nsamples = 0;
 * do {
 *		nsamples = hip_decode1(hip, mp3, len, left, right);
 *		if (nsamples) {
 *			... flush left & right ...
 *		}
 *		len = 0; ... subsequent calls will only flush buffers
 * } while (nsamples > 0);
 *
 * Up to three subsequent calls might be required for re-sync
 * until sample data is returned (init, read frame, decode frame).
 *********************************************************************/
int CDECL hip_decode1( hip_t          gfp
                     , unsigned char* mp3buf
                     , size_t         len
                     , short          pcm_l[]
                     , short          pcm_r[]
                     );

/*********************************************************************
 * Same as hip_decode1, but returns at most one frame and mp3 header data.
 *
 *********************************************************************/
int CDECL hip_decode1_headers( hip_t           gfp
                             , unsigned char*  mp3buf
                             , size_t          len
                             , short           pcm_l[]
                             , short           pcm_r[]
                             , mp3data_struct* mp3data
                             );

/*********************************************************************
 * Same as hip_decode1, but also returns enc_delay and enc_padding
 * from a VBR info tag (both are set to -1 if no tag was found)
 *********************************************************************/
int CDECL hip_decode1_headersB( hip_t gfp
                              , unsigned char*   mp3buf
                              , size_t           len
                              , short            pcm_l[]
                              , short            pcm_r[]
                              , mp3data_struct*  mp3data
                              , int             *enc_delay
                              , int             *enc_padding
                              );

/*********************************************************************
 * Same as hip_decode1, but returns float data.
 *
 *********************************************************************/
int CDECL hip_decode1_unclipped(hip_t gfp,
								unsigned char *buffer,
								size_t len,
								float pcm_l[],
								float pcm_r[]);


/*********************************************************************
 * Same as hip_decode1_unclipped, but also returns mp3 header data.
 *
 *********************************************************************/
int CDECL hip_decode1_headers_unclipped(hip_t gfp,
										unsigned char *buffer,
										size_t len,
										float pcm_l[],
										float pcm_r[],
										mp3data_struct*  mp3data);

#if defined(__cplusplus)
}
#endif
#endif /* MPAADEC_H */


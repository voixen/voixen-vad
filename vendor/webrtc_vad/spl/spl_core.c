/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <assert.h>
#include <string.h>
#include "include/signal_processing_library.h"

/* begin file: auto_correlation.c */

size_t WebRtcSpl_AutoCorrelation(const int16_t* in_vector,
                                 size_t in_vector_length,
                                 size_t order,
                                 int32_t* result,
                                 int* scale) {
  int32_t sum = 0;
  size_t i = 0, j = 0;
  int16_t smax = 0;
  int scaling = 0;

  assert(order <= in_vector_length);

  // Find the maximum absolute value of the samples.
  smax = WebRtcSpl_MaxAbsValueW16(in_vector, in_vector_length);

  // In order to avoid overflow when computing the sum we should scale the
  // samples so that (in_vector_length * smax * smax) will not overflow.
  if (smax == 0) {
    scaling = 0;
  } else {
    // Number of bits in the sum loop.
    int nbits = WebRtcSpl_GetSizeInBits((uint32_t)in_vector_length);
    // Number of bits to normalize smax.
    int t = WebRtcSpl_NormW32(WEBRTC_SPL_MUL(smax, smax));

    if (t > nbits) {
      scaling = 0;
    } else {
      scaling = nbits - t;
    }
  }

  // Perform the actual correlation calculation.
  for (i = 0; i < order + 1; i++) {
    sum = 0;
    /* Unroll the loop to improve performance. */
    for (j = 0; i + j + 3 < in_vector_length; j += 4) {
      sum += (in_vector[j + 0] * in_vector[i + j + 0]) >> scaling;
      sum += (in_vector[j + 1] * in_vector[i + j + 1]) >> scaling;
      sum += (in_vector[j + 2] * in_vector[i + j + 2]) >> scaling;
      sum += (in_vector[j + 3] * in_vector[i + j + 3]) >> scaling;
    }
    for (; j < in_vector_length - i; j++) {
      sum += (in_vector[j] * in_vector[i + j]) >> scaling;
    }
    *result++ = sum;
  }

  *scale = scaling;
  return order + 1;
}

/* end file: auto_correlation.c */
/* begin file: auto_corr_to_refl_coef.c */

void WebRtcSpl_AutoCorrToReflCoef(const int32_t *R, int use_order, int16_t *K)
{
    int i, n;
    int16_t tmp;
    const int32_t *rptr;
    int32_t L_num, L_den;
    int16_t *acfptr, *pptr, *wptr, *p1ptr, *w1ptr, ACF[WEBRTC_SPL_MAX_LPC_ORDER],
            P[WEBRTC_SPL_MAX_LPC_ORDER], W[WEBRTC_SPL_MAX_LPC_ORDER];

    // Initialize loop and pointers.
    acfptr = ACF;
    rptr = R;
    pptr = P;
    p1ptr = &P[1];
    w1ptr = &W[1];
    wptr = w1ptr;

    // First loop; n=0. Determine shifting.
    tmp = WebRtcSpl_NormW32(*R);
    *acfptr = (int16_t)((*rptr++ << tmp) >> 16);
    *pptr++ = *acfptr++;

    // Initialize ACF, P and W.
    for (i = 1; i <= use_order; i++)
    {
        *acfptr = (int16_t)((*rptr++ << tmp) >> 16);
        *wptr++ = *acfptr;
        *pptr++ = *acfptr++;
    }

    // Compute reflection coefficients.
    for (n = 1; n <= use_order; n++, K++)
    {
        tmp = WEBRTC_SPL_ABS_W16(*p1ptr);
        if (*P < tmp)
        {
            for (i = n; i <= use_order; i++)
                *K++ = 0;

            return;
        }

        // Division: WebRtcSpl_div(tmp, *P)
        *K = 0;
        if (tmp != 0)
        {
            L_num = tmp;
            L_den = *P;
            i = 15;
            while (i--)
            {
                (*K) <<= 1;
                L_num <<= 1;
                if (L_num >= L_den)
                {
                    L_num -= L_den;
                    (*K)++;
                }
            }
            if (*p1ptr > 0)
                *K = -*K;
        }

        // Last iteration; don't do Schur recursion.
        if (n == use_order)
            return;

        // Schur recursion.
        pptr = P;
        wptr = w1ptr;
        tmp = (int16_t)(((int32_t)*p1ptr * (int32_t)*K + 16384) >> 15);
        *pptr = WebRtcSpl_AddSatW16(*pptr, tmp);
        pptr++;
        for (i = 1; i <= use_order - n; i++)
        {
            tmp = (int16_t)(((int32_t)*wptr * (int32_t)*K + 16384) >> 15);
            *pptr = WebRtcSpl_AddSatW16(*(pptr + 1), tmp);
            pptr++;
            tmp = (int16_t)(((int32_t)*pptr * (int32_t)*K + 16384) >> 15);
            *wptr = WebRtcSpl_AddSatW16(*wptr, tmp);
            wptr++;
        }
    }
}

/* end file: auto_corr_to_refl_coef.c */
/* begin file: complex_bit_reverse.c */

/* Tables for data buffer indexes that are bit reversed and thus need to be
 * swapped. Note that, index_7[{0, 2, 4, ...}] are for the left side of the swap
 * operations, while index_7[{1, 3, 5, ...}] are for the right side of the
 * operation. Same for index_8.
 */

/* Indexes for the case of stages == 7. */
static const int16_t index_7[112] = {
  1, 64, 2, 32, 3, 96, 4, 16, 5, 80, 6, 48, 7, 112, 9, 72, 10, 40, 11, 104,
  12, 24, 13, 88, 14, 56, 15, 120, 17, 68, 18, 36, 19, 100, 21, 84, 22, 52,
  23, 116, 25, 76, 26, 44, 27, 108, 29, 92, 30, 60, 31, 124, 33, 66, 35, 98,
  37, 82, 38, 50, 39, 114, 41, 74, 43, 106, 45, 90, 46, 58, 47, 122, 49, 70,
  51, 102, 53, 86, 55, 118, 57, 78, 59, 110, 61, 94, 63, 126, 67, 97, 69,
  81, 71, 113, 75, 105, 77, 89, 79, 121, 83, 101, 87, 117, 91, 109, 95, 125,
  103, 115, 111, 123
};

/* Indexes for the case of stages == 8. */
static const int16_t index_8[240] = {
  1, 128, 2, 64, 3, 192, 4, 32, 5, 160, 6, 96, 7, 224, 8, 16, 9, 144, 10, 80,
  11, 208, 12, 48, 13, 176, 14, 112, 15, 240, 17, 136, 18, 72, 19, 200, 20,
  40, 21, 168, 22, 104, 23, 232, 25, 152, 26, 88, 27, 216, 28, 56, 29, 184,
  30, 120, 31, 248, 33, 132, 34, 68, 35, 196, 37, 164, 38, 100, 39, 228, 41,
  148, 42, 84, 43, 212, 44, 52, 45, 180, 46, 116, 47, 244, 49, 140, 50, 76,
  51, 204, 53, 172, 54, 108, 55, 236, 57, 156, 58, 92, 59, 220, 61, 188, 62,
  124, 63, 252, 65, 130, 67, 194, 69, 162, 70, 98, 71, 226, 73, 146, 74, 82,
  75, 210, 77, 178, 78, 114, 79, 242, 81, 138, 83, 202, 85, 170, 86, 106, 87,
  234, 89, 154, 91, 218, 93, 186, 94, 122, 95, 250, 97, 134, 99, 198, 101,
  166, 103, 230, 105, 150, 107, 214, 109, 182, 110, 118, 111, 246, 113, 142,
  115, 206, 117, 174, 119, 238, 121, 158, 123, 222, 125, 190, 127, 254, 131,
  193, 133, 161, 135, 225, 137, 145, 139, 209, 141, 177, 143, 241, 147, 201,
  149, 169, 151, 233, 155, 217, 157, 185, 159, 249, 163, 197, 167, 229, 171,
  213, 173, 181, 175, 245, 179, 205, 183, 237, 187, 221, 191, 253, 199, 227,
  203, 211, 207, 243, 215, 235, 223, 251, 239, 247
};

void WebRtcSpl_ComplexBitReverse(int16_t* __restrict complex_data, int stages) {
  /* For any specific value of stages, we know exactly the indexes that are
   * bit reversed. Currently (Feb. 2012) in WebRTC the only possible values of
   * stages are 7 and 8, so we use tables to save unnecessary iterations and
   * calculations for these two cases.
   */
  if (stages == 7 || stages == 8) {
    int m = 0;
    int length = 112;
    const int16_t* index = index_7;

    if (stages == 8) {
      length = 240;
      index = index_8;
    }

    /* Decimation in time. Swap the elements with bit-reversed indexes. */
    for (m = 0; m < length; m += 2) {
      /* We declare a int32_t* type pointer, to load both the 16-bit real
       * and imaginary elements from complex_data in one instruction, reducing
       * complexity.
       */
      int32_t* complex_data_ptr = (int32_t*)complex_data;
      int32_t temp = 0;

      temp = complex_data_ptr[index[m]];  /* Real and imaginary */
      complex_data_ptr[index[m]] = complex_data_ptr[index[m + 1]];
      complex_data_ptr[index[m + 1]] = temp;
    }
  }
  else {
    int m = 0, mr = 0, l = 0;
    int n = 1 << stages;
    int nn = n - 1;

    /* Decimation in time - re-order data */
    for (m = 1; m <= nn; ++m) {
      int32_t* complex_data_ptr = (int32_t*)complex_data;
      int32_t temp = 0;

      /* Find out indexes that are bit-reversed. */
      l = n;
      do {
        l >>= 1;
      } while (l > nn - mr);
      mr = (mr & (l - 1)) + l;

      if (mr <= m) {
        continue;
      }

      /* Swap the elements with bit-reversed indexes.
       * This is similar to the loop in the stages == 7 or 8 cases.
       */
      temp = complex_data_ptr[m];  /* Real and imaginary */
      complex_data_ptr[m] = complex_data_ptr[mr];
      complex_data_ptr[mr] = temp;
    }
  }
}

/* end file: complex_bit_reverse.c */
/* begin file: copy_set_operations.c */

void WebRtcSpl_MemSetW16(int16_t *ptr, int16_t set_value, size_t length)
{
    size_t j;
    int16_t *arrptr = ptr;

    for (j = length; j > 0; j--)
    {
        *arrptr++ = set_value;
    }
}

void WebRtcSpl_MemSetW32(int32_t *ptr, int32_t set_value, size_t length)
{
    size_t j;
    int32_t *arrptr = ptr;

    for (j = length; j > 0; j--)
    {
        *arrptr++ = set_value;
    }
}

void WebRtcSpl_MemCpyReversedOrder(int16_t* dest,
                                   int16_t* source,
                                   size_t length)
{
    size_t j;
    int16_t* destPtr = dest;
    int16_t* sourcePtr = source;

    for (j = 0; j < length; j++)
    {
        *destPtr-- = *sourcePtr++;
    }
}

void WebRtcSpl_CopyFromEndW16(const int16_t *vector_in,
                              size_t length,
                              size_t samples,
                              int16_t *vector_out)
{
    // Copy the last <samples> of the input vector to vector_out
    WEBRTC_SPL_MEMCPY_W16(vector_out, &vector_in[length - samples], samples);
}

void WebRtcSpl_ZerosArrayW16(int16_t *vector, size_t length)
{
    WebRtcSpl_MemSetW16(vector, 0, length);
}

void WebRtcSpl_ZerosArrayW32(int32_t *vector, size_t length)
{
    WebRtcSpl_MemSetW32(vector, 0, length);
}

/* end file: copy_set_operations.c */
/* begin file: cross_correlation.c */
/* C version of WebRtcSpl_CrossCorrelation() for generic platforms. */
void WebRtcSpl_CrossCorrelationC(int32_t* cross_correlation,
                                 const int16_t* seq1,
                                 const int16_t* seq2,
                                 size_t dim_seq,
                                 size_t dim_cross_correlation,
                                 int right_shifts,
                                 int step_seq2) {
  size_t i = 0, j = 0;

  for (i = 0; i < dim_cross_correlation; i++) {
    int32_t corr = 0;
    for (j = 0; j < dim_seq; j++)
      corr += (seq1[j] * seq2[j]) >> right_shifts;
    seq2 += step_seq2;
    *cross_correlation++ = corr;
  }
}
/* end file: cross_correlation.c */
/* begin file: division_operations.c */

uint32_t WebRtcSpl_DivU32U16(uint32_t num, uint16_t den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return (uint32_t)(num / den);
    } else
    {
        return (uint32_t)0xFFFFFFFF;
    }
}

int32_t WebRtcSpl_DivW32W16(int32_t num, int16_t den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return (int32_t)(num / den);
    } else
    {
        return (int32_t)0x7FFFFFFF;
    }
}

int16_t WebRtcSpl_DivW32W16ResW16(int32_t num, int16_t den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return (int16_t)(num / den);
    } else
    {
        return (int16_t)0x7FFF;
    }
}

int32_t WebRtcSpl_DivResultInQ31(int32_t num, int32_t den)
{
    int32_t L_num = num;
    int32_t L_den = den;
    int32_t div = 0;
    int k = 31;
    int change_sign = 0;

    if (num == 0)
        return 0;

    if (num < 0)
    {
        change_sign++;
        L_num = -num;
    }
    if (den < 0)
    {
        change_sign++;
        L_den = -den;
    }
    while (k--)
    {
        div <<= 1;
        L_num <<= 1;
        if (L_num >= L_den)
        {
            L_num -= L_den;
            div++;
        }
    }
    if (change_sign == 1)
    {
        div = -div;
    }
    return div;
}

int32_t WebRtcSpl_DivW32HiLow(int32_t num, int16_t den_hi, int16_t den_low)
{
    int16_t approx, tmp_hi, tmp_low, num_hi, num_low;
    int32_t tmpW32;

    approx = (int16_t)WebRtcSpl_DivW32W16((int32_t)0x1FFFFFFF, den_hi);
    // result in Q14 (Note: 3FFFFFFF = 0.5 in Q30)

    // tmpW32 = 1/den = approx * (2.0 - den * approx) (in Q30)
    tmpW32 = (den_hi * approx << 1) + ((den_low * approx >> 15) << 1);
    // tmpW32 = den * approx

    tmpW32 = (int32_t)0x7fffffffL - tmpW32; // result in Q30 (tmpW32 = 2.0-(den*approx))

    // Store tmpW32 in hi and low format
    tmp_hi = (int16_t)(tmpW32 >> 16);
    tmp_low = (int16_t)((tmpW32 - ((int32_t)tmp_hi << 16)) >> 1);

    // tmpW32 = 1/den in Q29
    tmpW32 = (tmp_hi * approx + (tmp_low * approx >> 15)) << 1;

    // 1/den in hi and low format
    tmp_hi = (int16_t)(tmpW32 >> 16);
    tmp_low = (int16_t)((tmpW32 - ((int32_t)tmp_hi << 16)) >> 1);

    // Store num in hi and low format
    num_hi = (int16_t)(num >> 16);
    num_low = (int16_t)((num - ((int32_t)num_hi << 16)) >> 1);

    // num * (1/den) by 32 bit multiplication (result in Q28)

    tmpW32 = num_hi * tmp_hi + (num_hi * tmp_low >> 15) +
        (num_low * tmp_hi >> 15);

    // Put result in Q31 (convert from Q28)
    tmpW32 = WEBRTC_SPL_LSHIFT_W32(tmpW32, 3);

    return tmpW32;
}
/* end file: division_operations.c */
/* begin file: dot_product_with_scale.c */
int32_t WebRtcSpl_DotProductWithScale(const int16_t* vector1,
                                      const int16_t* vector2,
                                      size_t length,
                                      int scaling) {
  int32_t sum = 0;
  size_t i = 0;

  /* Unroll the loop to improve performance. */
  for (i = 0; i + 3 < length; i += 4) {
    sum += (vector1[i + 0] * vector2[i + 0]) >> scaling;
    sum += (vector1[i + 1] * vector2[i + 1]) >> scaling;
    sum += (vector1[i + 2] * vector2[i + 2]) >> scaling;
    sum += (vector1[i + 3] * vector2[i + 3]) >> scaling;
  }
  for (; i < length; i++) {
    sum += (vector1[i] * vector2[i]) >> scaling;
  }

  return sum;
}
/* end file: dot_product_with_scale.c */
/* begin file: downsample_fast.c */
// TODO(Bjornv): Change the function parameter order to WebRTC code style.
// C version of WebRtcSpl_DownsampleFast() for generic platforms.
int WebRtcSpl_DownsampleFastC(const int16_t* data_in,
                              size_t data_in_length,
                              int16_t* data_out,
                              size_t data_out_length,
                              const int16_t* __restrict coefficients,
                              size_t coefficients_length,
                              int factor,
                              size_t delay) {
  size_t i = 0;
  size_t j = 0;
  int32_t out_s32 = 0;
  size_t endpos = delay + factor * (data_out_length - 1) + 1;

  // Return error if any of the running conditions doesn't meet.
  if (data_out_length == 0 || coefficients_length == 0
                           || data_in_length < endpos) {
    return -1;
  }

  for (i = delay; i < endpos; i += factor) {
    out_s32 = 2048;  // Round value, 0.5 in Q12.

    for (j = 0; j < coefficients_length; j++) {
      out_s32 += coefficients[j] * data_in[i - j];  // Q12.
    }

    out_s32 >>= 12;  // Q0.

    // Saturate and store the output.
    *data_out++ = WebRtcSpl_SatW32ToW16(out_s32);
  }

  return 0;
}
/* end file: downsample_fast.c */
/* begin file: energy.c */
int32_t WebRtcSpl_Energy(int16_t* vector,
                         size_t vector_length,
                         int* scale_factor)
{
    int32_t en = 0;
    size_t i;
    int scaling =
        WebRtcSpl_GetScalingSquare(vector, vector_length, vector_length);
    size_t looptimes = vector_length;
    int16_t *vectorptr = vector;

    for (i = 0; i < looptimes; i++)
    {
      en += (*vectorptr * *vectorptr) >> scaling;
      vectorptr++;
    }
    *scale_factor = scaling;

    return en;
}
/* end file: energy.c */
/* begin file: filter_ar.c */
size_t WebRtcSpl_FilterAR(const int16_t* a,
                          size_t a_length,
                          const int16_t* x,
                          size_t x_length,
                          int16_t* state,
                          size_t state_length,
                          int16_t* state_low,
                          size_t state_low_length,
                          int16_t* filtered,
                          int16_t* filtered_low,
                          size_t filtered_low_length)
{
    int32_t o;
    int32_t oLOW;
    size_t i, j, stop;
    const int16_t* x_ptr = &x[0];
    int16_t* filteredFINAL_ptr = filtered;
    int16_t* filteredFINAL_LOW_ptr = filtered_low;

    for (i = 0; i < x_length; i++)
    {
        // Calculate filtered[i] and filtered_low[i]
        const int16_t* a_ptr = &a[1];
        int16_t* filtered_ptr = &filtered[i - 1];
        int16_t* filtered_low_ptr = &filtered_low[i - 1];
        int16_t* state_ptr = &state[state_length - 1];
        int16_t* state_low_ptr = &state_low[state_length - 1];

        o = (int32_t)(*x_ptr++) << 12;
        oLOW = (int32_t)0;

        stop = (i < a_length) ? i + 1 : a_length;
        for (j = 1; j < stop; j++)
        {
          o -= *a_ptr * *filtered_ptr--;
          oLOW -= *a_ptr++ * *filtered_low_ptr--;
        }
        for (j = i + 1; j < a_length; j++)
        {
          o -= *a_ptr * *state_ptr--;
          oLOW -= *a_ptr++ * *state_low_ptr--;
        }

        o += (oLOW >> 12);
        *filteredFINAL_ptr = (int16_t)((o + (int32_t)2048) >> 12);
        *filteredFINAL_LOW_ptr++ = (int16_t)(o - ((int32_t)(*filteredFINAL_ptr++)
                << 12));
    }

    // Save the filter state
    if (x_length >= state_length)
    {
        WebRtcSpl_CopyFromEndW16(filtered, x_length, a_length - 1, state);
        WebRtcSpl_CopyFromEndW16(filtered_low, x_length, a_length - 1, state_low);
    } else
    {
        for (i = 0; i < state_length - x_length; i++)
        {
            state[i] = state[i + x_length];
            state_low[i] = state_low[i + x_length];
        }
        for (i = 0; i < x_length; i++)
        {
            state[state_length - x_length + i] = filtered[i];
            state[state_length - x_length + i] = filtered_low[i];
        }
    }

    return x_length;
}
/* end file: filter_ar.c */
/* begin file: filter_ar_fast_q12.c */
// TODO(bjornv): Change the return type to report errors.

void WebRtcSpl_FilterARFastQ12(const int16_t* data_in,
                               int16_t* data_out,
                               const int16_t* __restrict coefficients,
                               size_t coefficients_length,
                               size_t data_length) {
  size_t i = 0;
  size_t j = 0;

  assert(data_length > 0);
  assert(coefficients_length > 1);

  for (i = 0; i < data_length; i++) {
    int32_t output = 0;
    int32_t sum = 0;

    for (j = coefficients_length - 1; j > 0; j--) {
      sum += coefficients[j] * data_out[i - j];
    }

    output = coefficients[0] * data_in[i];
    output -= sum;

    // Saturate and store the output.
    output = WEBRTC_SPL_SAT(134215679, output, -134217728);
    data_out[i] = (int16_t)((output + 2048) >> 12);
  }
}
/* end file: filter_ar_fast_q12.c */
/* begin file: filter_ma_fast_q12.c */
void WebRtcSpl_FilterMAFastQ12(const int16_t* in_ptr,
                               int16_t* out_ptr,
                               const int16_t* B,
                               size_t B_length,
                               size_t length)
{
    size_t i, j;
    for (i = 0; i < length; i++)
    {
        int32_t o = 0;

        for (j = 0; j < B_length; j++)
        {
          o += B[j] * in_ptr[i - j];
        }

        // If output is higher than 32768, saturate it. Same with negative side
        // 2^27 = 134217728, which corresponds to 32768 in Q12

        // Saturate the output
        o = WEBRTC_SPL_SAT((int32_t)134215679, o, (int32_t)-134217728);

        *out_ptr++ = (int16_t)((o + (int32_t)2048) >> 12);
    }
    return;
}
/* end file: filter_ma_fast_q12.c */
/* begin file: get_hanning_window.c */
// Hanning table with 256 entries
static const int16_t kHanningTable[] = {
    1,      2,      6,     10,     15,     22,     30,     39,
   50,     62,     75,     89,    104,    121,    138,    157,
  178,    199,    222,    246,    271,    297,    324,    353,
  383,    413,    446,    479,    513,    549,    586,    624,
  663,    703,    744,    787,    830,    875,    920,    967,
 1015,   1064,   1114,   1165,   1218,   1271,   1325,   1381,
 1437,   1494,   1553,   1612,   1673,   1734,   1796,   1859,
 1924,   1989,   2055,   2122,   2190,   2259,   2329,   2399,
 2471,   2543,   2617,   2691,   2765,   2841,   2918,   2995,
 3073,   3152,   3232,   3312,   3393,   3475,   3558,   3641,
 3725,   3809,   3895,   3980,   4067,   4154,   4242,   4330,
 4419,   4509,   4599,   4689,   4781,   4872,   4964,   5057,
 5150,   5244,   5338,   5432,   5527,   5622,   5718,   5814,
 5910,   6007,   6104,   6202,   6299,   6397,   6495,   6594,
 6693,   6791,   6891,   6990,   7090,   7189,   7289,   7389,
 7489,   7589,   7690,   7790,   7890,   7991,   8091,   8192,
 8293,   8393,   8494,   8594,   8694,   8795,   8895,   8995,
 9095,   9195,   9294,   9394,   9493,   9593,   9691,   9790,
 9889,   9987,  10085,  10182,  10280,  10377,  10474,  10570,
10666,  10762,  10857,  10952,  11046,  11140,  11234,  11327,
11420,  11512,  11603,  11695,  11785,  11875,  11965,  12054,
12142,  12230,  12317,  12404,  12489,  12575,  12659,  12743,
12826,  12909,  12991,  13072,  13152,  13232,  13311,  13389,
13466,  13543,  13619,  13693,  13767,  13841,  13913,  13985,
14055,  14125,  14194,  14262,  14329,  14395,  14460,  14525,
14588,  14650,  14711,  14772,  14831,  14890,  14947,  15003,
15059,  15113,  15166,  15219,  15270,  15320,  15369,  15417,
15464,  15509,  15554,  15597,  15640,  15681,  15721,  15760,
15798,  15835,  15871,  15905,  15938,  15971,  16001,  16031,
16060,  16087,  16113,  16138,  16162,  16185,  16206,  16227,
16246,  16263,  16280,  16295,  16309,  16322,  16334,  16345,
16354,  16362,  16369,  16374,  16378,  16382,  16383,  16384
};

void WebRtcSpl_GetHanningWindow(int16_t *v, size_t size)
{
    size_t jj;
    int16_t *vptr1;

    int32_t index;
    int32_t factor = ((int32_t)0x40000000);

    factor = WebRtcSpl_DivW32W16(factor, (int16_t)size);
    if (size < 513)
        index = (int32_t)-0x200000;
    else
        index = (int32_t)-0x100000;
    vptr1 = v;

    for (jj = 0; jj < size; jj++)
    {
        index += factor;
        (*vptr1++) = kHanningTable[index >> 22];
    }
}
/* end file: get_hanning_window.c */
/* begin file: get_scaling_square.c */
int16_t WebRtcSpl_GetScalingSquare(int16_t* in_vector,
                                   size_t in_vector_length,
                                   size_t times)
{
    int16_t nbits = WebRtcSpl_GetSizeInBits((uint32_t)times);
    size_t i;
    int16_t smax = -1;
    int16_t sabs;
    int16_t *sptr = in_vector;
    int16_t t;
    size_t looptimes = in_vector_length;

    for (i = looptimes; i > 0; i--)
    {
        sabs = (*sptr > 0 ? *sptr++ : -*sptr++);
        smax = (sabs > smax ? sabs : smax);
    }
    t = WebRtcSpl_NormW32(WEBRTC_SPL_MUL(smax, smax));

    if (smax == 0)
    {
        return 0; // Since norm(0) returns 0
    } else
    {
        return (t > nbits) ? 0 : nbits - t;
    }
}
/* end file: get_scaling_square.c */
/* begin file: lpc_to_refl_coef.c */

#define SPL_LPC_TO_REFL_COEF_MAX_AR_MODEL_ORDER 50

void WebRtcSpl_LpcToReflCoef(int16_t* a16, int use_order, int16_t* k16)
{
    int m, k;
    int32_t tmp32[SPL_LPC_TO_REFL_COEF_MAX_AR_MODEL_ORDER];
    int32_t tmp_inv_denom32;
    int16_t tmp_inv_denom16;

    k16[use_order - 1] = a16[use_order] << 3;  // Q12<<3 => Q15
    for (m = use_order - 1; m > 0; m--)
    {
        // (1 - k^2) in Q30
        tmp_inv_denom32 = 1073741823 - k16[m] * k16[m];
        // (1 - k^2) in Q15
        tmp_inv_denom16 = (int16_t)(tmp_inv_denom32 >> 15);

        for (k = 1; k <= m; k++)
        {
            // tmp[k] = (a[k] - RC[m] * a[m-k+1]) / (1.0 - RC[m]*RC[m]);

            // [Q12<<16 - (Q15*Q12)<<1] = [Q28 - Q28] = Q28
            tmp32[k] = (a16[k] << 16) - (k16[m] * a16[m - k + 1] << 1);

            tmp32[k] = WebRtcSpl_DivW32W16(tmp32[k], tmp_inv_denom16); //Q28/Q15 = Q13
        }

        for (k = 1; k < m; k++)
        {
            a16[k] = (int16_t)(tmp32[k] >> 1);  // Q13>>1 => Q12
        }

        tmp32[m] = WEBRTC_SPL_SAT(8191, tmp32[m], -8191);
        k16[m - 1] = (int16_t)WEBRTC_SPL_LSHIFT_W32(tmp32[m], 2); //Q13<<2 => Q15
    }
}
/* end file: lpc_to_refl_coef.c */
/* begin file: refl_coef_to_lpc.c */
void WebRtcSpl_ReflCoefToLpc(const int16_t *k, int use_order, int16_t *a)
{
    int16_t any[WEBRTC_SPL_MAX_LPC_ORDER + 1];
    int16_t *aptr, *aptr2, *anyptr;
    const int16_t *kptr;
    int m, i;

    kptr = k;
    *a = 4096; // i.e., (Word16_MAX >> 3)+1.
    *any = *a;
    a[1] = *k >> 3;

    for (m = 1; m < use_order; m++)
    {
        kptr++;
        aptr = a;
        aptr++;
        aptr2 = &a[m];
        anyptr = any;
        anyptr++;

        any[m + 1] = *kptr >> 3;
        for (i = 0; i < m; i++)
        {
            *anyptr = *aptr + (int16_t)((*aptr2 * *kptr) >> 15);
            anyptr++;
            aptr++;
            aptr2--;
        }

        aptr = a;
        anyptr = any;
        for (i = 0; i < (m + 2); i++)
        {
            *aptr = *anyptr;
            aptr++;
            anyptr++;
        }
    }
}
/* end file: refl_coef_to_lpc.c */
/* begin file: sqrt_of_one_minus_x_squared.c */
void WebRtcSpl_SqrtOfOneMinusXSquared(int16_t *xQ15, size_t vector_length,
                                      int16_t *yQ15)
{
    int32_t sq;
    size_t m;
    int16_t tmp;

    for (m = 0; m < vector_length; m++)
    {
        tmp = xQ15[m];
        sq = tmp * tmp;  // x^2 in Q30
        sq = 1073741823 - sq; // 1-x^2, where 1 ~= 0.99999999906 is 1073741823 in Q30
        sq = WebRtcSpl_Sqrt(sq); // sqrt(1-x^2) in Q15
        yQ15[m] = (int16_t)sq;
    }
}
/* end file: sqrt_of_one_minus_x_squared.c */
/* begin file: vector_scaling_operations.c */

void WebRtcSpl_VectorBitShiftW16(int16_t *res, size_t length,
                                 const int16_t *in, int16_t right_shifts)
{
    size_t i;

    if (right_shifts > 0)
    {
        for (i = length; i > 0; i--)
        {
            (*res++) = ((*in++) >> right_shifts);
        }
    } else
    {
        for (i = length; i > 0; i--)
        {
            (*res++) = ((*in++) << (-right_shifts));
        }
    }
}

void WebRtcSpl_VectorBitShiftW32(int32_t *out_vector,
                                 size_t vector_length,
                                 const int32_t *in_vector,
                                 int16_t right_shifts)
{
    size_t i;

    if (right_shifts > 0)
    {
        for (i = vector_length; i > 0; i--)
        {
            (*out_vector++) = ((*in_vector++) >> right_shifts);
        }
    } else
    {
        for (i = vector_length; i > 0; i--)
        {
            (*out_vector++) = ((*in_vector++) << (-right_shifts));
        }
    }
}

void WebRtcSpl_VectorBitShiftW32ToW16(int16_t* out, size_t length,
                                      const int32_t* in, int right_shifts) {
  size_t i;
  int32_t tmp_w32;

  if (right_shifts >= 0) {
    for (i = length; i > 0; i--) {
      tmp_w32 = (*in++) >> right_shifts;
      (*out++) = WebRtcSpl_SatW32ToW16(tmp_w32);
    }
  } else {
    int left_shifts = -right_shifts;
    for (i = length; i > 0; i--) {
      tmp_w32 = (*in++) << left_shifts;
      (*out++) = WebRtcSpl_SatW32ToW16(tmp_w32);
    }
  }
}

void WebRtcSpl_ScaleVector(const int16_t *in_vector, int16_t *out_vector,
                           int16_t gain, size_t in_vector_length,
                           int16_t right_shifts)
{
    // Performs vector operation: out_vector = (gain*in_vector)>>right_shifts
    size_t i;
    const int16_t *inptr;
    int16_t *outptr;

    inptr = in_vector;
    outptr = out_vector;

    for (i = 0; i < in_vector_length; i++)
    {
      *outptr++ = (int16_t)((*inptr++ * gain) >> right_shifts);
    }
}

void WebRtcSpl_ScaleVectorWithSat(const int16_t *in_vector, int16_t *out_vector,
                                 int16_t gain, size_t in_vector_length,
                                 int16_t right_shifts)
{
    // Performs vector operation: out_vector = (gain*in_vector)>>right_shifts
    size_t i;
    const int16_t *inptr;
    int16_t *outptr;

    inptr = in_vector;
    outptr = out_vector;

    for (i = 0; i < in_vector_length; i++) {
      *outptr++ = WebRtcSpl_SatW32ToW16((*inptr++ * gain) >> right_shifts);
    }
}

void WebRtcSpl_ScaleAndAddVectors(const int16_t *in1, int16_t gain1, int shift1,
                                  const int16_t *in2, int16_t gain2, int shift2,
                                  int16_t *out, size_t vector_length)
{
    // Performs vector operation: out = (gain1*in1)>>shift1 + (gain2*in2)>>shift2
    size_t i;
    const int16_t *in1ptr;
    const int16_t *in2ptr;
    int16_t *outptr;

    in1ptr = in1;
    in2ptr = in2;
    outptr = out;

    for (i = 0; i < vector_length; i++)
    {
      *outptr++ = (int16_t)((gain1 * *in1ptr++) >> shift1) +
          (int16_t)((gain2 * *in2ptr++) >> shift2);
    }
}

// C version of WebRtcSpl_ScaleAndAddVectorsWithRound() for generic platforms.
int WebRtcSpl_ScaleAndAddVectorsWithRoundC(const int16_t* in_vector1,
                                           int16_t in_vector1_scale,
                                           const int16_t* in_vector2,
                                           int16_t in_vector2_scale,
                                           int right_shifts,
                                           int16_t* out_vector,
                                           size_t length) {
  size_t i = 0;
  int round_value = (1 << right_shifts) >> 1;

  if (in_vector1 == NULL || in_vector2 == NULL || out_vector == NULL ||
      length == 0 || right_shifts < 0) {
    return -1;
  }

  for (i = 0; i < length; i++) {
    out_vector[i] = (int16_t)((
        in_vector1[i] * in_vector1_scale + in_vector2[i] * in_vector2_scale +
        round_value) >> right_shifts);
  }

  return 0;
}
/* end file: vector_scaling_operations.c */


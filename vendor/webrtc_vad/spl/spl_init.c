/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* The global function contained in this file initializes SPL function
 * pointers, currently only for ARM platforms.
 *
 * Some code came from common/rtcd.c in the WebM project.
 */

#include "include/signal_processing_library.h"

/* Declare function pointers. */
MaxAbsValueW16 WebRtcSpl_MaxAbsValueW16;
MaxAbsValueW32 WebRtcSpl_MaxAbsValueW32;
MaxValueW16 WebRtcSpl_MaxValueW16;
MaxValueW32 WebRtcSpl_MaxValueW32;
MinValueW16 WebRtcSpl_MinValueW16;
MinValueW32 WebRtcSpl_MinValueW32;
CrossCorrelation WebRtcSpl_CrossCorrelation;
DownsampleFast WebRtcSpl_DownsampleFast;
ScaleAndAddVectorsWithRound WebRtcSpl_ScaleAndAddVectorsWithRound;

/* Initialize function pointers to the generic C version. */
static void InitPointersToC(void) {
  WebRtcSpl_MaxAbsValueW16 = WebRtcSpl_MaxAbsValueW16C;
  WebRtcSpl_MaxAbsValueW32 = WebRtcSpl_MaxAbsValueW32C;
  WebRtcSpl_MaxValueW16 = WebRtcSpl_MaxValueW16C;
  WebRtcSpl_MaxValueW32 = WebRtcSpl_MaxValueW32C;
  WebRtcSpl_MinValueW16 = WebRtcSpl_MinValueW16C;
  WebRtcSpl_MinValueW32 = WebRtcSpl_MinValueW32C;
  WebRtcSpl_CrossCorrelation = WebRtcSpl_CrossCorrelationC;
  WebRtcSpl_DownsampleFast = WebRtcSpl_DownsampleFastC;
  WebRtcSpl_ScaleAndAddVectorsWithRound =
      WebRtcSpl_ScaleAndAddVectorsWithRoundC;
}

static void InitFunctionPointers(void) {
  InitPointersToC();
}

#if !defined(_WIN32)
#include <pthread.h>

static void once(void (*func)(void)) {
  static pthread_once_t lock = PTHREAD_ONCE_INIT;
  pthread_once(&lock, func);
}

#else
#include <windows.h>

static void once(void (*func)(void)) {
  /* Didn't use InitializeCriticalSection() since there's no race-free context
   * in which to execute it.
   *
   * TODO(kma): Change to different implementation (e.g.
   * InterlockedCompareExchangePointer) to avoid issues similar to
   * http://code.google.com/p/webm/issues/detail?id=467.
   */
  static CRITICAL_SECTION lock = {(void *)((size_t)-1), -1, 0, 0, 0, 0};
  static int done = 0;

  EnterCriticalSection(&lock);
  if (!done) {
    func();
    done = 1;
  }
  LeaveCriticalSection(&lock);
}

/* There's no fallback version as an #else block here to ensure thread safety.
 * In case of neither pthread for WEBRTC_POSIX nor _WIN32 is present, build
 * system should pick it up.
 */
#endif  /* WEBRTC_POSIX */

void WebRtcSpl_Init() {
  once(InitFunctionPointers);
}

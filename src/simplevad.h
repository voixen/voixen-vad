#ifndef SIMPLEVAD_H
#define SIMPLEVAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Opaque VAD system state */
typedef struct _vadstate_t* vad_t; 

/* VAD event types */
typedef enum _vad_event
{
    /* Processing error occured */ 
    VAD_EVENT_ERROR = -1,
    /* Silence detected */
    VAD_EVENT_SILENCE = 0,
    /* Voice detected */
    VAD_EVENT_VOICE = 1,
    /* Noise detected */
    VAD_EVENT_NOISE = 2
} vad_event;

/* VAD detection modes */
typedef enum _vad_mode
{
    /* Normal mode */
    VAD_MODE_NORMAL = 0,
    /* Optimised for low bitrate */
    VAD_MODE_LOW_BITRATE = 1,
    /* Aggressive mode */
    VAD_MODE_AGGRESSIVE = 2,
    /* very aggressive mode */
    VAD_MODE_VERY_AGGRESSIVE = 3
} vad_mode;

/**
 * Allocate the VAD system state 
 * @param mem      Memory for the VAD state - can be NULL
 * @param memSize  Size of the provided memory; will be set
 *                 to the actual used/required memory in bytes
 * @returns Opaque system state, NULL, if no memory was provided
 *          or the given memory size was too low
 */
vad_t    vadAllocate(void* mem, size_t* memSize);

/**
 * Initialise the VAD system
 * @param    state        VAD system state
 * @returns 0 on successs, <0 on error
 */
int      vadInit(vad_t state);

/**
 * Apply detection mode
 * @param    state        VAD system state
 * @param    mode        Detection mode
 * @returns 0 on successs, <0 on error
 */
int      vadSetMode(vad_t state, vad_mode mode);

/**
 * Process audio samples
 * @param state         VAD system state as returned by vadInit()
 * @param samples         Pointer to PCM samples that are to be processed
 * @param num_samples     Total number of samples in the provided buffer
 * @returns Event type for the given samples
 * @remarks
 * The result is the integral of all detected sub-events for the given samples
 */
vad_event  vadProcessAudio(vad_t state, int samplerate, const float* samples, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif

#if defined(VAD_DEBUG)
#include <stdio.h>             /* for printf-debugging */
#endif 
#include <string.h>            /* for memset() */
#include "webrtc_vad.h" 
#include "simplevad.h"

/* calculate the number of samples for an audio frame */
#define CALC_FRAME_SIZE(duration, rate) (((rate) / 1000) * (duration))
/* max. supported sample rate in Hz */
#define MAX_SAMPLERATE                  48000
/* max. supported frame length in ms */
#define MAX_FRAME_LENGTH                30
/* max. possible buffer length */
#define MAX_BUFFER_SIZE                 CALC_FRAME_SIZE(MAX_FRAME_LENGTH, MAX_SAMPLERATE)
/* number of events per call */
#define EVENT_BUFFER_SIZE               16
/* number of unique event types */
#define EVENT_COUNT                     4
/* Map event code to offset */
#define EVENT_OFFSET(event)             ((event) + 1)
/* Select an event from a histogram */
#define SELECT_EVENT(event, histogram)  ((histogram)[EVENT_OFFSET(event)])

#if defined(VAD_DEBUG)
static const char* event_names[3] = { "ERROR", "SILENCE", "VOICE" };
#define NAME(event) event_names[event]
#endif

/* VAD processing state and support structures */
struct _vadstate_t
{
    /* ring buffer for full frames */
    short        frame[MAX_BUFFER_SIZE];
    /* length of a full frame of the given sample rate */
    int          frame_length;
    /* current frame offset (e.g. # of samples in buffer) */
    int          frame_offset;
    /* sample rate */
    int          sample_rate;
    /* handle of the VAD implementation */
    VadInst*     vad;
};

/* Sample iterator */
typedef struct _vad_sample_iterator
{
    short*         buf;    /* frame buffer */
    const float*   ptr;    /* input samples */
    size_t         ofs;    /* offset into frame buffer */
    size_t         len;    /* number of input samples */
    size_t         inc;    /* frame increment in samples */
} vad_sample_iterator;

static void vadFrameBegin(vad_sample_iterator* it, vad_t state, const float* samples, size_t num_samples);
static int  vadFrameNext(vad_sample_iterator* it);
static void vadFrameEnd(vad_t state, vad_sample_iterator* it);
static vad_event vadDecision(const int* histogram);

#define VAD_ADDR(mem) (((char*)(mem)) + sizeof(struct _vadstate_t))
#define CLIP(value)   ((short)(value < -32768 ? 32768 : value > 32767 ? 32767 : value)) 

static int vadInitState(vad_t state, int rate);

vad_t vadAllocate(void* mem, size_t* memSize)
{
    int size = (int)(memSize ? memSize[0] : 0);
    int required = WebRtcVad_CreateUser(NULL, size);
    int result;

    required += sizeof(struct _vadstate_t);

#if defined(VAD_DEBUG)
    printf("[native] vadAllocate mem=%p size=%d\n", mem, memSize ? (int)memSize[0] : -1);
#endif

    if (size < required || !mem)
    {
        if (memSize)
        {
            memSize[0] = required;
        }
        return NULL;
    }

    result = WebRtcVad_CreateUser(VAD_ADDR(mem), size);
    if (!result)
    {
        vad_t state = (vad_t)mem;
        state->sample_rate = 0; 
        state->vad = (VadInst*)VAD_ADDR(mem);

#if defined(VAD_DEBUG)
        printf("[native] vadAllocate OK\n");
#endif
        return (vad_t)(mem);
    }

    if (memSize)
    {
        memSize[0] = result > 0 ? result + sizeof(struct _vadstate_t) : 0;
    }

#if defined(VAD_DEBUG)
    printf("[native] vadAllocate FAILED\n");
#endif

    return NULL;
}

int vadInit(vad_t state)
{
    int result = WebRtcVad_Init(state->vad);

#if defined(VAD_DEBUG)
    printf("[native] vadInit res=%d\n", result);
#endif
 
    return result;
}

int vadSetMode(vad_t state, vad_mode mode)
{
    int result = WebRtcVad_set_mode(state->vad, mode);

#if defined(VAD_DEBUG)
    printf("[native] vadSetMode mode=%d res=%d\n", mode, result);
#endif

    return result;
}

vad_event vadProcessAudio(vad_t state, int samplerate, const float* samples, size_t num_samples)
{
    int                 histogram[EVENT_COUNT];
    vad_sample_iterator it;

    if (!state->sample_rate && vadInitState(state, samplerate)) { return VAD_EVENT_ERROR; }
    else if (state->sample_rate != samplerate) { return VAD_EVENT_ERROR; } /* variable sample rate is not supported */

    memset(histogram, 0, sizeof histogram);

    vadFrameBegin(&it, state, samples, num_samples);
    while (!vadFrameNext(&it)) {
        int event = WebRtcVad_Process(state->vad, samplerate, it.buf, it.inc);
#if defined(VAD_DEBUG)
        printf("[native] vadProcessAudio event=%s\n", NAME(event+1));
#endif
        ++histogram[EVENT_OFFSET(event)];
    }
    vadFrameEnd(state, &it);

    return vadDecision(histogram);
}

static int vadInitState(vad_t state, int rate)
{
    state->sample_rate = rate;
    state->frame_length = CALC_FRAME_SIZE(MAX_FRAME_LENGTH, rate);
    state->frame_offset = 0;
    return !(rate == 8000 || rate == 16000 || rate == 32000 || rate == 48000);
}

static void vadFrameBegin(vad_sample_iterator* it, vad_t state, const float* samples, size_t num_samples)
{
    it->buf = state->frame;
    it->inc = state->frame_length;
    it->len = num_samples;
    it->ptr = samples;
    it->ofs = state->frame_offset;
}

static int vadFrameNext(vad_sample_iterator* it)
{
    size_t i, fill;

    if (it->ofs >= it->inc)
    {
        it->ofs = 0;
        return 0;
    }

    if (it->len == 0) { return 1; }

    for (i = 0, fill = it->inc - it->ofs; i < fill && it->len > 0; ++i, --it->len)
    {
        int sample = *it->ptr++ * 32768;
        it->buf[it->ofs++] = CLIP(sample);
    }

    return it->ofs < it->inc;
}

static void vadFrameEnd(vad_t state, vad_sample_iterator* it)
{
#if defined(VAD_DEBUG)
    printf("[native] vadFrameEnd overhang=%d samples\n", (int)it->ofs);
#endif
    state->frame_offset = it->ofs;
}

static vad_event vadDecision(const int* histogram)
{
    int i, sum, maj;

    for (i = 0, sum = 0; i < EVENT_COUNT; ++i) sum += histogram[i];

    if (sum == 0)
        return VAD_EVENT_SILENCE;      /* not enough data - default to silence */

    maj = (sum * 80) / 100;            /* calculate 80% of grand total */

    if (SELECT_EVENT(VAD_EVENT_ERROR, histogram) > 0)
        return VAD_EVENT_ERROR;        /* something went wrong along the way */

    /* use the 80% rule to decide whether voice is active */
    if (SELECT_EVENT(VAD_EVENT_VOICE, histogram) >= maj)
        return VAD_EVENT_VOICE;

    return VAD_EVENT_SILENCE;
}

#include <algorithm>
#include <nan.h>
#include "mpadec.h"

/**
 * NodeJS bindings for the MPEG audio decoder library.
 *
 * The bindings are defined at a medium API level to support
 * error handling, configuration and data input handling on
 * the Javascript side.
 *
 */
using namespace v8;

using std::transform;

namespace mpa
{

/**
 * Get cached frame info from the decoder state buffer
 * @param handle    Decoder state handle
 * @returns Last decoded frame info (stored just after the decoder state) 
 */
static mp3data_struct* GetFrameInfo(Local<Value> handle)
{
    return reinterpret_cast<mp3data_struct*>(
        node::Buffer::Data(handle) + node::Buffer::Length(handle) - sizeof(mp3data_struct) - sizeof(int));
}

static bool IsNewFrameInfo(const mp3data_struct* current, const mp3data_struct* last)
{
    const size_t SIGNIFICANT_FRAME_INFO_SIZE = 28;
    return current->header_parsed && memcmp(current, last, SIGNIFICANT_FRAME_INFO_SIZE) != 0;
}

/**
 * Create js object from the given frame info (this is expensive - so only do it on demand)
 * @param data    Frame info
 * @returns Frame info wrapped as js object 
 */
static Local<Object> GetFrameInfoObject(const mp3data_struct& data, int bitsPerSample)
{
    static const char* VERSIONS[] = { "MPEG1", "MPEG2", "MPEG2.5" };

    Local<Object> info = Nan::New<Object>();
    const char* version = VERSIONS[data.version % 3];

    Nan::Set(info, Nan::New("bitrate").ToLocalChecked(), Nan::New(data.bitrate));
    Nan::Set(info, Nan::New("channels").ToLocalChecked(), Nan::New(data.stereo));
    Nan::Set(info, Nan::New("samplerate").ToLocalChecked(), Nan::New(data.samplerate));
    Nan::Set(info, Nan::New("bitsPerSample").ToLocalChecked(), Nan::New(bitsPerSample));
    Nan::Set(info, Nan::New("layer").ToLocalChecked(), Nan::New(data.layer));
    Nan::Set(info, Nan::New("version").ToLocalChecked(), Nan::New(version).ToLocalChecked());
    Nan::Set(info, Nan::New("mode").ToLocalChecked(), Nan::New(data.mode));

    return info;
}

// generic decoder function selection
template<typename T>
struct Decoder { static int decode(hip_t, uint8_t*, size_t, T*, T*, mp3data_struct*); };

template<>
int Decoder<int16_t>::decode(hip_t hip, uint8_t* input,
                             size_t length, int16_t* left, int16_t* right, mp3data_struct* data)
{
    return hip_decode1_headers(hip, input, length, left, right, data);
}

// normalise 16 bit output to [-1..+1] - no clipping
inline static float normalise(float f)
{
    return f * (1.0f / 32768.0f);
}

template<>
int Decoder<float>::decode(hip_t hip, uint8_t* input,
                           size_t length, float* left, float* right, mp3data_struct* data)
{
    int ret = hip_decode1_headers_unclipped(hip, input, length, left, right, data);

    if (ret > 0)
    {
        transform(left, left + ret, left, normalise);
        if (data->stereo > 1) {
            transform(right, right + ret, right, normalise);
        }
    }

    return ret;
}

/**
 * Async worker for decoding frames
 */
template<typename T>
class DecodeFrameWorker : public Nan::AsyncWorker
{
public:
    /**
     * It's not safe to access V8 or V8 data structures in the Execute() method,
     * so pass everything the method requires as plain C(++) data into the state.
     */
    DecodeFrameWorker(Nan::Callback* callback, Local<Value> mp,
                      Local<Value> input, Local<Value> left, Local<Value> right, int length)
        : AsyncWorker(callback),
          mp(reinterpret_cast<hip_t>(node::Buffer::Data(mp))),
          input(reinterpret_cast<uint8_t*>(node::Buffer::Data(input))),
          outLeft(reinterpret_cast<T*>(node::Buffer::Data(left))),
          outRight(reinterpret_cast<T*>(node::Buffer::Data(right))),
          length(length), needData(false), isError(false), samplesRead(0),
          lastFrame(GetFrameInfo(mp))
    {
        memset(&data, 0, sizeof data);
        SaveToPersistent(Nan::New("left").ToLocalChecked(), left);
        SaveToPersistent(Nan::New("right").ToLocalChecked(), right);
    }

    ~DecodeFrameWorker() {}

    /**
     * Performs work in a separate thread.
     */
    void Execute()
    {
        if (length > 0)
        {
            StartDecoding();
        }
        else
        {
            samplesRead = Decoder<T>::decode(mp, input, length, outLeft, outRight, &data);
            isError = samplesRead < 0;
            needData = !samplesRead;
        }
    }

    /**
     * Pass the results back to V8.
     */
    void HandleOKCallback()
    {
        Nan::HandleScope scope;
        Local<Value> left = GetFromPersistent("left");
        Local<Value> right = GetFromPersistent("right");
        Local<Object> obj = Nan::New<Object>();

        Nan::Set(obj, Nan::New("sampleCount").ToLocalChecked(), Nan::New(samplesRead));
        Nan::Set(obj, Nan::New("needMoreData").ToLocalChecked(), Nan::New(needData));
        Nan::Set(obj, Nan::New("error").ToLocalChecked(), Nan::New(isError));

        // only pass frame info if the parsed portion differs from the last decode run
        bool emitFrameInfo = IsNewFrameInfo(&data, lastFrame);
        if (emitFrameInfo)
        {
            Nan::Set(obj, Nan::New("frameInfo").ToLocalChecked(), GetFrameInfoObject(data, sizeof(T) * 8));
            *lastFrame = data;                        // cache the updated frame info
            *(int*)(&lastFrame[1]) = sizeof(T) * 8; // set the bits per sample
        }

        // only pass actual data if we have a fully decoded frame
        if (samplesRead)
        {
            Nan::Set(obj, Nan::New("samplesLeft").ToLocalChecked(), left);
            Nan::Set(obj, Nan::New("samplesRight").ToLocalChecked(), right);
        }

        Local<Value> argv[] = {
            Nan::Null(),
            obj
        };

        callback->Call(2, argv); // -> callback(error, result)
    }

private:

    // the API behaves really stupid - the very first frame returns nothing
    // (-> it seeks the first audio frame)
    // the second decode call then gets the first frame header
    // (still returning nothing)
    // and the third decode call finally decodes an actual frame...
    // This method deals with all possible combinations in a sane way.
    void StartDecoding()
    {
        enum e_state { init, head, frame, done } state = init;

        while (state != done)
        {
            switch (state)
            {
            case init:
                samplesRead = Decoder<T>::decode(mp, input, length, outLeft, outRight, &data);
                if (samplesRead) state = done;
                else if (!data.header_parsed) state = head;
                else state = frame;
                break;
            case head:
                samplesRead = Decoder<T>::decode(mp, input, 0, outLeft, outRight, &data);
                if (samplesRead || !data.header_parsed) state = done; // needs more data if samplesRead == 0
                else state = frame;
                break;
            case frame:
                samplesRead = Decoder<T>::decode(mp, input, 0, outLeft, outRight, &data);
                state = done;
                break;
            default:
                assert(0 && "Invalid decoding state");
                break;
            }
        }

        // evaluate final state
        isError = samplesRead < 0;
        needData = (state == done && !samplesRead);
    }

    // all required state goes here
    mp3data_struct  data;
    hip_t           mp;
    uint8_t*        input;
    T*              outLeft;
    T*              outRight;
    int             length;
    bool            needData;
    bool            isError;
    int             samplesRead;
    mp3data_struct* lastFrame;
};

// Wraps hip_decode_init
NAN_METHOD(initDecoder)
{
    if (!node::Buffer::HasInstance(info[0]))
    {
        // we need some extra space to cache the last decoded frame info
        int size = hip_decode_init(NULL) + sizeof(mp3data_struct) + sizeof(int);
        info.GetReturnValue().Set(size);
    }
    else
    { 
        hip_t mp = reinterpret_cast<hip_t>(node::Buffer::Data(info[0]));

        int result = hip_decode_init(mp);
        if (!result)
        {
            memset(GetFrameInfo(info[0]), 0, sizeof(mp3data_struct));
        }

        info.GetReturnValue().Set(result);
    }
}

// Wraps hip_decode_exit
NAN_METHOD(freeDecoder)
{
    if (node::Buffer::HasInstance(info[0]))
    {
        hip_t mp = reinterpret_cast<hip_t>(node::Buffer::Data(info[0]));
        if (hip_validate(mp))
        {
            ThrowException(Exception::TypeError(Nan::New("Invalid decoder state!").ToLocalChecked()));
            return;
        }

        hip_decode_exit(mp);
    }
}

// Async function for frame decoding
template<typename T>
NAN_METHOD(decodeFrame)
{
    Nan::HandleScope scope;

    if (!(node::Buffer::HasInstance(info[0]) && // decoder insance
          node::Buffer::HasInstance(info[1]) && // input buffer
          node::Buffer::HasInstance(info[3]) && // output buffer left channel
          node::Buffer::HasInstance(info[4])))  // output buffer right channel
    {
        ThrowException(Exception::TypeError(Nan::New("Invalid argument").ToLocalChecked()));
        return;
    }

    hip_t mp = reinterpret_cast<hip_t>(node::Buffer::Data(info[0]));
    if (hip_validate(mp))
    {
        ThrowException(Exception::TypeError(Nan::New("Invalid decoder state!").ToLocalChecked()));
        return;
    }

    int length = Nan::To<int>(info[2]).FromJust();
    Nan::Callback* callback = new Nan::Callback(info[5].As<Function>());

    DecodeFrameWorker<T>* worker = new DecodeFrameWorker<T>(callback, info[0], info[1], info[3], info[4], length);
    Nan::AsyncQueueWorker(worker);
}

// Query the most recent frame info
NAN_METHOD(getLastFrameInfo)
{
    Nan::HandleScope scope;

    if (!node::Buffer::HasInstance(info[0]))
    {
        info.GetReturnValue().Set(Nan::Undefined());
        return;
    }

    hip_t mp = reinterpret_cast<hip_t>(node::Buffer::Data(info[0]));
    if (hip_validate(mp))
    {
        ThrowException(Exception::TypeError(Nan::New("Invalid decoder state!").ToLocalChecked()));
        return;
    }

    const mp3data_struct* data = GetFrameInfo(info[0]);
    int bpp = *((int*)&data[1]);
    if (!memchr(data, 0, sizeof *data))
    {
        info.GetReturnValue().Set(Nan::Undefined());
    }
    else
    {
        info.GetReturnValue().Set(GetFrameInfoObject(*data, bpp));
    }
}

// Setup the native exports
NAN_MODULE_INIT(init)
{
    const int MP3_FRAME_SIZE = 1152;
    target->Set(String::NewSymbol("MPA_INPUT_BUFFER_SIZE"), Nan::New(4096),    // nicely align to page
        static_cast<PropertyAttribute>(ReadOnly|DontDelete));
    target->Set(String::NewSymbol("MPA_SAMPLE_BUFFER_SIZE"),
        Nan::New(static_cast<int>(MP3_FRAME_SIZE * sizeof(short))),
        static_cast<PropertyAttribute>(ReadOnly|DontDelete));
    target->Set(String::NewSymbol("MPA_FLOAT_BUFFER_SIZE"),
        Nan::New(static_cast<int>(MP3_FRAME_SIZE * sizeof(float))),
        static_cast<PropertyAttribute>(ReadOnly|DontDelete));

    Nan::Export(target, "initDecoder",      initDecoder);
    Nan::Export(target, "freeDecoder",      freeDecoder);
    Nan::Export(target, "decodeFrame",      decodeFrame<int16_t>);
    Nan::Export(target, "decodeFrameFloat", decodeFrame<float>);
    Nan::Export(target, "getLastFrameInfo", getLastFrameInfo);
}

} //< mpa namespace

NODE_MODULE(mpa, mpa::init)

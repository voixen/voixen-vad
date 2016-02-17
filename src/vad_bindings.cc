#include <algorithm>
#include <sstream>
#include <nan.h>
#include "simplevad.h"

using std::min;
using std::transform;
using std::stringstream;

// required due to name collisions between Nan and V8 - we need to choose what we want here
using v8::Array;
using v8::Exception;
using v8::Function;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::DontDelete;
using v8::String;
using v8::Value;

using Nan::AsyncQueueWorker;
using Nan::AsyncWorker;
using Nan::Callback;
using Nan::Get;
using Nan::HandleScope;
using Nan::MaybeLocal;
using Nan::New;
using Nan::Null;
using Nan::Set;
using Nan::To;

namespace vad
{

namespace
{
// Async worker for simple voice activity detection
class VADWorker : public AsyncWorker
{
public:
    VADWorker(Callback* callback, vad_t vad, size_t rate, const float* samples, size_t length)
        : AsyncWorker(callback), vad(vad), rate(rate), samples(samples), length(length),
          result(VAD_EVENT_SILENCE) {}

    ~VADWorker() {}

    /**
     *    Performs work in a separate thread.
     */
    void Execute() { result = vadProcessAudio(vad, rate, samples, length / sizeof(float)); }

    /**
       *    Convert the output and pass it back to js
     */
    void HandleOKCallback()
    {
        HandleScope scope;
        Local<Value> argv[] = { Null(), New(static_cast<int>(result)) };
        callback->Call(2, argv);    // callback(error, result)
    }

private:
    vad_t        vad;
    size_t       rate;
    const float* samples;
    size_t       length;
    vad_event    result;
};

}

#if defined(V8_MAJOR_VERSION) && (V8_MAJOR_VERSION > 4 ||                      \
  (V8_MAJOR_VERSION == 4 && defined(V8_MINOR_VERSION) && V8_MINOR_VERSION >= 3))
// nodejs v0.12+
static size_t GetByteLength(Local<Value> value)
{
    using v8::ArrayBufferView;

    if (value->IsArrayBufferView())
    {
        Local<ArrayBufferView> array = Local<ArrayBufferView>::Cast(value);
        return array->ByteLength();
    }
    else
    {
        return node::Buffer::Length(value);
    }
}
#else
// nodejs v0.10+
static size_t GetByteLength(Local<Value> value)
{
    if (value->IsObject() && !value->IsNull()) {
        Local<Object> array = Local<Object>::Cast(value);

        MaybeLocal<Value> length = Get(array,
            New<String>("byteLength").ToLocalChecked());

        return (!length.IsEmpty() && length.ToLocalChecked()->IsUint32()) ?
            length.ToLocalChecked()->Uint32Value() : node::Buffer::Length(value);
    }
    else
    {
        return node::Buffer::Length(value);
    }
}
#endif

// Wraps vadAllocate
NAN_METHOD(vadAlloc_)
{
    HandleScope scope;

    Local<Object> obj = New<Object>();

    // #0 buffer
    void* mem         = node::Buffer::HasInstance(info[0]) ? node::Buffer::Data(info[0]) : NULL;
    size_t lenmem     = mem ? node::Buffer::Length(info[0]) : 0;

    vad_t vad = vadAllocate(mem, &lenmem);
    Set(obj, New("size").ToLocalChecked(), New(static_cast<int>(lenmem)));

    if (mem)
    {
        bool error = vad != mem;
        Set(obj, New("error").ToLocalChecked(), New(error));
    }
    else
    {
        Set(obj, New("error").ToLocalChecked(), New(false));
    }

    // return value is { error: true|false, size: Integer }
    info.GetReturnValue().Set(obj);
}


// Wraps vadIint
NAN_METHOD(vadInit_)
{
    HandleScope scope;

    // #0 buffer
    vad_t vad = node::Buffer::HasInstance(info[0]) ?
                reinterpret_cast<vad_t>(node::Buffer::Data(info[0])) : NULL;

    if (!vad)
    {
        Nan::ThrowTypeError("Invalid VAD instance!");
        return;
    }

    // initialise VAD system
    int result = vadInit(vad);
    info.GetReturnValue().Set(result == 0);
}

// Wraps vadSetMode
NAN_METHOD(vadSetMode_)
{
    HandleScope scope;

    // #0 buffer #1 integer
    vad_t vad = node::Buffer::HasInstance(info[0]) ?
                reinterpret_cast<vad_t>(node::Buffer::Data(info[0])) : NULL;

    if (!vad)
    {
        Nan::ThrowTypeError("Invalid VAD instance!");
        return;
    }

    vad_mode mode = static_cast<vad_mode>(To<int32_t>(info[1]).FromJust());

    // apply mode
    int result = vadSetMode(vad, mode);
    info.GetReturnValue().Set(result == 0);
}

// Wraps vadProcessAudio
NAN_METHOD(vadProcessAudioBuffer_)
{
    HandleScope scope;

    // #0 buffer #1 buffer #2 integer #3 callback
    vad_t vad = node::Buffer::HasInstance(info[0]) ?
                reinterpret_cast<vad_t>(node::Buffer::Data(info[0])) : NULL;
    const float* samples = node::Buffer::HasInstance(info[1]) ?
                reinterpret_cast<const float*>(node::Buffer::Data(info[1])) : NULL;

    if (!vad || !samples)
    {
        if (!vad) Nan::ThrowTypeError("Invalid VAD instance!");
        else Nan::ThrowTypeError("Invalid audio buffer!");
        return;
    }

    uint32_t rate = To<uint32_t>(info[2]).FromJust();

    size_t length = GetByteLength(info[1]);
    Callback* callback = new Callback(info[3].As<Function>());
    VADWorker* worker = new VADWorker(callback, vad, rate, samples, length);
    AsyncQueueWorker(worker);
}

// Setup the native exports
NAN_MODULE_INIT(init)
{
    Nan::Export(target, "vad_alloc", vadAlloc_);
    Nan::Export(target, "vad_init", vadInit_);
    Nan::Export(target, "vad_setmode", vadSetMode_);
    Nan::Export(target, "vad_processAudio", vadProcessAudioBuffer_);
}

}

NODE_MODULE(vad, vad::init)

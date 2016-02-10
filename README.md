# voixen-vad
WebRTC-based Voice Activity Detection library

Voice Activity Detection based on the method used in the upcoming [WebRTC](http://http://www.webrtc.org) HTML5 standard.
Extracted from [Chromium](https://chromium.googlesource.com/external/webrtc/+/branch-heads/43/webrtc/common_audio/vad/) for
stand-alone use as a library.

Sample MPEG audio decoder is a stripped-down libmpeg123 from [MPEG123](http://www.mpg123.de).

The Node.js bindings provide a simple way to do VAD on PCM audio input. Input data needs to be constant bitrate normalised
float (-1..+1) PCM audio samples. Detection results are returned using an async callback and additionally via events.

Supported sample rates are:
- 8000Hz*
- 160000Hz*
- 320000Hz
- 480000Hz

*recommended sample rate for best performance/accuracy tradeoff

## Installation

## API

```javascript
var VAD = require('vad').VAD,
```

### VAD(mode)

Create a new `VAD` object using the given mode. The 'mode' parameter is optional.

#### .processAudio(samples, samplerate, callback)

Analyse the given samples (`Buffer` object containing normalised 32bit float values) and notify the detected voice
event via `callback` and event.

#### .on(event, callback)

Subscribe to an event emitted by the VAD instance after detection. The event data provided to the callback is a number that
represents the voice detection result.
Supported event names are:
- 'event': VAD processsing finished successfully or with an error
- 'voice': Human speech was detected
- 'silence': Silence/non-speech was detected
- 'noise': [not implemented yet]
- 'error': an error occured during detection

### Event codes

Event codes are passed to the `processAudio` callback and to event handlers subscribed to the general
'event'-event.

#### VAD.EVENT_ERROR

Constant for voice detection errors. Passed to 'error' event handlers.

#### VAD.EVENT_SILENCE

Constant for voice detection results with no detected voices.
Passed to 'silence' event handlers.

#### VAD.EVENT_VOICE

Constant for voice detection results with detected voice.
Passed to 'voice' event handlers.

#### VAD.EVENT_NOISE

Constant for voice detection results with detected noise.
Not implemented yet

### Available VAD Modes

These contants can be used as the `mode` parameter of the `VAD` constructor to
configure the VAD algorithm. 

#### VAD.MODE_NORMAL

Constant for normal voice detection mode. Suitable for high bitrate, low-noise data.
May classify noise as voice, too. The default value if `mode` is omitted in the constructor.

#### VAD.MODE_LOW_BITRATE

Detection mode optimised for low-bitrate audio.

#### VAD.MODE_AGGRESSIVE

Detection mode best suited for somewhat noisy, lower quality audio.

#### VAD.MODE_VERY_AGGRESSIVE

Detection mode with lowest miss-rate. Works well for most inputs.

### toFloatArray(buffer)

Utility function that coverts a `Buffer` object to a `TypedArray` of type `Float32Array`.
Works with node <0.12 as well as recent versions. Introduced as a node version-agnostic shim.

## Notes

The library is designed to work with input streams in mind, that is, sample buffers fed to `processAudio` should be
rather short (36ms to 144ms - depending on your needs) and the sample rate no higher than 32kHz. Sample rates higher than
than 16kHz provide no benefit to the VAD algorithm, as human voice patterns center around 4000 to 6000Hz. Minding the
Nyquist-frequency yields sample rates between 8000 and 12000Hz for best results.

## Example

```javascript
var VAD = require('vad').VAD

var pcmInputStream = getReadableAudioStreamSomehow()
var pcmOutputStream = getWritableStreamSomehow()
var vad = new VAD(VAD.MODE_LOW_BITRATE)

vad.on('voice', function() {
  console.info('Voice detected!')
})

// this example tries to remove non-speech from an audio file
pcmInputStream.on('data', function(chunk) {
  // assume audio data is 32bit float @ 16kHz
  vad.processAudio(chunk, 160000, function(error, event) {
    if (event === VAD.EVENT_VOICE) {
      pcmOutputStream.write(chunk)
    }
  })
})
```

## License

[MIT](LICENSE)

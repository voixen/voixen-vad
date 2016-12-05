/**
 * VAD Library Sample
 *
 * The example program analyses a given MP3 audio file and outputs a PNG containg both
 * a waveform image of the selected audio channel and a visualisation of the detected
 * voice activity.
 */
'use strict'

const mp3 = require('vad').mpa,
    vad = require('vad').vad,
    VAD = vad.VAD,
    png = require('node-png').PNG

/**
 * Define a colour value comprised of red, green, blue and transparency (alpha)
 * @param {number} [red=0]     Defines the red colour component in the range [0,255]
 * @param {number} [green=0]   Defines the green colour component in the range [0,255]
 * @param {number} [blue=0]    Defines the blue colour component in the range [0,255]
 * @param {number} [alpha=255] Defines the alpha colour component in the range [0,255]
 */
function Colour(red, green, blue, alpha) {
    let clamp = x => Math.min(255, Math.max(0, x >>> 0))
    this.r = clamp(red || 0)
    this.g = clamp(green || 0)
    this.b = clamp(blue || 0)
    this.a = clamp(typeof alpha === 'undefined' ? 255 : alpha)
}

// define some default colours
Object.defineProperty(Colour, 'white',      {value: new Colour(255,255,255), writable: false})
Object.defineProperty(Colour, 'black',      {value: new Colour(), writable: false})
Object.defineProperty(Colour, 'red',        {value: new Colour(255), writable: false})
Object.defineProperty(Colour, 'green',      {value: new Colour(0, 255), writable: false})
Object.defineProperty(Colour, 'background', {value: new Colour(214, 214, 214), writable: false})
Object.defineProperty(Colour, 'waveform',   {value: new Colour(63, 77, 155, 192), writable: false})
Object.defineProperty(Colour, 'voice',      {value: new Colour(255, 152, 0, 224), writable: false})
Object.defineProperty(Colour, 'silence',    {value: new Colour(202, 207, 210, 224), writable: false})


/**
 * Default options for the waveform generator function
 * @property {number} width      Width of the waveform image in pixels (=1024)
 * @property {number} height     Height of the waveform image in pixels (=96)
 * @property {number} mode       VAD detection mode (=vad.MODE_VERY_AGGRESSIVE)
 * @property {Colour} background Background colour of the waveform image
 * @property {Colour} waveform   Colour of the waveform
 * @property {Colour} vad        Colour of the VAD overlay
 * @property {number} channel    Audio channel to visualise (0=left/mono,1=right)
 */
const kDefaultOptions = {
    width: 1024,
    height: 96,
    mode: VAD.MODE_VERY_AGGRESSIVE,
    background: Colour.background,
    waveform: Colour.waveform,
    voice: Colour.voice,
    silence: Colour.silence,
    channel: 0
}

// ------------- start mixins --------------

// Return a flattened version of an array (supports TypedArray and enumerables)
Array.prototype.flatten = function() {
    return this.reduce((a, b) => {
        let i = 0, n = b.length
        while (i < n) a.push(b[i++])
        return a
    }, [])
}

// fill a png with a solid colour (non-blended)
png.prototype.fill = function(colour) {
    var a, b, g, r, ref, ref1, ref2, ref3;

    r = (ref = colour.r) != null ? ref : r, g = (ref1 = colour.g) != null ? ref1 : g, b = (ref2 = colour.b) != null ? ref2 : b, a = (ref3 = colour.a) != null ? ref3 : a;
    //const { r = r, g = g, b = b, a = a } = colour
    const pixels = this.data
    for (let y = 0, ofs = 0; y < this.height; ++y) {
        for (let x = 0; x < this.width; ++x, ofs += 4) {
            pixels[ofs + 0] = r
            pixels[ofs + 1] = g
            pixels[ofs + 2] = b
            pixels[ofs + 3] = a
        }
    }
}

// draw a vertical line
png.prototype.vline = function(x, y1, y2, colour) {
    var a, b, g, r, ref, ref1, ref2, ref3;
    r = (ref = colour.r) != null ? ref : r, g = (ref1 = colour.g) != null ? ref1 : g, b = (ref2 = colour.b) != null ? ref2 : b, a = (ref3 = colour.a) != null ? ref3 : a;

    const pixels = this.data
    const ia = 255 - a
    let ofs = (y1 * this.width + x) * 4,
        inc = this.width * 4
    for (let i = 0, n = y2 - y1; i < n; ++i, ofs += inc) {
        var _a, _b, _g, _r, ref, oa;
        ref = pixels.slice(ofs, ofs + 4), _r = ref[0], _g = ref[1], _b = ref[2], _a = ref[3];
        oa = (a + (_a * ia) / 255) >>> 0;
        // simple, unptimised full alpha blending
        pixels[ofs + 0] = ((r * a + (_r * _a * ia) / 255) / oa) >>> 0
        pixels[ofs + 1] = ((g * a + (_g * _a * ia) / 255) / oa) >>> 0
        pixels[ofs + 2] = ((b * a + (_b * _a * ia) / 255) / oa) >>> 0
        pixels[ofs + 3] = oa
    }
}

// ------------- end mixins --------------

/**
 * Simple waveform image generator.
 * @param {Object} [options]                              Selected options
 * @param {Number} [options.width=1024]                   Width of the output image in pixels
 * @param {Number} [options.height=96]                    Height of the output image in pixels
 *                                                        (>= 32 pixels)
 * @param {Number} [options.mode=3]                       Voice activity detection mode [0..3]
 * @param {Colour} [options.background=Colour.background] Background colour of the output image
 * @param {Colour} [options.waveform=Colour.waveform]     Waveform visualisation colour
 * @param {Colour} [options.voice=Colour.voice]           Speech visualisation colour
 * @param {Colour} [options.silence=Colour.silence]       Silence visualisation colour
 * @param {Number} [options.channel=0]                    Visualised audio channel [0=left/mono, 1=right]
 */
function WaveformGenerator(options) {
    this.options = this._joinOptions(options || {}, kDefaultOptions)
    this.frameInfo = null
    this.duration = 0
    this.samples = []
    this.voiceEvents = []
    this.vadFrames = 0
    this.totalFrames = 0

    // create waveform image
    this.image = new png(this.options)

    // create MP3 decoder instance and tell it to return normalised float data [-1..+1]
    this.decode = mp3.createDecoder({decodeAsFloat: true})
    // the "frameinfo"-event provides us with meta data (channels, bitrate, etc.)
    this.decode.once('frameInfo', info => this.frameInfo = info)
    // the 'samples'-event hands us an object containg channel data (samples)
    this.decode.on('samples', this._onSamples.bind(this))
    // once the decoder finished, check if VAD has finished as well and draw the results
    this.decode.on('finish', (() => {
        let vadFinished = this.vadFrames === this.totalFrames
        this.decoderFinished = true
        // draw waveform image and voice activity if detection has finished
        if (vadFinished) this._draw()
        // in case there is still VAD going on in the background, we handle
        // the drawing in the VAD callback as well
    }).bind(this))

    // create Voice Activity Detection instance
    this.voiceDetection = vad.createVAD(this.options.mode)
}

// Join default and user options
WaveformGenerator.prototype._joinOptions = function(options, defaults) {
    const select = prop => prop in options ?
        options[prop] : defaults[prop]
    return {
        width: select('width'), height: Math.max(32, select('height')),
        mode: select('mode'), channel: select('channel'),
        background: select('background'), waveform: select('waveform'),
        voice: select('voice'), silence: select('silence')
    }
}

// Select the requested audio channel from the decoded audio frame data
WaveformGenerator.prototype._selectChannel = function(sampleData) {
    const samples = this.options.channel === 0 ? sampleData.left : sampleData.right
    return samples || []
}

// Process a single frame of decoded audio data
WaveformGenerator.prototype._onSamples = function(sampleData) {
    const channel = this._selectChannel(sampleData)
    const sampleRate = this.frameInfo.samplerate
    const bitsPerSample = this.frameInfo.bitsPerSample
    const bytesPerFrame = channel.length
    const frameDuration = ((bytesPerFrame / bitsPerSample) * 8) / sampleRate

    ++this.totalFrames
    this.duration += frameDuration
    // since we never actually read from the
    this.decode.read()

    // channel data is a nodejs Buffer object - we want an array of floats instead
    // for drawing the waveform data
    this.samples.push(vad.toFloatArray(channel))

    // start voice activity detection on frame
    this.voiceDetection.processAudio(channel, sampleRate, ((err, evt)  => {
        let vadFinished = ++this.vadFrames === this.totalFrames
        this.voiceEvents.push(evt)
        // draw waveform and voice activity if both decoder and detection have finished
        if (this.decoderFinished && vadFinished) this._draw()
        // if there are more frames to come, the decoder's 'finish' event handler
        // will take care of drawing the outputs
    }).bind(this))
}

// Resize samples to match the output image width
// (only works if the image width is lower or equal to the number of samples*2)
WaveformGenerator.prototype._resample = function(samples, rate) {
    let result = [], min = 1, max = -1, j = 0

    for (let i = 0; i < samples.length; ++i) {
        if (samples[i] < min) min = samples[i]
        if (samples[i] > max) max = samples[i]
        if (++j >= rate) {
            result.push(min, max)
            j = 0, min = 1, max = -1
        }
    }

    if (j > 0) {
        result.push(min, max)
    }

    return result
}

// Draw the output image
WaveformGenerator.prototype._draw = function() {
    const samples = this.samples.flatten()
    const samplesPerPixel = Math.round(samples.length / this.options.width)
    const resampled = this._resample(samples, samplesPerPixel)

    this.image.fill(this.options.background)

    for (let x = 0, i = 0; x < this.options.width; ++x, i += 2) {
        this._drawSample(x, resampled[i], resampled[i+1], this.options.waveform)
    }

    this._drawVoiceEvents(this.options.voice)

    this.image.pack().pipe(this.output)
}

// Draw a single set of samples and keep the lower 16 pixels for voice events
WaveformGenerator.prototype._drawSample = function(x, y1, y2, colour) {
    const kWaveformHeight = this.image.height - 16
    const low = y1 + 1, // [-1..+1] => [0..2]
        hi = y2 + 1,    // [-1..+1] => [0..2]
        // calculate the amplitude and centre it; make sure it's at least one pixel
        d = Math.max((hi - low) * kWaveformHeight * 0.5, 1),
        ly = Math.round((kWaveformHeight - d) / 2),
        hy = Math.round(ly + d)

    this.image.vline(x, ly, hy, colour)
}

// Visualise voice activity in the lower 16 pixels of the output image
WaveformGenerator.prototype._drawVoiceEvents = function() {
    const eventsPerPixel = this.voiceEvents.length / this.image.width,
        kVoiceActivityOffset = this.image.height - 16,
        events = this.voiceEvents;

        var _a, _b, _g, _r, ref;

        ref = pixels.slice(ofs, ofs + 4), _r = ref[0], _g = ref[1], _b = ref[2], _a = ref[3];
        
    for (let x = 0; x < width; ++x) {
        const i = Math.round(x * eventsPerPixel)
        let colour = events[i] === VAD.EVENT_VOICE ? this.options.voice :
                    events[i] === VAD.EVENT_SILENCE ? this.options.silence : null
        if (colour !== null) {
            this.image.vline(x, kVoiceActivityOffset, height, colour)
        }
    }
}

/**
 * @api public
 * Generate a waveform image and a voice activity visualisation from a given
 * MP3 audio stream and output the result into the provided output stream
 * @param {Stream} audioStream  Readable stream that contains MP3 audio data
 * @param {Stream} outputStream Writeable stream that receives the waveform PNG image
 */
WaveformGenerator.prototype.generateWaveform = function(audioStream, outputStream) {
    if (typeof audioStream !== 'object')
        throw new TypeError('audioStream must be a valid Stream instance')

    if (typeof outputStream !== 'object')
        throw new TypeError('outputStream must be a valid Stream instance')

    this.input = audioStream
    this.output = outputStream
    this.input.pipe(this.decode)
}

// -== Main script ==-

const stdio = require('stdio'),
    fs = require('fs')
const opts = stdio.getopt({
    'mode': {key: 'm', args: 1, description: 'Selects the VAD mode [0..3] (default: 3)'},
    'input': {key: 'i', args: 1, mandatory: true, description: 'Input file name'},
    'output': {key: 'o', args: 1, mandatory: true, description: 'Output file name'},
    'width': {key: 'w', args: 1, description: 'Image width in pixels (default: 1024)'},
    'height': {key: 'h', args: 1, description: 'Image height in pixels (default: 96)'},
    'channel': {key: 'c', args: 1, description: 'Selected audio channel (0|1) (default: 0)'},
    'background': {key: 'b', args: 1, description: 'Image background in RGB(A) hex (e.g. ffffff)'},
    'voice': {key: 'v', args: 1, description: 'Voice activity colour in RGB(A) hex (e.g. b0220080)'},
    'silence': {key: 's', args: 1, description: 'Non-voice colour in RGB(A) hex (e.g. 22222280)'},
    'waveform': {key: 'f', args: 1, description: 'Waveform colour in RGB(A) hex (e.g. 646464a0)'}
})

// extract colours from hex values
let parseHex = (str, c) => parseInt(str.substr(2 * c, 2), 16)
let parseCol = col => /^[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/.test(col) ?
    new Colour(parseHex(col, 0), parseHex(col, 1), parseHex(col, 2),
        col.length === 8 ? parseHex(col, 3) : 255) : null

const parseopt = {
    'mode': { parse: parseInt, verify: v => v >= 0 && v <= 3 },
    'width': { parse: parseInt, verify: v => v > 0 },
    'height': { parse: parseInt, verify: v => v >= 32 },
    'channel': { parse: parseInt, verify: v => v >= 0 && v <= 1 },
    'background': { parse: parseCol, verify: v => v !== null },
    'voice': { parse: parseCol, verify: v => v !== null },
    'silence': { parse: parseCol, verify: v => v !== null },
    'waveform': { parse: parseCol, verify: v => v !== null }
}

let options = Object.keys(parseopt).filter(key => opts[key]).reduce((out, opt) => {
    if (typeof opts[opt] === 'boolean') {
        console.error('Missing argument for ' + opt)
        process.exit(1)
    }

    const parser = parseopt[opt]
    const value = parser.parse(opts[opt])

    if (!parser.verify(value)) {
        console.error('Invalid argument for ' + opt + ': "' + opts[opt] + '"')
        process.exit(1)
    }

    out[opt] = value
    return out
}, {})

// do the actual work
let waveform = new WaveformGenerator(options)
waveform.generateWaveform(fs.createReadStream(opts.input), fs.createWriteStream(opts.output))

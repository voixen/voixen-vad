var binding     = require('./binding').mpa,    // native bindings
    inherits    = require('util').inherits,    // inheritance utils
    Transform   = require('stream').Transform, // transform stream
    async       = require('async')             // async package

/**
 * @api public
 * @class
 * Provides a transform stream for decoding MPEG.1 and MPEG.2 Layer I-III
 * audio streams
 * @param {Object}  [options] Options for the underlying stream
 * @param {Boolean} [options.decodeAsFloat] If true, the stream will be decoded
 *                  as Float, otherwise clipped Int16 samples will be returned
 * @param {Integer} [options.bufferSize] Output buffer size in bytes - use with caution!
 *
 * @fires DecoderStream#frameInfo
 * @fires DecoderStream#samples
 * @remarks
 * The decoder will always return {@link Buffer} objects. For stereo files, the resulting
 * PCM samples will be interleaved, otherwise only the left channel will be populated.
 */
function DecoderStream(options)
{
    // disallow use without new
    if (!(this instanceof DecoderStream))  {
        throw new Error('Must be called with "new"')
    }

    var bufferSize = 0
    this._options = options || {}

    if (this._options.decodeAsFloat) {
        this._sampleSize = 4
        this._decode = binding.decodeFrameFloat
        bufferSize = binding.MPA_FLOAT_BUFFER_SIZE
    } else {
        this._sampleSize = 2
        this._decode = binding.decodeFrame
        bufferSize = binding.MPA_SAMPLE_BUFFER_SIZE
    }

    bufferSize = this._options.bufferSize || bufferSize

    Transform.call(this, options)

    // initialise the native decoder
    var mpaSize = binding.initDecoder(null)
    this._mpa = new Buffer(mpaSize)
    binding.initDecoder(this._mpa)

    // create output buffers
    this._samplesLeft = new Buffer(bufferSize)
    this._samplesRight = new Buffer(bufferSize)
    this._firstFrame = true
    this._closed = false
    this._frameInfo = {}
}

inherits(DecoderStream, Transform)

/**
 * @api private
 * Flushes the output buffers
 */
DecoderStream.prototype._flush = function(callback) {
    this._transform(new Buffer(0), '', callback)
}

/**
 * @api private
 * Flushes the output buffers
 */
DecoderStream.prototype.flush = function(callback) {
    var ws = this._writableState

    if (ws.ended) {
        if (callback) {
            process.nextTick(callback)
        }
    } else if (ws.ending) {
        if (callback) {
            this.once('end', callback)
        }
    } else if (ws.needDrain) {
        this.once('drain', this.flush.bind(this, callback))
    } else {
        this.write(new Buffer(0), '', callback)
    }
}

/**
 * @api private
 * Close the stream and free the decoder state
 */
DecoderStream.prototype.close = function(callback) {
    if (callback) {
        process.nextTick(callback)
    }

    if (this._closed) {
        return
    }

    this._closed = true
    binding.freeDecoder(this._mpa)

    process.nextTick(this.emit.bind(this, 'close'))
}

/**
 * @private
 * Interleave stereo samples (Int16)
 */
function interleaveShort(inter, left, right, bytes) {
    for (var i = 0, j = 0; i < bytes; i += 2) {
        inter[j++] = left[i+0]
        inter[j++] = left[i+1]
        inter[j++] = right[i+0]
        inter[j++] = right[i+1]
    }
}

/**
 * @private
 * Interleave stereo samples (Float)
 */
function interleaveFloat(inter, left, right, bytes) {
    for (var i = 0, j = 0; i < bytes; i += 4) {
        inter[j++] = left[i+0]
        inter[j++] = left[i+1]
        inter[j++] = left[i+2]
        inter[j++] = left[i+3]
        inter[j++] = right[i+0]
        inter[j++] = right[i+1]
        inter[j++] = right[i+2]
        inter[j++] = right[i+3]
    }
}

/**
 * @api private
 * Implements the actual transform by decoding the audio stream (async)
 */
DecoderStream.prototype._transform = function(chunk, encoding, callback) {

    if (chunk !== null && !Buffer.isBuffer(chunk)) {
        // we can only handle buffers
        return callback(new Error('Invalid input'))
    }

    if (chunk === null) {
        // nothing to flush
        return callback()
    }

    var haveSamples = false,
        inputLength = chunk.length

    function dataAvailable() {
        return haveSamples
    }

    function decodeInput(next) {
        // implement readable stream interface by providing decoded PCM data
        function emitSamples(error, result) {
            haveSamples = result.sampleCount > 0
            if (result.frameInfo) {
                this._frameInfo = result.frameInfo
                this.emit('frameInfo', this._frameInfo)
            }
            if (haveSamples) {
                var bytes = result.sampleCount * this._sampleSize,
                    info = this._frameInfo,
                    left = new Buffer(result.samplesLeft),
                    right = new Buffer(result.samplesRight),
                    data = {
                        left: left.slice(0, bytes),
                        right: right.slice(0, bytes)
                    }

                if (info.channels > 1) {
                    var inter = new Buffer(bytes * 2)
                    if (this._sampleSize === 2) {
                        interleaveShort(inter, left, right, bytes)
                    } else {
                        interleaveFloat(inter, left, right, bytes)
                    }
                    this.push(inter)
                } else {
                    this.push(data.left)
                }

                this.emit('samples', data)
            }
            // subsequent calls only flush input buffers
            inputLength = 0
            next(error)
        }

        try {
            this._decode(this._mpa, chunk, inputLength, this._samplesLeft,
                this._samplesRight, emitSamples.bind(this))
        } catch (error) {
            next(error)
        }
    }

    async.doWhilst(decodeInput.bind(this), dataAvailable, callback)
}

/**
 * @api public
 * Create a decoder stream
 * @param {object} options See {@link DecoderStream}
 */
function createDecoder(options) {
    return new DecoderStream(options)
}

/**
 *    Fires whenever the stream properties change.
 *    This event is useful for getting sample rate,
 *    bits per sample and other information about
 *    the audio stream.
 *
 *    @event DecoderStream#frameInfo
 *    @type {object}
 *    @property {Integer} bitrate Bitrate of the stream in kbps
 *    @property {Integer} channels Number of channels in the stream (1: mono, 2: stereo}
 *    @property {Integer} samplerate Number of samples per second (frequency in Hz)
 *    @property {Integer} bitsPerSample Number of bits per output sample (16: Short, 32: Float)
 *    @property {Integer} layer MPEG.x Layer (1: Layer I, 2: Layer II, 3: layer III)
 *    @property {String} version MPEG stream version ('MPEG1', 'MPEG2' or 'MPEG2.5')
 *    @property {Integer} mode Stream stereo mode (0: Stereo, 1: Joint Stereo, 2: Dual Channel, 3: Mono)
 */

/**
 *    Fires whenever decoded samples are available.
 *
 *    @event DecoderStream#samples
 *    @type {object}
 *    @property {Buffer} left Decoded samples of the left channel
 *    @property {Buffer} [right] Decoded samples of the right channel (stereo only)
 *    @property {Object} [frameInfo] Frame information (available only if changed)
 *                                   see {@link DecoderStream#frameInfo} for contents
 */

// Exports
module.exports = {
    DecoderStream: DecoderStream,
    createDecoder: createDecoder
}

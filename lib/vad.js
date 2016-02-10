var binding         = require('./binding').vad,        // native bindings
    inherits        = require('util').inherits,
    EventEmitter    = require('events').EventEmitter

/**
 * @api public
 * Utility function that converts a buffer to a ES float array
 * @param {Buffer} buffer Node buffer containing binary float32 data
 * @returns {Float32Array} TypedArray from buffer object
 */
function toFloatArray(buffer) {

    if (!buffer || !buffer.length) {
        return new Float32Array()
    }

    if (buffer.toArrayBuffer) {
        // Node 0.12+
        return new Float32Array(buffer.toArrayBuffer())
    } else {
        // Node <0.12
        var len = buffer.length, i,
            arrayBuffer = new ArrayBuffer(len),
            view = new Uint8Array(arrayBuffer)

        for (i = 0; i < len; ++i) {
            view[i] = buffer[i]
        }

        return new Float32Array(arrayBuffer)
    }
}

/**
 * @api public
 * @class
 * Provides simple Voice Activity Detection
 * @param {Number} [mode] Voice detection mode
 */
function VAD(mode) {
    if (!(this instanceof VAD)) {
        throw new Error('Must be called with "new"')
    }

    var res = binding.vad_alloc(null)
    if (res.error) {
        throw new Error('Failed to get VAD size')
    }

    this._vad = new Buffer(res.size)
    res = binding.vad_alloc(this._vad)
    if (!res) {
        throw new Error('Failed to allocate VAD')
    }

    res = binding.vad_init(this._vad)
    if (!res) {
        throw new Error('Failed to initialise VAD')
    }

    if (typeof mode === 'number' &&
        mode >= VAD.MODE_NORMAL && mode <= VAD.MODE_VERY_AGGRESSIVE) {
        binding.vad_setmode(this._vad, mode)
    } else if (typeof mode !== 'undefined') {
        throw new Error('Invalid mode settings')
    }

    this._processQueue = []
}

inherits(VAD, EventEmitter)

/**
 * @api public
 * @static
 * @readonly
 * @property {Number} VAD.EVENT_ERROR   Constant for VAD error event
 * @property {Number} VAD.EVENT_SILENCE Constant for VAD silence event
 * @property {Number} VAD.EVENT_VOICE   Constant for VAD voice event
 * @property {Number} VAD.EVENT_NOISE   Constant for VAD noise event [not supported yet]
 */
Object.defineProperty(VAD, 'EVENT_ERROR',   { value: -1, writable: false })
Object.defineProperty(VAD, 'EVENT_SILENCE', { value: 0, writable: false })
Object.defineProperty(VAD, 'EVENT_VOICE',   { value: 1, writable: false })
Object.defineProperty(VAD, 'EVENT_NOISE',   { value: 2, writable: false })

/**
 * @api public
 * @static
 * @readonly
 * @property {Number} VAD.MODE_NORMAL           Constant for normal voice detection mode
 * @property {Number} VAD.MODE_LOW_BITRATE      Constant for low bitrate voice detection mode
 * @property {Number} VAD.MODE_AGGRESSIVE       Constant for aggressive voice detection mode
 * @property {Number} VAD.MODE_VERY_AGGRESSIVE  Constant for very aggressive voice detection mode
 */
Object.defineProperty(VAD, 'MODE_NORMAL',          { value: 0, writable: false })
Object.defineProperty(VAD, 'MODE_LOW_BITRATE',     { value: 1, writable: false })
Object.defineProperty(VAD, 'MODE_AGGRESSIVE',      { value: 2, writable: false })
Object.defineProperty(VAD, 'MODE_VERY_AGGRESSIVE', { value: 3, writable: false })

/**
 * @api private
 * @function
 * Processes the next item in the processing queue
 */
VAD.prototype._dequeueItem = function() {
    var EVENT_MAP = ['error', 'silence', 'voice', 'noise']

    function evaluateAndDequeueNext(err, res) {
        var item = this._processQueue.shift(),
            index = res + 1

        this.emit('event', res)

        if (index >= 0 && index < EVENT_MAP.length) {
            this.emit(EVENT_MAP[index], res)
        }

        try {
            item.callback(err, res)
        } catch(e) {
            this.emit('error', e)
        }

        // continue on the next tick
        process.nextTick(this._dequeueItem.bind(this))
    }

    if (this._processQueue.length > 0) {
        var entry = this._processQueue[0]
        binding.vad_processAudio(this._vad, entry.samples, entry.rate,
            evaluateAndDequeueNext.bind(this))
    }
}

/**
 * @api public
 * @function
 * Analyses the given buffer and returns voice or silence.
 *
 * @param    {Buffer}            samples     Signal to analyse (containing normalised float samples)
 * @param    {Number}            samplerate  Sample rate of the signal in Hz
 * @param    {VAD~asyncCallback} callback    Async callback that is invoked after completion
 */
VAD.prototype.processAudio = function(samples, samplerate, callback) {
    if (!callback || typeof callback !== 'function') {
        throw new Error('Callback must be a function')
    }

    this._processQueue.push({ samples: samples, rate: samplerate, callback: callback })

    if (this._processQueue.length === 1) {
        this._dequeueItem()
    }
}

/**
 * @api public
 * @function
 * Creates a new Voice Activity Detection object
 *
 * @param {Number} [mode] Voice detection mode
 * @returns {VAD}
 */
function createVAD(mode) {
    return VAD(mode)
}

/**
 * This callback notifies the detected voice event for the processed audio.
 * @callback VAD~asyncCallback
 * @param {Object|Null} error    Error that occurred during the operation
 * @param {VoiceEvent}  result   VAD event that was generated by the audio
 */

module.exports = {
    VAD:            VAD,
    createVAD:      createVAD,
    toFloatArray:   toFloatArray
}
var Decoder = require('./lib/decoderstream'),
    VAD		= require('./lib/vad')

module.exports = {
    mpa: Decoder,	// Transform stream that decodes MPEG audio input files to PCM samples
    vad: VAD		// Voice Activity Detection
}
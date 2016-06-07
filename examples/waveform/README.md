# Waveform Example Application

## Purpose

The example illustrates how to:
* open and decode MP3 files
* perform voice activity detection on MPEG audio frames
* visualise audio data as a waveform
* display voice activity events

## Installation

Change into the `./examples/waveform` subfolder and use `npm install` to get all dependencies.

## Usage

Start the program with `node waveform-generator.js --help` to get a list of all supported options.

## Example

Generate waveform images and voice activity outputs from a file named _test.mp3_ and output
the results to _left.png_ for the left channel and _right.png_ for the right channel:

```
node waveform-generator.js -i test.mp3 -o left.png -c 0 && \
node waveform-generator.js -i test.mp3 -o right.png -c 1
```
## Implementation

Please refer source code for more information on the implementation itself.
The sources are documented.

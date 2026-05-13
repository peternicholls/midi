#ifndef MIDI_OUTPUT_H
#define MIDI_OUTPUT_H

#include "midi_result.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>

#include <stddef.h>
#include <stdint.h>

typedef struct MidiOutput {
  MIDIClientRef client;
  MIDIPortRef port;
} MidiOutput;

void midi_output_init(MidiOutput *output);
MidiResult midi_output_open(MidiOutput *output, CFStringRef client_name,
                            CFStringRef port_name);
void midi_output_close(MidiOutput *output);
MidiResult midi_output_send(MidiOutput *output, MIDIEndpointRef destination,
                            const uint8_t *bytes, size_t length);

#endif

#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include <stddef.h>
#include <stdint.h>

typedef enum MidiParsedKind {
    MIDI_PARSED_CHANNEL = 0,
    MIDI_PARSED_SYSEX = 1,
    MIDI_PARSED_UNSUPPORTED = 2,
    MIDI_PARSED_INCOMPLETE = 3
} MidiParsedKind;

typedef struct MidiParsedMessage {
    MidiParsedKind kind;
    size_t length;
} MidiParsedMessage;

size_t midi_next_message(const uint8_t *bytes, size_t available, MidiParsedMessage *out);

#endif

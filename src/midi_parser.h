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
    uint8_t channel_bytes[3];
    size_t channel_length;
} MidiParsedMessage;

typedef struct MidiParserState {
    uint8_t running_status;
} MidiParserState;

size_t midi_channel_message_length(uint8_t status);
void midi_parser_state_init(MidiParserState *state);
size_t midi_next_message(const uint8_t *bytes, size_t available, MidiParsedMessage *out);
size_t midi_next_stream_message(MidiParserState *state,
                                const uint8_t *bytes,
                                size_t available,
                                MidiParsedMessage *out);

#endif

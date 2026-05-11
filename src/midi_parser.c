#include "midi_parser.h"

static size_t midi_channel_message_length(uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0:
        case 0xD0:
            return 2;
        default:
            return 3;
    }
}

size_t midi_next_message(const uint8_t *bytes, size_t available, MidiParsedMessage *out) {
    if (out == NULL) {
        return 0;
    }

    out->kind = MIDI_PARSED_INCOMPLETE;
    out->length = 0;

    if (bytes == NULL || available == 0) {
        return 0;
    }

    const uint8_t status = bytes[0];

    if (status < 0x80) {
        out->kind = MIDI_PARSED_UNSUPPORTED;
        out->length = 1;
        return 1;
    }

    if (status < 0xF0) {
        const size_t needed = midi_channel_message_length(status);
        if (available < needed) {
            return 0;
        }

        out->kind = MIDI_PARSED_CHANNEL;
        out->length = needed;
        return needed;
    }

    switch (status) {
        case 0xF0: {
            for (size_t i = 1; i < available; ++i) {
                if (bytes[i] == 0xF7) {
                    out->kind = MIDI_PARSED_SYSEX;
                    out->length = i + 1;
                    return i + 1;
                }
            }
            return 0;
        }
        case 0xF1:
        case 0xF3:
            if (available < 2) {
                return 0;
            }
            out->kind = MIDI_PARSED_UNSUPPORTED;
            out->length = 2;
            return 2;
        case 0xF2:
            if (available < 3) {
                return 0;
            }
            out->kind = MIDI_PARSED_UNSUPPORTED;
            out->length = 3;
            return 3;
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            out->kind = MIDI_PARSED_UNSUPPORTED;
            out->length = 1;
            return 1;
        default:
            return 0;
    }
}

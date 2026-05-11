#include "midi_parser.h"

size_t midi_channel_message_length(uint8_t status) {
    switch (status & 0xF0) {
        case 0xC0:
        case 0xD0:
            return 2;
        default:
            return 3;
    }
}

void midi_parser_state_init(MidiParserState *state) {
    if (state != NULL) {
        state->running_status = 0;
    }
}

static void clear_message(MidiParsedMessage *out) {
    out->kind = MIDI_PARSED_INCOMPLETE;
    out->length = 0;
    out->channel_length = 0;
    out->channel_bytes[0] = 0;
    out->channel_bytes[1] = 0;
    out->channel_bytes[2] = 0;
}

static int status_supports_running(uint8_t status) {
    return status >= 0x80 && status < 0xF0;
}

static void clear_running_status_for_system_message(MidiParserState *state, uint8_t status) {
    if (state == NULL) {
        return;
    }

    if (status >= 0xF0 && status <= 0xF7) {
        state->running_status = 0;
    }
}

static size_t parse_message(const uint8_t *bytes,
                            size_t available,
                            MidiParsedMessage *out,
                            MidiParserState *state) {
    if (out == NULL) {
        return 0;
    }

    clear_message(out);

    if (bytes == NULL || available == 0) {
        return 0;
    }

    const uint8_t status = bytes[0];

    if (status < 0x80) {
        if (state != NULL && status_supports_running(state->running_status)) {
            const size_t total_length = midi_channel_message_length(state->running_status);
            const size_t data_length = total_length - 1;
            if (available < data_length) {
                return 0;
            }

            out->kind = MIDI_PARSED_CHANNEL;
            out->length = data_length;
            out->channel_length = total_length;
            out->channel_bytes[0] = state->running_status;
            for (size_t i = 0; i < data_length; ++i) {
                out->channel_bytes[i + 1] = bytes[i];
            }
            return data_length;
        }

        out->kind = MIDI_PARSED_UNSUPPORTED;
        out->length = 1;
        return 1;
    }

    if (status < 0xF0) {
        const size_t needed = midi_channel_message_length(status);
        if (available < needed) {
            return 0;
        }

        if (state != NULL) {
            state->running_status = status;
        }

        out->kind = MIDI_PARSED_CHANNEL;
        out->length = needed;
        out->channel_length = needed;
        for (size_t i = 0; i < needed; ++i) {
            out->channel_bytes[i] = bytes[i];
        }
        return needed;
    }

    clear_running_status_for_system_message(state, status);

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

size_t midi_next_message(const uint8_t *bytes, size_t available, MidiParsedMessage *out) {
    return parse_message(bytes, available, out, NULL);
}

size_t midi_next_stream_message(MidiParserState *state,
                                const uint8_t *bytes,
                                size_t available,
                                MidiParsedMessage *out) {
    return parse_message(bytes, available, out, state);
}

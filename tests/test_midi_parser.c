#include "midi_parser.h"

#include <assert.h>
#include <stdint.h>

static void assert_channel_bytes(const MidiParsedMessage *parsed,
                                 uint8_t status,
                                 uint8_t data1,
                                 uint8_t data2,
                                 size_t length) {
    assert(parsed->channel_length == length);
    assert(parsed->channel_bytes[0] == status);
    assert(parsed->channel_bytes[1] == data1);
    if (length == 3) {
        assert(parsed->channel_bytes[2] == data2);
    }
}

static void test_note_on_is_three_bytes(void) {
    const uint8_t bytes[] = {0x90, 60, 100};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 3);
    assert(parsed.kind == MIDI_PARSED_CHANNEL);
    assert(parsed.length == 3);
    assert_channel_bytes(&parsed, 0x90, 60, 100, 3);
}

static void test_program_change_is_two_bytes(void) {
    const uint8_t bytes[] = {0xC3, 10};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 2);
    assert(parsed.kind == MIDI_PARSED_CHANNEL);
    assert(parsed.length == 2);
    assert_channel_bytes(&parsed, 0xC3, 10, 0, 2);
}

static void test_sysex_consumes_until_f7(void) {
    const uint8_t bytes[] = {0xF0, 0x7D, 0x01, 0xF7, 0x90, 64, 127};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 4);
    assert(parsed.kind == MIDI_PARSED_SYSEX);
    assert(parsed.length == 4);
}

static void test_incomplete_message_stays_incomplete(void) {
    const uint8_t bytes[] = {0x90, 60};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 0);
    assert(parsed.kind == MIDI_PARSED_INCOMPLETE);
}

static void test_null_bytes_input_is_incomplete(void) {
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(NULL, 3, &parsed);

    assert(used == 0);
    assert(parsed.kind == MIDI_PARSED_INCOMPLETE);
}

static void test_null_out_parameter_is_rejected(void) {
    const uint8_t bytes[] = {0x90, 60, 100};

    size_t used = midi_next_message(bytes, sizeof(bytes), NULL);

    assert(used == 0);
}

static void test_sysex_without_terminator_is_incomplete(void) {
    const uint8_t bytes[] = {0xF0, 0x7D, 0x01};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 0);
    assert(parsed.kind == MIDI_PARSED_INCOMPLETE);
}

static void test_running_status_reconstructs_channel_message(void) {
    const uint8_t first[] = {0x90, 60, 100};
    const uint8_t second[] = {64, 127};
    MidiParserState state;
    MidiParsedMessage parsed = {0};
    midi_parser_state_init(&state);

    size_t used = midi_next_stream_message(&state, first, sizeof(first), &parsed);
    assert(used == 3);
    assert_channel_bytes(&parsed, 0x90, 60, 100, 3);

    used = midi_next_stream_message(&state, second, sizeof(second), &parsed);
    assert(used == 2);
    assert(parsed.kind == MIDI_PARSED_CHANNEL);
    assert(parsed.length == 2);
    assert_channel_bytes(&parsed, 0x90, 64, 127, 3);
}

static void test_running_status_without_previous_status_is_unsupported(void) {
    const uint8_t bytes[] = {64};
    MidiParserState state;
    MidiParsedMessage parsed = {0};
    midi_parser_state_init(&state);

    size_t used = midi_next_stream_message(&state, bytes, sizeof(bytes), &parsed);

    assert(used == 1);
    assert(parsed.kind == MIDI_PARSED_UNSUPPORTED);
    assert(parsed.length == 1);
}

static void test_system_common_lengths_are_reported(void) {
    const uint8_t mtc[] = {0xF1, 0x7F};
    const uint8_t song_position[] = {0xF2, 0x01, 0x02};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(mtc, sizeof(mtc), &parsed);
    assert(used == 2);
    assert(parsed.kind == MIDI_PARSED_UNSUPPORTED);
    assert(parsed.length == 2);

    used = midi_next_message(song_position, sizeof(song_position), &parsed);
    assert(used == 3);
    assert(parsed.kind == MIDI_PARSED_UNSUPPORTED);
    assert(parsed.length == 3);
}

static void test_realtime_message_is_skippable(void) {
    const uint8_t bytes[] = {0xFE};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 1);
    assert(parsed.kind == MIDI_PARSED_UNSUPPORTED);
    assert(parsed.length == 1);
}

int main(void) {
    test_note_on_is_three_bytes();
    test_program_change_is_two_bytes();
    test_sysex_consumes_until_f7();
    test_incomplete_message_stays_incomplete();
    test_null_bytes_input_is_incomplete();
    test_null_out_parameter_is_rejected();
    test_sysex_without_terminator_is_incomplete();
    test_running_status_reconstructs_channel_message();
    test_running_status_without_previous_status_is_unsupported();
    test_system_common_lengths_are_reported();
    test_realtime_message_is_skippable();
    return 0;
}

#include "midi_parser.h"

#include <assert.h>
#include <stdint.h>

static void test_note_on_is_three_bytes(void) {
    const uint8_t bytes[] = {0x90, 60, 100};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 3);
    assert(parsed.kind == MIDI_PARSED_CHANNEL);
    assert(parsed.length == 3);
}

static void test_program_change_is_two_bytes(void) {
    const uint8_t bytes[] = {0xC3, 10};
    MidiParsedMessage parsed = {0};

    size_t used = midi_next_message(bytes, sizeof(bytes), &parsed);

    assert(used == 2);
    assert(parsed.kind == MIDI_PARSED_CHANNEL);
    assert(parsed.length == 2);
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
    test_realtime_message_is_skippable();
    return 0;
}

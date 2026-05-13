#ifndef MIDI_RECORDER_H
#define MIDI_RECORDER_H

#include "midi_parser.h"
#include "midi_result.h"

#include <AudioToolbox/AudioToolbox.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Recorder counters remain atomic with relaxed loads/stores because CoreMIDI
 * read callbacks may update them from a real-time thread. The shared recorder
 * keeps packet ingestion lock-free and leaves elapsed-time policy to callers.
 */
typedef struct MidiRecorder {
  MusicSequence sequence;
  MusicTrack track;
  MidiParserState parser_state;
  atomic_uint captured_events;
  atomic_uint ignored_messages;
} MidiRecorder;

MidiResult midi_recorder_init(MidiRecorder *recorder);
void midi_recorder_dispose(MidiRecorder *recorder);

MidiResult midi_recorder_add_channel_event(MidiRecorder *recorder,
                                           const uint8_t *bytes, size_t length,
                                           double elapsed_seconds);
MidiResult midi_recorder_add_sysex_event(MidiRecorder *recorder,
                                         const uint8_t *bytes, size_t length,
                                         double elapsed_seconds);
MidiResult midi_recorder_record_packet_bytes(MidiRecorder *recorder,
                                             const uint8_t *bytes,
                                             size_t byte_count,
                                             double elapsed_seconds);

MusicSequence midi_recorder_sequence(const MidiRecorder *recorder);
unsigned int midi_recorder_captured_events(const MidiRecorder *recorder);
unsigned int midi_recorder_ignored_messages(const MidiRecorder *recorder);

#endif

#ifndef MIDI_SEQUENCE_H
#define MIDI_SEQUENCE_H

#include "midi_result.h"

#include <AudioToolbox/AudioToolbox.h>

#include <stddef.h>
#include <stdint.h>

typedef struct MidiSequenceEvent {
  double seconds;
  size_t length;
  uint8_t inline_bytes[3];
  uint8_t *data;
  unsigned long order;
} MidiSequenceEvent;

typedef struct MidiSequenceEventList {
  MidiSequenceEvent *items;
  size_t count;
  size_t capacity;
  unsigned long next_order;
} MidiSequenceEventList;

void midi_sequence_event_list_init(MidiSequenceEventList *events);
void midi_sequence_event_list_free(MidiSequenceEventList *events);

MidiResult midi_sequence_event_list_append(MidiSequenceEventList *events,
                                           double seconds,
                                           const uint8_t *data,
                                           size_t length);
MidiResult midi_sequence_load_file(const char *path, MusicSequence *sequence);
MidiResult midi_sequence_collect_events(MusicSequence sequence,
                                        MidiSequenceEventList *events,
                                        double *out_total_seconds);
MidiResult midi_sequence_load_events_from_file(const char *path,
                                               MidiSequenceEventList *events,
                                               double *out_total_seconds);

#endif

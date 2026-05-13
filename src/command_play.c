#include "command_play.h"

#include "app_support.h"
#include "midi_output.h"
#include "midi_sequence.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_midi_result(MidiResult result) {
  const char *operation =
      result.operation != NULL ? result.operation : "operation";

  switch (result.code) {
  case MIDI_RESULT_OK:
    return;
  case MIDI_RESULT_INVALID_ARGUMENT:
    fprintf(stderr, "%s failed: invalid argument\n", operation);
    return;
  case MIDI_RESULT_NO_MEMORY:
    fprintf(stderr, "%s failed: out of memory\n", operation);
    return;
  case MIDI_RESULT_OSSTATUS:
    log_osstatus_error(operation, result.os_status);
    return;
  }
}

static void wait_with_status(const char *mode, uint64_t started_nanos,
                             double target_seconds, bool has_total,
                             double total_seconds, uint64_t last_rx_nanos,
                             uint64_t last_tx_nanos) {
  while (!g_stop_requested) {
    uint64_t now_nanos = host_time_now_nanos();
    double elapsed_seconds = (double)(now_nanos - started_nanos) / 1000000000.0;
    if (elapsed_seconds >= target_seconds) {
      return;
    }

    draw_status_line(mode, elapsed_seconds, has_total, total_seconds,
                     activity_is_active(now_nanos, last_rx_nanos),
                     activity_is_active(now_nanos, last_tx_nanos));
    usleep(20000);
  }
}

static int parse_play_request(const char *destination_arg,
                              long *destination_index) {
  *destination_index = 0;

  if (destination_arg != NULL &&
      !parse_long_arg(destination_arg, destination_index)) {
    fputs("destination-index must be an integer\n", stderr);
    return 0;
  }

  return 1;
}

static void dispose_playback_resources(MidiOutput *output,
                                       MidiSequenceEventList *events) {
  midi_output_close(output);
  midi_sequence_event_list_free(events);
}

static int play_events(MidiOutput *output, MIDIEndpointRef destination,
                       MidiSequenceEventList *events, double total_seconds) {
  uint64_t started_nanos = host_time_now_nanos();
  uint64_t last_tx_nanos = 0;
  MidiResult result;

  for (size_t i = 0; i < events->count && !g_stop_requested; ++i) {
    wait_with_status("PLAY", started_nanos, events->items[i].seconds, true,
                     total_seconds, 0, last_tx_nanos);
    if (g_stop_requested) {
      break;
    }
    result = midi_output_send(output, destination, events->items[i].data,
                              events->items[i].length);
    if (!midi_result_is_ok(result)) {
      print_midi_result(result);
      return 0;
    }

    last_tx_nanos = host_time_now_nanos();
    draw_status_line("PLAY", events->items[i].seconds, true, total_seconds,
                     false, true);
  }

  wait_with_status("PLAY", started_nanos, total_seconds, true, total_seconds, 0,
                   last_tx_nanos);
  draw_status_line("PLAY", total_seconds, true, total_seconds, false, false);
  putchar('\n');
  return 1;
}

int command_play(const char *input_path, const char *destination_arg) {
  long destination_index;

  if (!parse_play_request(destination_arg, &destination_index)) {
    return 1;
  }

  MIDIEndpointRef destination = get_destination_by_index(destination_index);
  if (destination == 0) {
    fputs("invalid destination index or no MIDI destinations are available\n",
          stderr);
    return 1;
  }

  char destination_name[256];
  get_endpoint_name(destination, destination_name, sizeof(destination_name));

  MidiOutput output;
  MidiSequenceEventList events;
  midi_output_init(&output);
  midi_sequence_event_list_init(&events);

  MidiResult result;
  double total_seconds = 0.0;
  result =
      midi_sequence_load_events_from_file(input_path, &events, &total_seconds);
  if (!midi_result_is_ok(result)) {
    print_midi_result(result);
    dispose_playback_resources(&output, &events);
    return 1;
  }

  result = midi_output_open(&output, CFSTR("midi-capture-playback-client"),
                            CFSTR("midi-capture-output"));
  if (!midi_result_is_ok(result)) {
    print_midi_result(result);
    dispose_playback_resources(&output, &events);
    return 1;
  }

  g_stop_requested = 0;
  signal(SIGINT, on_sigint);

  printf("Playing %s to destination [%ld] %s...\n", input_path,
         destination_index, destination_name);

  int ok = play_events(&output, destination, &events, total_seconds);
  dispose_playback_resources(&output, &events);
  return ok ? 0 : 1;
}

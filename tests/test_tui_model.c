#include "tui_model.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_midi_filename_match_is_case_insensitive(void) {
  assert(tui_is_midi_filename("take.mid") == 1);
  assert(tui_is_midi_filename("TAKE.MID") == 1);
  assert(tui_is_midi_filename("take.midi") == 0);
}

static void test_recording_name_uses_timestamp_shape(void) {
  struct tm local_time;
  char name[32];

  memset(&local_time, 0, sizeof(local_time));
  local_time.tm_year = 126;
  local_time.tm_mon = 4;
  local_time.tm_mday = 11;
  local_time.tm_hour = 14;
  local_time.tm_min = 5;
  local_time.tm_sec = 9;

  assert(tui_format_recording_name(&local_time, name, sizeof(name)) == 1);
  assert(strcmp(name, "20260511140509.mid") == 0);
}

static void test_join_path_avoids_double_separator(void) {
  char path[64];

  assert(tui_join_path(path, sizeof(path), "/tmp/recordings", "take.mid") == 1);
  assert(strcmp(path, "/tmp/recordings/take.mid") == 0);

  assert(tui_join_path(path, sizeof(path), "/tmp/recordings/", "take.mid") ==
         1);
  assert(strcmp(path, "/tmp/recordings/take.mid") == 0);
}

static void test_format_midi_bytes_renders_hex_stream(void) {
  const uint8_t bytes[] = {0x90, 0x3C, 0x64};
  char text[32];

  tui_format_midi_bytes(text, sizeof(text), bytes, sizeof(bytes));

  assert(strcmp(text, "90 3C 64") == 0);
}

static void test_format_clock_time_matches_status_style(void) {
  char text[16];

  tui_format_clock_time(text, sizeof(text), 61.2);

  assert(strcmp(text, "01:01.2") == 0);
}

int main(void) {
  test_midi_filename_match_is_case_insensitive();
  test_recording_name_uses_timestamp_shape();
  test_join_path_avoids_double_separator();
  test_format_midi_bytes_renders_hex_stream();
  test_format_clock_time_matches_status_style();
  return 0;
}

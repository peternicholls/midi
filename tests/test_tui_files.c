#include "tui_files.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void touch_file(const char *path) {
  FILE *file = fopen(path, "wb");

  assert(file != NULL);
  fclose(file);
}

static void make_file(const char *directory, const char *name) {
  char path[PATH_MAX];

  assert(tui_join_path(path, sizeof(path), directory, name) == 1);
  touch_file(path);
}

static void remove_file(const char *directory, const char *name) {
  char path[PATH_MAX];

  assert(tui_join_path(path, sizeof(path), directory, name) == 1);
  assert(unlink(path) == 0);
}

static void test_midi_filename_match_is_case_insensitive(void) {
  assert(tui_is_midi_filename("take.mid") == 1);
  assert(tui_is_midi_filename("TAKE.MID") == 1);
  assert(tui_is_midi_filename("take.midi") == 0);
  assert(tui_is_midi_filename("mid") == 0);
  assert(tui_is_midi_filename(NULL) == 0);
}

static void test_join_path_avoids_double_separator(void) {
  char path[64];
  char small_path[8];

  assert(tui_join_path(path, sizeof(path), "/tmp/recordings", "take.mid") == 1);
  assert(strcmp(path, "/tmp/recordings/take.mid") == 0);

  assert(tui_join_path(path, sizeof(path), "/tmp/recordings/", "take.mid") ==
         1);
  assert(strcmp(path, "/tmp/recordings/take.mid") == 0);

  assert(tui_join_path(path, sizeof(path), ".", "take.mid") == 1);
  assert(strcmp(path, "take.mid") == 0);

  assert(tui_join_path(small_path, sizeof(small_path), "/tmp", "take.mid") ==
         0);
}

static void test_scan_filters_sorts_and_preserves_selection(void) {
  char template_path[] = "/tmp/midi-tui-files-XXXXXX";
  char *directory = mkdtemp(template_path);
  TuiFileList files;
  TuiFileScanResult result;

  assert(directory != NULL);
  make_file(directory, "20260512000001.mid");
  make_file(directory, "notes.txt");
  make_file(directory, "20260512000002.mid");
  make_file(directory, "take.midi");

  tui_file_list_init(&files);
  result = tui_file_list_scan(&files, directory);
  assert(tui_file_scan_result_is_ok(result));
  assert(files.count == 2);
  assert(strcmp(files.items[0].name, "20260512000002.mid") == 0);
  assert(strcmp(files.items[1].name, "20260512000001.mid") == 0);
  assert(strcmp(tui_file_list_selected(&files)->name, "20260512000002.mid") ==
         0);

  files.selected = 1;
  make_file(directory, "20260512000003.mid");
  result = tui_file_list_scan(&files, directory);
  assert(tui_file_scan_result_is_ok(result));
  assert(files.count == 3);
  assert(strcmp(files.items[0].name, "20260512000003.mid") == 0);
  assert(strcmp(files.items[2].name, "20260512000001.mid") == 0);
  assert(files.selected == 2);

  tui_file_list_dispose(&files);
  assert(files.items == NULL);
  assert(files.count == 0);
  assert(files.selected == 0);

  remove_file(directory, "20260512000001.mid");
  remove_file(directory, "20260512000002.mid");
  remove_file(directory, "20260512000003.mid");
  remove_file(directory, "notes.txt");
  remove_file(directory, "take.midi");
  assert(rmdir(directory) == 0);
}

static void test_missing_directory_scans_as_empty_list(void) {
  TuiFileList files;
  TuiFileScanResult result;

  tui_file_list_init(&files);
  result = tui_file_list_scan(&files, "/tmp/midi-tui-files-does-not-exist");
  assert(tui_file_scan_result_is_ok(result));
  assert(files.items == NULL);
  assert(files.count == 0);
  assert(files.selected == 0);
}

int main(void) {
  test_midi_filename_match_is_case_insensitive();
  test_join_path_avoids_double_separator();
  test_scan_filters_sorts_and_preserves_selection();
  test_missing_directory_scans_as_empty_list();
  return 0;
}

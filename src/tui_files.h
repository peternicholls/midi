#ifndef TUI_FILES_H
#define TUI_FILES_H

#include <limits.h>
#include <stddef.h>

typedef struct TuiFileEntry {
  char name[NAME_MAX + 1];
  char path[PATH_MAX];
} TuiFileEntry;

typedef struct TuiFileList {
  TuiFileEntry *items;
  size_t count;
  size_t selected;
} TuiFileList;

typedef enum TuiFileScanCode {
  TUI_FILE_SCAN_OK = 0,
  TUI_FILE_SCAN_INVALID_ARGUMENT,
  TUI_FILE_SCAN_NO_MEMORY,
  TUI_FILE_SCAN_OPEN_FAILED,
  TUI_FILE_SCAN_PATH_TOO_LONG
} TuiFileScanCode;

typedef struct TuiFileScanResult {
  TuiFileScanCode code;
  int system_error;
  const char *operation;
} TuiFileScanResult;

static inline int tui_file_scan_result_is_ok(TuiFileScanResult result) {
  return result.code == TUI_FILE_SCAN_OK;
}

void tui_file_list_init(TuiFileList *files);
void tui_file_list_dispose(TuiFileList *files);
const TuiFileEntry *tui_file_list_selected(const TuiFileList *files);

TuiFileScanResult tui_file_list_scan(TuiFileList *files, const char *directory);

int tui_is_midi_filename(const char *name);
int tui_join_path(char *buffer, size_t buffer_size, const char *directory,
                  const char *name);

#endif

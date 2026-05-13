#include "tui_files.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TuiFileScanResult scan_result(TuiFileScanCode code, int system_error,
                                     const char *operation) {
  TuiFileScanResult result = {code, system_error, operation};
  return result;
}

static int compare_file_entries_descending(const void *left,
                                           const void *right) {
  const TuiFileEntry *a = (const TuiFileEntry *)left;
  const TuiFileEntry *b = (const TuiFileEntry *)right;

  return strcmp(b->name, a->name);
}

void tui_file_list_init(TuiFileList *files) {
  if (files == NULL) {
    return;
  }
  files->items = NULL;
  files->count = 0;
  files->selected = 0;
}

void tui_file_list_dispose(TuiFileList *files) {
  if (files == NULL) {
    return;
  }
  free(files->items);
  tui_file_list_init(files);
}

const TuiFileEntry *tui_file_list_selected(const TuiFileList *files) {
  if (files == NULL || files->items == NULL ||
      files->selected >= files->count) {
    return NULL;
  }
  return &files->items[files->selected];
}

int tui_is_midi_filename(const char *name) {
  size_t length;

  if (name == NULL) {
    return 0;
  }
  length = strlen(name);
  if (length < 4) {
    return 0;
  }
  return tolower((unsigned char)name[length - 4]) == '.' &&
         tolower((unsigned char)name[length - 3]) == 'm' &&
         tolower((unsigned char)name[length - 2]) == 'i' &&
         tolower((unsigned char)name[length - 1]) == 'd';
}

int tui_join_path(char *buffer, size_t buffer_size, const char *directory,
                  const char *name) {
  int written;

  if (buffer == NULL || buffer_size == 0 || directory == NULL || name == NULL) {
    return 0;
  }

  if (directory[0] == '\0' || strcmp(directory, ".") == 0) {
    written = snprintf(buffer, buffer_size, "%s", name);
  } else if (directory[strlen(directory) - 1] == '/') {
    written = snprintf(buffer, buffer_size, "%s%s", directory, name);
  } else {
    written = snprintf(buffer, buffer_size, "%s/%s", directory, name);
  }

  return written >= 0 && (size_t)written < buffer_size;
}

TuiFileScanResult tui_file_list_scan(TuiFileList *files,
                                     const char *directory_path) {
  DIR *directory;
  struct dirent *entry;
  TuiFileEntry *items = NULL;
  size_t count = 0;
  size_t capacity = 0;
  char selected_name[NAME_MAX + 1] = "";

  if (files == NULL || directory_path == NULL) {
    return scan_result(TUI_FILE_SCAN_INVALID_ARGUMENT, 0, "tui_file_list_scan");
  }

  if (files->count > 0 && files->selected < files->count) {
    snprintf(selected_name, sizeof(selected_name), "%s",
             files->items[files->selected].name);
  }

  directory = opendir(directory_path);
  if (directory == NULL) {
    int error_number = errno;
    tui_file_list_dispose(files);
    if (error_number == ENOENT) {
      return scan_result(TUI_FILE_SCAN_OK, 0, NULL);
    }
    return scan_result(TUI_FILE_SCAN_OPEN_FAILED, error_number, "opendir");
  }

  while ((entry = readdir(directory)) != NULL) {
    if (!tui_is_midi_filename(entry->d_name)) {
      continue;
    }

    if (count == capacity) {
      size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
      TuiFileEntry *resized =
          (TuiFileEntry *)realloc(items, new_capacity * sizeof(*resized));
      if (resized == NULL) {
        closedir(directory);
        free(items);
        return scan_result(TUI_FILE_SCAN_NO_MEMORY, 0, "realloc");
      }
      items = resized;
      capacity = new_capacity;
    }

    memset(&items[count], 0, sizeof(items[count]));
    snprintf(items[count].name, sizeof(items[count].name), "%s", entry->d_name);
    if (!tui_join_path(items[count].path, sizeof(items[count].path),
                       directory_path, entry->d_name)) {
      closedir(directory);
      free(items);
      return scan_result(TUI_FILE_SCAN_PATH_TOO_LONG, ENAMETOOLONG,
                         "tui_join_path");
    }
    count += 1;
  }

  closedir(directory);

  if (count > 1) {
    qsort(items, count, sizeof(items[0]), compare_file_entries_descending);
  }

  free(files->items);
  files->items = items;
  files->count = count;
  files->selected = 0;

  if (selected_name[0] != '\0') {
    for (size_t i = 0; i < count; ++i) {
      if (strcmp(files->items[i].name, selected_name) == 0) {
        files->selected = i;
        break;
      }
    }
  }

  return scan_result(TUI_FILE_SCAN_OK, 0, NULL);
}

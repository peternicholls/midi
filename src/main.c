#include "app_support.h"
#include "command_list.h"
#include "command_play.h"
#include "command_record.h"
#include "command_tui.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *program_name) {
  fprintf(stderr,
          "Usage:\n"
          "  %s --version\n"
          "  %s list\n"
          "  %s tui [recordings-dir]\n"
          "  %s record <output.mid> [seconds] [source-index]\n"
          "  %s play <input.mid> [destination-index]\n",
          program_name, program_name, program_name, program_name,
          program_name);
}

static int handle_record_command(int argc, char **argv) {
  if (argc < 3 || argc > 5) {
    print_usage(argv[0]);
    return 1;
  }

  return command_record(argv[2], argc > 3 ? argv[3] : NULL,
                        argc > 4 ? argv[4] : NULL);
}

static int handle_play_command(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    print_usage(argv[0]);
    return 1;
  }

  return command_play(argv[2], argc > 3 ? argv[3] : NULL);
}

static int dispatch_command(int argc, char **argv) {
  if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "version") == 0) {
    printf("midi-capture %s\n", app_version());
    return 0;
  }

  if (strcmp(argv[1], "list") == 0) {
    return command_list();
  }

  if (strcmp(argv[1], "tui") == 0) {
    if (argc > 3) {
      print_usage(argv[0]);
      return 1;
    }
    return command_tui(argc > 2 ? argv[2] : NULL);
  }

  if (strcmp(argv[1], "record") == 0) {
    return handle_record_command(argc, argv);
  }

  if (strcmp(argv[1], "play") == 0) {
    return handle_play_command(argc, argv);
  }

  print_usage(argv[0]);
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  return dispatch_command(argc, argv);
}

CC := clang
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Isrc -Iinclude
APP_LDFLAGS := -lcurses -framework CoreMIDI -framework AudioToolbox -framework CoreFoundation -framework CoreAudio

APP := midi-capture
APP_SRCS := src/main.c src/app_support.c src/command_list.c src/command_record.c src/command_play.c src/command_tui.c src/midi_parser.c src/status_line.c src/tui_model.c
TESTS := test_midi_parser test_status_line test_tui_model

.PHONY: all clean test

all: $(APP)

$(APP): $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(APP_LDFLAGS)

test_midi_parser: tests/test_midi_parser.c src/midi_parser.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_parser.c src/midi_parser.c

test_status_line: tests/test_status_line.c src/status_line.c
	$(CC) $(CFLAGS) -o $@ tests/test_status_line.c src/status_line.c

test_tui_model: tests/test_tui_model.c src/tui_model.c
	$(CC) $(CFLAGS) -o $@ tests/test_tui_model.c src/tui_model.c

test: $(TESTS)
	./test_midi_parser
	./test_status_line
	./test_tui_model

clean:
	rm -f $(APP) $(TESTS) output.mid

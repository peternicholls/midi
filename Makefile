CC := clang
VERSION := $(shell tr -d '[:space:]' < VERSION)
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Isrc -DAPP_VERSION=\"$(VERSION)\"
APP_LDFLAGS := -lcurses -framework CoreMIDI -framework AudioToolbox -framework CoreFoundation -framework CoreAudio

APP := midi-capture
APP_SRCS := src/main.c src/app_support.c src/command_list.c src/command_record.c src/command_play.c src/command_tui.c src/midi_describe.c src/midi_output.c src/midi_parser.c src/midi_recorder.c src/midi_sequence.c src/status_line.c src/tui_files.c src/tui_log.c src/tui_model.c src/tui_render.c
TESTS := test_midi_describe test_midi_parser test_midi_recorder test_midi_sequence test_status_line test_tui_files test_tui_log test_tui_model

# Refactor sprint convention: add each new app module .c to APP_SRCS, add each
# new test binary to TESTS, and give that test a focused build rule pairing the
# test source with the module under test.
.PHONY: all clean test

all: $(APP)

$(APP): $(APP_SRCS)
	$(CC) $(CFLAGS) -o $@ $(APP_SRCS) $(APP_LDFLAGS)

test_midi_describe: tests/test_midi_describe.c src/midi_describe.c src/midi_parser.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_describe.c src/midi_describe.c src/midi_parser.c

test_midi_parser: tests/test_midi_parser.c src/midi_parser.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_parser.c src/midi_parser.c

test_midi_recorder: tests/test_midi_recorder.c src/midi_recorder.c src/midi_parser.c src/midi_sequence.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_recorder.c src/midi_recorder.c src/midi_parser.c src/midi_sequence.c $(APP_LDFLAGS)

test_midi_sequence: tests/test_midi_sequence.c src/midi_sequence.c src/midi_parser.c
	$(CC) $(CFLAGS) -o $@ tests/test_midi_sequence.c src/midi_sequence.c src/midi_parser.c $(APP_LDFLAGS)

test_status_line: tests/test_status_line.c src/status_line.c
	$(CC) $(CFLAGS) -o $@ tests/test_status_line.c src/status_line.c

test_tui_files: tests/test_tui_files.c src/tui_files.c
	$(CC) $(CFLAGS) -o $@ tests/test_tui_files.c src/tui_files.c

test_tui_log: tests/test_tui_log.c src/tui_log.c
	$(CC) $(CFLAGS) -o $@ tests/test_tui_log.c src/tui_log.c

test_tui_model: tests/test_tui_model.c src/tui_model.c
	$(CC) $(CFLAGS) -o $@ tests/test_tui_model.c src/tui_model.c

test: $(TESTS)
	./test_midi_describe
	./test_midi_parser
	./test_midi_recorder
	./test_midi_sequence
	./test_status_line
	./test_tui_files
	./test_tui_log
	./test_tui_model

clean:
	rm -f $(APP) $(TESTS) output.mid

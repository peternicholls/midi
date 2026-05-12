# MIDI Capture

Small macOS command-line tool in C that:

- lists available CoreMIDI sources and destinations
- records MIDI from a class-compliant USB MIDI interface into a Standard MIDI File
- plays that `.mid` file back to a chosen MIDI destination
- provides a terminal TUI mode for browsing takes, recording timestamped files, and previewing live MIDI traffic

## Build

```sh
make
```

## Test

```sh
make test
```

## Usage

List MIDI ports:

```sh
./midi-capture list
```

Record from source `0` for 10 seconds:

```sh
./midi-capture record take.mid 10 0
```

Record until `Ctrl-C`:

```sh
./midi-capture record take.mid
```

Play the file back to destination `0`:

```sh
./midi-capture play take.mid 0
```

Launch the TUI with the current directory as the recordings destination:

```sh
./midi-capture tui
```

Launch the TUI with a dedicated recordings directory:

```sh
./midi-capture tui recordings
```

## TUI Mode

The TUI presents three panes:

- a recordings list sourced from the selected output directory
- a MIDI event view for the currently selected file, with left and right stepping through the sequence
- a live stream pane that shows incoming `RX` bytes whenever MIDI arrives on source `0`, plus outgoing `TX` bytes during playback

Default file naming uses `YYYYMMDDhhmmss.mid` in the selected recordings directory.

Keys:

- `r` starts recording to a new timestamped `.mid` file in the current recordings directory
- `s` stops the active recording or playback transport
- `space` pauses or resumes the active recording or playback transport
- `p` plays the currently selected file from the currently selected MIDI event
- `up` and `down` change the selected `.mid` file
- `left` and `right` step through the selected file's MIDI event sequence
- `o` changes the recordings directory from inside the TUI
- `q` exits the TUI; an active recording is stopped and saved first

## Notes

- The tool is intentionally small and targets MIDI 1.0 byte-stream devices on macOS.
- It records channel voice messages, MIDI running-status channel messages, and SysEx into the MIDI file.
- It ignores MIDI clock, active sensing, and other unsupported or incomplete system messages.
- During `record`, the terminal shows `REC`, elapsed time, and an `RX.` flash when MIDI arrives.
- During `play`, the terminal shows `PLAY`, elapsed and total time, and a `TX.` flash when MIDI is sent.
- In `tui` mode, source `0` is monitored continuously for live `RX` logging, recording also uses source `0`, and playback uses destination `0`.

# Manual Smoke Test Checklist

Use this checklist for CoreMIDI behavior that is not covered by unit tests.
Run it after a clean build from the repository root.

## Current Command Behavior

- `./midi list`
  - Prints available MIDI sources.
  - Prints available MIDI destinations.
  - Exits successfully even when no endpoints are present.
- `./midi record <path> [seconds] [source-index]`
  - With a valid source, records MIDI input to `<path>`.
  - With `seconds` omitted, records until Ctrl-C.
  - With `seconds` provided, stops after that duration.
  - Rejects invalid `seconds` or `source-index` values with readable stderr.
- `./midi play <path> [destination-index]`
  - Loads a MIDI file and sends events to the selected destination.
  - Defaults to destination index `0`.
  - Rejects invalid destination indices and unreadable files with readable
    stderr.
- `./midi tui [recordings-dir]`
  - Opens the curses UI.
  - Defaults the recordings directory when no path is provided.
  - Lists `.mid` files from the recordings directory.
  - Shows live MIDI input, recording state, playback state, and status text.

## Hardware Or Virtual-MIDI Checks

1. Connect a MIDI source and destination, or create a virtual CoreMIDI route.
2. Run `./midi list` and confirm the source and destination names are
   visible.
3. Run `./midi record recordings/smoke.mid 3 0`, send a few MIDI
   messages, and confirm the file is created.
4. Run `./midi play recordings/smoke.mid 0` and confirm the destination
   receives MIDI events.
5. Run `./midi tui recordings`.
6. In the TUI, confirm the recordings list includes `smoke.mid`.
7. Send live MIDI input and confirm log rows appear.
8. Start recording, pause, resume, then stop. Confirm a timestamped `.mid` file
   appears in the recordings list.
9. Select a recording and start playback. Confirm MIDI is sent to destination
   `0`.
10. Quit the TUI and confirm the terminal returns to normal input/echo behavior.

## Known Automated-Test Gaps

- CoreMIDI endpoint availability and packet delivery depend on local hardware
  or virtual MIDI setup.
- Curses rendering requires an interactive terminal for meaningful validation.
- Playback audio/MIDI side effects are verified manually by observing the
  destination endpoint.

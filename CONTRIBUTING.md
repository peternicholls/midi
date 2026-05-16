# Contributing

Thanks for taking the time to improve MIDI Capture. This project is small on
purpose, so contributions should keep changes narrow, reviewable, and easy to
verify.

## License

By submitting a contribution, you agree that it is licensed under the
[Apache License 2.0](LICENSE). Do not include code, recordings, samples, or
other assets unless you have the right to contribute them under that license.

## Development Setup

This project targets macOS and uses CoreMIDI, AudioToolbox, CoreFoundation,
CoreAudio, and curses.

```sh
make
make test
```

Use `./midi-capture --version` to confirm the binary was built with the version
from `VERSION`.

## Before Opening A Pull Request

- Keep the diff focused on one issue or feature.
- Add or update regression tests for behavior changes.
- Run `make clean`, `make`, and `make test`.
- Run manual smoke tests when changing CLI behavior, playback, recording, MIDI
  device handling, or TUI workflows.
- Update `README.md`, `CHANGELOG.md`, and other docs when user-facing behavior
  changes.
- Follow the versioning and release rules in
  [docs/versioning.md](docs/versioning.md) when preparing releases.

## Coding Guidelines

- Prefer existing helpers and module boundaries before adding new abstractions.
- Keep C code compatible with the repository's C11 build flags.
- Use clear ownership rules for allocated memory and document non-obvious
  ownership transfers in headers.
- Avoid new dependencies unless they are necessary and explicitly justified.
- Keep terminal behavior usable without assuming specific MIDI hardware is
  connected.

## Commit Messages

Use the Lore Commit Protocol described in `AGENTS.md`: start with an intent
line that explains why the change was made, then add useful trailers such as
`Tested:`, `Not-tested:`, `Confidence:`, and `Scope-risk:` where they add
context.

## Pull Request Checklist

- [ ] `make clean` passes.
- [ ] `make` passes.
- [ ] `make test` passes.
- [ ] User-facing changes are documented.
- [ ] `CHANGELOG.md` includes notable changes.
- [ ] Manual smoke tests are recorded when hardware, recording, playback, or
      TUI behavior changed.

## Reporting Issues

When reporting a bug, include:

- macOS version
- command or TUI workflow used
- MIDI device or virtual MIDI setup
- expected behavior
- actual behavior
- any relevant `.mid` file details, without sharing private recordings

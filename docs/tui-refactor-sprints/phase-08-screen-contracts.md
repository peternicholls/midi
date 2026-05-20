# Phase 8 Screen Contracts

**Date:** 2026-05-19

**Status:** Draft for planning review

**Depends on:**

- `docs/tui-refactor-sprints/phase-08-mockups-v3/index.html`
- `docs/tui-refactor-sprints/phase-08-mockups-v3/styles.css`
- `docs/tui-refactor-sprints/tui-c-interop-language.md`

## Purpose

Define fixed-size terminal contracts for the V3 screens so the C implementation
has an exact text-mode target.

These are not code samples. They are planning contracts.

## Contract Rules

1. `90x24` is the compact review target.
2. `120x36` is the comfortable desktop review target.
3. Every screen must name visible regions, columns, and state ownership.
4. Every screen must identify what may clip and what must remain visible.
5. If a screen needs a compact fallback, the fallback must be written here.

## Shared Structural Contract

All Phase 8 screens share this base shape.

```text
top row      : rail
second row   : keybar
body         : files | workpane
bottom row   : footer
overlay mode : centered overlay on top of dimmed base screen
```

Shared region intent:

- `rail`: app identity, current mode/transport emphasis, I/O activity,
  endpoint/scope identity, version
- `keybar`: highest-value available actions only
- `files`: persistent orientation and file selection
- `workpane`: active task surface
- `footer`: state summary, not command discovery

## Global Width Priorities

When space is constrained, preserve visibility in this order:

1. mode token
2. RX/TX state
3. selected/current row marker
4. `#/` counts
5. event and note identity
6. numeric value text
7. raw bytes
8. extended descriptions and hints

## Sequence Contract

### Sequence @90x24

Contract:

```text
screen sequence @90x24 {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane enter-load space-play-stop 0-start ,-settings d-directory }
  files { title path selected-file other-files }
  workpane sequence {
    title sequence-name event-count
    table marker number time ch event target value raw
    selection sequence-selection
  }
  footer { loaded-summary tempo playhead }
}
```

Column contract:

| Column | Width target | Must remain visible |
| --- | --- | --- |
| marker | 2 | yes |
| `#/` | 8 | yes |
| time | 8 | yes |
| ch | 4 | yes |
| event | 11-12 | yes |
| target | 9-10 | yes |
| value | 10-12 | yes |
| raw | remaining | yes, but may clip |

Compact rules:

- no extra `description` column
- path row remains present in file pane
- footer may abbreviate secondary status text, but loaded summary must remain
- `value` keeps numeric text even if any mini-bar is shortened

### Sequence @120x36

Contract:

```text
screen sequence @120x36 {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane enter-load space-play-stop 0-start ,-settings d-directory }
  files { title path selected-file file-list }
  workpane sequence {
    title sequence-name event-count
    table marker number time ch event target value raw
    selection sequence-selection
  }
  footer { loaded-summary tempo playhead duration }
}
```

Column contract:

| Column | Width target |
| --- | --- |
| marker | 2 |
| `#/` | 8 |
| time | 10 |
| ch | 4 |
| event | 15 |
| target | 12 |
| value | 22 |
| raw | remaining |

Wide rules:

- use the extra width to improve `value` and `raw`
- do not reintroduce a separate Sequence `description` column unless approved

## Live Player Contract

### Live Player @90x24

Contract:

```text
screen live-player @90x24 {
  rail { brand mode rx tx source channel-scope version }
  keybar { tab-pane space-hold c-channel ,-settings d-directory q-quit }
  files { title path selected-file info-row }
  workpane live-player {
    title source-name fade-policy
    controls scope mod pressure pitch last-rx
    table number note state velocity pressure bend-mod age
  }
  footer { live-summary note-format fade-timeout }
}
```

Ownership rules:

- `channel-scope` must be explicit app state
- controls row must render from that scope state, not inferred note activity

Column contract:

| Column | Width target | Must remain visible |
| --- | --- | --- |
| `#/` | 7 | yes |
| note | 7 | yes |
| state | 8 | yes |
| velocity | 12-14 | yes |
| pressure | 12-14 | yes |
| bend/mod | 12-14 | yes |
| age | remaining | yes |

Compact rules:

- controls row must remain present
- values remain numeric even if bars are shortened
- file pane may show an informational muted row indicating that file selection
  is orienting rather than driving live playback

### Live Player @120x36

Contract:

```text
screen live-player @120x36 {
  rail { brand mode rx tx source channel-scope version }
  keybar { tab-pane space-hold c-channel ,-settings d-directory q-quit }
  files { title path selected-file file-list }
  workpane live-player {
    title source-name fade-policy
    controls scope mod pressure pitch last-rx
    table number note state velocity pressure bend-mod age
  }
  footer { live-summary middle-c note-format fade-timeout }
}
```

Column contract:

| Column | Width target |
| --- | --- |
| `#/` | 7 |
| note | 7 |
| state | 8 |
| velocity | 22 |
| pressure | 24 |
| bend/mod | 16 |
| age | remaining |

Wide rules:

- `velocity` and `pressure` should be the first columns to gain breathing room
- controls row should visually echo the mockup proportions from `.controls`

## Live Diagnostic Contract

### Live Diagnostic @90x24

Contract:

```text
screen live-diagnostic @90x24 {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane f-filter space-pause-log ,-settings d-directory }
  files { title path selected-file info-row }
  workpane live-diagnostic {
    title log-window-size
    table time dir ch event target value raw description
  }
  footer { diagnostic-summary filter-state pause-state }
}
```

Column contract:

| Column | Width target | Compact behavior |
| --- | --- | --- |
| time | 8-9 | must remain visible |
| dir | 4-5 | must remain visible |
| ch | 4 | must remain visible |
| event | 10-11 | may clip within cell |
| target | 8-10 | may clip within cell |
| value | 10-12 | keep numeric text |
| raw | 12-14 | may clip |
| description | remaining | may clip most aggressively |

Compact rules:

- `description` stays present, but is the first field to lose width
- `raw` remains visible even when clipped

### Live Diagnostic @120x36

Contract:

```text
screen live-diagnostic @120x36 {
  rail { brand mode rx tx source destination version }
  keybar { tab-pane f-filter space-pause-log ,-settings d-directory }
  files { title path selected-file file-list }
  workpane live-diagnostic {
    title log-window-size
    table time dir ch event target value raw description
  }
  footer { diagnostic-summary filter-state pause-state }
}
```

Column contract:

| Column | Width target |
| --- | --- |
| time | 10 |
| dir | 5 |
| ch | 4 |
| event | 13 |
| target | 12 |
| value | 18 |
| raw | 15 |
| description | remaining |

Wide rules:

- use extra width to recover the readable explanatory tail of `description`
- keep `raw` secondary but visible

## Settings Overlay Contract

### Settings Overlay @90x24

Contract:

```text
overlay settings @90x24 {
  title Settings
  table setting value notes
  selection settings-selection
  actions up-down left-right enter-apply esc-close
}
```

Compact rules:

- `notes` stays present if width allows
- if `notes` must shrink, clip notes before clipping setting labels
- if extreme constraint forces reduction, `notes` may abbreviate but the overlay
  must stay three-column in planning intent

### Settings Overlay @120x36

Contract:

```text
overlay settings @120x36 {
  title Settings
  table setting value notes
  selection settings-selection
  actions up-down left-right enter-apply esc-close
}
```

Wide rules:

- notes should read like the V3 overlay hints, not internal implementation
  labels
- value and notes columns should both have comfortable clipping margins

## Rail Contract

Shared rail rules:

- `brand` always left-most
- `version` always right-aligned when width allows
- `mode` is a token, not body text
- `rx` and `tx` are compact activity tokens
- the final endpoint token may become `channel-scope` in Live Player instead of
  destination

Rail order:

```text
brand | mode | rx | tx | source | destination-or-scope | version
```

## Keybar Contract

Shared keybar rules:

- show only current high-value actions
- tokens should read like paired keycap + label phrases
- do not let the footer become the primary command-discovery surface again

## File Pane Contract

Shared file-pane rules:

- title must read `Files`
- path row is part of the V3 hierarchy and should be preserved
- selected file uses reverse-video equivalent
- loaded-file indication may be additional, but must not overpower selection

## Parity Checklist

The coded TUI cannot be called parity-complete until all of these are true.

### Shared

- rail composition matches the contract order
- keybar matches the screen’s intended actions
- file pane includes title and path row
- footer summarizes state instead of duplicating commands

### Sequence

- table columns match `#/ time ch event target value raw`
- no extra Sequence `description` column unless intentionally re-approved
- `#/` renders `current/total`

### Live Player

- rail shows explicit `channel-scope`
- controls row is present and state-owned
- `c-channel` is a real interaction, not mockup-only text

### Live Diagnostic

- `description` remains present on both target sizes
- direction and raw bytes are visible in the same row

### Settings

- `setting / value / notes` structure is preserved
- notes are human-facing hints, not implementation jargon

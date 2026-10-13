# Omawrite

A dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

Install via the Omarchy Package Repository via the `omawrite` package. It's installed by default in new installations of Omarchy (from Quattro forward).

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new Omawrite window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+Mouse wheel` increases the writing font size when scrolling up and decreases it when scrolling down.
- `Ctrl+?` shows the keyboard shortcut reference.

Unsaved drafts are recovered after an abnormal exit. Omawrite also watches open files
and warns before an external change can replace local work.

The font button in the footer picks the writing font from any installed text font,
and Omawrite remembers the choice. IBM Plex Mono is the default.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
Omawrite at the size it is designed around; larger and smaller sizes scale from there.

Ctrl+scroll adjusts only the writing area, leaving interface controls and the desktop
text size unchanged. Each wheel notch changes the font by 2px before desktop scaling,
within an 8–72px range. By default, new windows start at 20px; zoom persistence can
be enabled in the configuration below. Scrolling without Ctrl still scrolls the document.

## Configuration

Omawrite stores preferences in `$XDG_CONFIG_HOME/Omacom/omawrite.conf`, falling back
to `~/.config/Omacom/omawrite.conf` when `XDG_CONFIG_HOME` is unset. Edit the file
while Omawrite is closed; these options are read when each window starts.
Add the keys to an existing `[editor]` section if present, preserving other preferences.

```ini
[editor]
persistZoom=false
zoom=
scaleMarginsWithZoom=true
```

- `persistZoom`: set to `true` to restore the saved zoom in new windows and future
  sessions. Each zoom change writes `zoom` to disk immediately. The default is
  `false`: saved zoom is ignored, and zoom changes do not overwrite it.
- `zoom`: writing font size in pixels before desktop text scaling, from 8 to 72;
  fractional values are supported. Missing, empty, invalid, or out-of-range values
  use the 20px default.
- `scaleMarginsWithZoom`: defaults to `true`. Both side margins grow or shrink
  proportionally to zoom, keeping the writing area centered. Margins are capped
  to leave at least 360px for the editor when the window allows it. Set to `false`
  to keep the side margins independent of zoom; resizing the window, changing the
  font, or changing desktop text scaling can still change the layout.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `xdg-desktop-portal` and a portal backend

The IBM Plex Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright IBM Corp.

# autocopy

Highlight text anywhere and it lands in the clipboard, ready for `Ctrl+V`. Same behavior as Claude Code, for the whole desktop.

Highlighting text to paste over it still works: `Ctrl+V` replaces the highlight with what you copied before, not with the highlight itself.

## Install

```sh
git clone https://github.com/Edward-Pratt/autocopy.git && cd autocopy && ./install.sh
```

This installs `autocopy`, its systemd user service, and `autocopy-update` with its daily timer (see [Updates](#updates)). Running it again is safe.

Requirements:

- A Wayland compositor with the `ext-data-control-v1` protocol and a primary selection (recent Hyprland, Sway and KDE Plasma have both). On Hyprland, `misc:middle_click_paste` must be `true`: setting it to `false` turns off the primary selection.
- A C compiler, `make`, `pkg-config`, `wayland` and `wayland-protocols` 1.39+ (`sudo pacman -S base-devel wayland wayland-protocols`). Hyprland already pulls in the last two.
- A systemd-managed session, so `graphical-session.target` is active (for example Hyprland started with uwsm)

## Updates

Once a day a systemd user timer checks for a newer release and says so in a desktop notification. It never installs anything by itself. To install the newest release:

```sh
autocopy-update
```

It downloads the release from GitHub, checks it against the release's `SHA256SUMS`, and runs that release's `install.sh`, so run it from a terminal inside your Wayland session. `autocopy-update --check` only checks, and `autocopy-update --rollback` goes back to the release installed before. Releases are kept in `~/.local/share/autocopy/releases`. An install from a clone checked out at a release tag counts as that release; from any other commit, the first check offers the newest release. The updater follows stable releases only; it never installs a pre-release or the main branch.

## How it works

autocopy is a small daemon that watches the primary selection (the text you highlight) and owns the clipboard itself, so it sees every paste and decides at that moment what to hand over.

- Highlighting text starts a selection. The clipboard from before it is kept in memory, every type included, so an image or rich text you copied survives.
- Pasting while the highlight is still live, in the window it was made in, pastes that kept clipboard over the highlight. The highlight is dropped, even if you then move to another window.
- Anything else ends the selection and makes the highlight the clipboard: clicking elsewhere, pasting it into another window, or highlighting something different.
- Copying with `Ctrl+C` always wins over a highlight.

Paste-over only happens in an app that has been seen letting go of a highlight, for example when you click elsewhere in VS Code. Some apps, Firefox among them, keep the highlight as the primary selection after you click into another field. There a highlight never looks finished, so `Ctrl+V` always pastes the highlight. An app gets paste-over once it has let go of a highlight for the first time. autocopy remembers it by window class in `~/.local/state/autocopy/releasers`, one class per line; delete a line to make that app learn again.

Which window has focus comes from Hyprland's IPC socket. On another compositor there is no window check, so every paste pastes the highlight, as a plain select-to-copy tool would.

Text highlighted in a password field is marked sensitive and is not copied, and a password manager's clipboard is left with the manager so its own clearing keeps working. Launchers and other overlays are not windows, so highlighting text and pasting it into one counts as pasting in the same window: the paste gives the previous clipboard, not the highlight. Dragging a selection in an app that does not release the primary selection on mouse press merges the new drag into the previous highlight, so the previous highlight is not kept as the clipboard; most toolkits do release it.

## Uninstall

```sh
./uninstall.sh
```

This removes autocopy, `autocopy-update`, its timer and the releases it downloaded.

## Logs

```sh
journalctl --user -u autocopy
journalctl --user -u autocopy-update
```

## Releasing

Releases come from tags, made with [herald](https://github.com/Edward-Pratt/herald) on an up-to-date `main`:

```sh
herald release minor        # or patch, major, or 1.4.0
herald release minor --rc   # a release candidate, published as a pre-release
```

The tag is the version; no file holds it. `.github/workflows/release.yml` publishes the tag as a GitHub release with a source archive and its `SHA256SUMS`. A `vX.Y.Z` tag is a stable release that `autocopy-update` offers; a `-rc.N` tag is a pre-release it skips.

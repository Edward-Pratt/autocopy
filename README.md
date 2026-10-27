# autocopy

Highlight text anywhere and it lands in the clipboard, ready for `Ctrl+V`. Same behavior as Claude Code, for the whole desktop.

## Install

```sh
git clone https://github.com/Edward-Pratt/autocopy.git && cd autocopy && ./install.sh
```

This installs `autocopy`, its systemd user service, and `autocopy-update` with its daily timer (see [Updates](#updates)). Running it again is safe.

Requirements:

- A Wayland compositor that supports the primary selection (Hyprland, Sway, GNOME, KDE). On Hyprland, `misc:middle_click_paste` must be `true`: setting it to `false` turns off the primary selection.
- `wl-clipboard` (`sudo pacman -S wl-clipboard`)
- A systemd-managed session, so `graphical-session.target` is active (for example Hyprland started with uwsm)

## Updates

Once a day a systemd user timer checks for a newer release and says so in a desktop notification. It never installs anything by itself. To install the newest release:

```sh
autocopy-update
```

It downloads the release from GitHub, checks it against the release's `SHA256SUMS`, and runs that release's `install.sh`, so run it from a terminal inside your Wayland session. `autocopy-update --check` only checks, and `autocopy-update --rollback` goes back to the release installed before. Releases are kept in `~/.local/share/autocopy/releases`. An install from a clone checked out at a release tag counts as that release; from any other commit, the first check offers the newest release. The updater follows stable releases only; it never installs a pre-release or the main branch.

## How it works

`wl-paste --primary --watch` reports every change to the primary selection (the text you highlight). autocopy copies each new selection into the clipboard with `wl-copy`. A systemd user service runs it with your graphical session.

Clicking elsewhere clears the primary selection. autocopy ignores that, so the clipboard keeps the last text you highlighted. Text highlighted in a password field is marked sensitive and is not copied.

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

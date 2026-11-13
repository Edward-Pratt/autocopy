# autocopy

Highlight text anywhere and it lands in the clipboard, ready for `Ctrl+V`. Same behavior as Claude Code, for the whole desktop.

Highlighting text to paste over it still works: `Ctrl+V` replaces the highlight with what you copied before, not with the highlight itself.

## Install

```sh
git clone https://github.com/Edward-Pratt/autocopy.git && cd autocopy && ./install.sh
```

This installs `autocopy`, its systemd user service, `autocopy-update` with its daily timer (see [Updates](#updates)), and `autocopy-notify` with its launcher entry (see [Notifications](#notifications)). Running it again is safe.

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

## Notifications

autocopy can show a short desktop notification each time a highlight becomes the clipboard, so you can see that the copy went through. It is off by default. To turn it on or off, search "autocopy" in your app launcher and open **Autocopy notifications on/off**, or run:

```sh
autocopy-notify on        # or off, or toggle; with no argument it says which
```

The setting is the file `~/.config/autocopy/notify`: notifications are on while it exists. A change counts from the next copy, and updates keep it.

The notification shows the first 80 characters of the copied text on one line. In apps without paste-over (see [How it works](#how-it-works)), Firefox and Discord among them, the highlight is copied as soon as you stop dragging, and the notification follows right away. In apps with paste-over it appears when the highlight is copied: when you click elsewhere, switch windows, paste it into another window, or highlight something else. Pasting over a highlight copies nothing, so it shows nothing, and `Ctrl+C` copies are the app's own. Notifications are transient, so they stay out of the notification history, but they are on screen, screen shares included.

Notifications need `notify-send` (`sudo pacman -S libnotify`), a notification daemon, and Hyprland: without its window checks every highlight is copied while you drag, so there is no single moment to announce.

## How it works

autocopy is a small daemon that watches the primary selection (the text you highlight) and owns the clipboard itself, so it sees every paste and decides at that moment what to hand over.

- Highlighting text starts a selection. In an app with paste-over (below), the clipboard from before it is kept in memory, every type included, so an image or rich text you copied survives.
- Pasting while the highlight is still live, in the window it was made in, pastes that kept clipboard over the highlight. The highlight is dropped, even if you then move to another window.
- Anything else ends the selection and makes the highlight the clipboard: clicking elsewhere, pasting it into another window, or highlighting something different.
- In an app without paste-over, the highlight is the clipboard at once, as plain text, so `Ctrl+V` in the same app pastes it the first time. Apps such as Discord would otherwise paste the old clipboard's rich text.
- Copying with `Ctrl+C` always wins over a highlight.

Paste-over only happens in an app that has been seen letting go of a highlight, for example when you click elsewhere in VS Code. Some apps, Firefox among them, keep the highlight as the primary selection after you click into another field. There a highlight never looks finished, so it is copied at once and `Ctrl+V` always pastes it. An app gets paste-over once it has let go of a highlight for the first time. autocopy remembers it by window class in `~/.local/state/autocopy/releasers`, one class per line; delete a line to make that app learn again. Firefox is never learned: it once let go of a highlight with no click, and that one time turned paste-over on, so pasting a highlight into another Firefox field pasted the old clipboard.

Which window has focus comes from Hyprland's IPC socket. On another compositor there is no window check, so every paste pastes the highlight, as a plain select-to-copy tool would.

In Discord, Vesktop and Equibop, only text you drag over is copied. A click there can highlight hidden text: double-clicking a message to reply picks up its toolbar, timestamp or the message box's labels. So a highlight that appears all at once, from a double-click, triple-click or shift-click, is not copied and the clipboard stays as it was; `Ctrl+C` still copies it. A drag sets the highlight again with every mouse move, so once it changes it is copied as usual. The toolbar's labels ("Click to react", "Add Reaction", "Reply" and so on) are also dropped from a dragged highlight, in English only. All this needs Hyprland, to know the highlight was made in Discord.

Text highlighted in a password field is marked sensitive and is not copied, and a password manager's clipboard is left with the manager so its own clearing keeps working. Launchers and other overlays are not windows, so highlighting text in an app with paste-over and pasting it into one counts as pasting in the same window: the paste gives the previous clipboard, not the highlight. Text highlighted before autocopy starts is not copied. Dragging a selection in an app that does not release the primary selection on mouse press merges the new drag into the previous highlight, so the previous highlight is not kept as the clipboard; most toolkits do release it.

## Uninstall

```sh
./uninstall.sh
```

This removes autocopy, `autocopy-update`, `autocopy-notify` and its launcher entry, the timer, the releases it downloaded and the notification setting.

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

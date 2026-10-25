# autocopy

Highlight text anywhere and it lands in the clipboard, ready for `Ctrl+V`. Same behavior as Claude Code, for the whole desktop.

## Install

```sh
git clone https://github.com/Edward-Pratt/autocopy.git && cd autocopy && ./install.sh
```

Requirements:

- A Wayland compositor that supports the primary selection (Hyprland, Sway, GNOME, KDE)
- `wl-clipboard` (`sudo pacman -S wl-clipboard`)
- A systemd-managed session, so `graphical-session.target` is active (for example Hyprland started with uwsm)

## How it works

`wl-paste --primary --watch` reports every change to the primary selection (the text you highlight). autocopy copies each new selection into the clipboard with `wl-copy`. A systemd user service runs it with your graphical session.

Clicking elsewhere clears the primary selection. autocopy ignores that, so the clipboard keeps the last text you highlighted. Text highlighted in a password field is marked sensitive and is not copied.

## Uninstall

```sh
./uninstall.sh
```

## Logs

```sh
journalctl --user -u autocopy
```

#!/bin/sh
set -eu
cd "$(dirname "$0")"

command -v wl-paste >/dev/null || { echo "wl-clipboard is missing. Install it: sudo pacman -S wl-clipboard" >&2; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "No Wayland session found. autocopy needs Wayland." >&2; exit 1; }
systemctl --user is-active --quiet graphical-session.target || {
	echo "graphical-session.target is not active. Start your compositor through uwsm or another systemd session manager." >&2
	exit 1
}

install -Dm755 bin/autocopy "$HOME/.local/bin/autocopy"
install -Dm644 autocopy.service "$HOME/.config/systemd/user/autocopy.service"
systemctl --user daemon-reload
systemctl --user enable autocopy.service
systemctl --user restart autocopy.service
echo "autocopy installed. Highlighted text now goes to the clipboard."

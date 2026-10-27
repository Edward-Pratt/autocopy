#!/bin/sh
# Installs autocopy and its updater. Run from a clone, or by autocopy-update from an unpacked release.
set -eu
cd "$(dirname "$0")"

command -v wl-paste >/dev/null || { echo "wl-clipboard is missing. Install it: sudo pacman -S wl-clipboard" >&2; exit 1; }
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "No Wayland session found. autocopy needs Wayland." >&2; exit 1; }
systemctl --user is-active --quiet graphical-session.target || {
	echo "graphical-session.target is not active. Start your compositor through uwsm or another systemd session manager." >&2
	exit 1
}

# install replaces each file rather than writing over it: autocopy-update may be running this.
install -Dm755 bin/autocopy "$HOME/.local/bin/autocopy"
install -Dm755 updater/autocopy-update "$HOME/.local/bin/autocopy-update"
install -Dm644 autocopy.service "$HOME/.config/systemd/user/autocopy.service"
install -Dm644 updater/autocopy-update.service "$HOME/.config/systemd/user/autocopy-update.service"
install -Dm644 updater/autocopy-update.timer "$HOME/.config/systemd/user/autocopy-update.timer"
systemctl --user daemon-reload
systemctl --user enable autocopy.service
systemctl --user restart autocopy.service
systemctl --user enable --now autocopy-update.timer
echo "autocopy ${HERALD_VERSION:+$HERALD_VERSION }installed. Highlighted text now goes to the clipboard."

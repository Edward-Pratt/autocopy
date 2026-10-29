#!/bin/sh
# Installs autocopy and its updater. Run from a clone, or by autocopy-update from an unpacked release.
set -eu
cd "$(dirname "$0")"

for tool in cc make pkg-config wayland-scanner; do
	command -v "$tool" >/dev/null || { echo "$tool is missing. Install it: sudo pacman -S base-devel wayland" >&2; exit 1; }
done
pkg-config --exists wayland-client || { echo "wayland-client headers are missing. Install them: sudo pacman -S wayland" >&2; exit 1; }
proto=$(pkg-config --variable=pkgdatadir wayland-protocols 2>/dev/null || true)
[ -f "$proto/staging/ext-data-control/ext-data-control-v1.xml" ] || {
	echo "wayland-protocols 1.39 or newer is missing. Install it: sudo pacman -S wayland-protocols" >&2
	exit 1
}
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "No Wayland session found. autocopy needs Wayland." >&2; exit 1; }
systemctl --user is-active --quiet graphical-session.target || {
	echo "graphical-session.target is not active. Start your compositor through uwsm or another systemd session manager." >&2
	exit 1
}

make
# install replaces each file rather than writing over it: autocopy-update may be running this.
install -Dm755 autocopy "$HOME/.local/bin/autocopy"
install -Dm755 updater/autocopy-update "$HOME/.local/bin/autocopy-update"
install -Dm644 autocopy.service "$HOME/.config/systemd/user/autocopy.service"
install -Dm644 updater/autocopy-update.service "$HOME/.config/systemd/user/autocopy-update.service"
install -Dm644 updater/autocopy-update.timer "$HOME/.config/systemd/user/autocopy-update.timer"
systemctl --user daemon-reload
systemctl --user enable autocopy.service
systemctl --user restart autocopy.service
systemctl --user enable --now autocopy-update.timer
# From a clone, note the release it's on (if any), so the first daily check doesn't announce what's installed.
[ -n "${HERALD_VERSION:-}" ] || "$HOME/.local/bin/autocopy-update" --record "$PWD"
echo "autocopy ${HERALD_VERSION:+$HERALD_VERSION }installed. Highlighted text now goes to the clipboard."

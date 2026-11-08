#!/bin/sh
set -eu
systemctl --user disable --now autocopy.service autocopy-update.timer 2>/dev/null || true
rm -f "$HOME/.local/bin/autocopy" "$HOME/.local/bin/autocopy-update" "$HOME/.local/bin/autocopy-notify" \
	"$HOME/.config/systemd/user/autocopy.service" \
	"$HOME/.config/systemd/user/autocopy-update.service" "$HOME/.config/systemd/user/autocopy-update.timer" \
	"${XDG_DATA_HOME:-$HOME/.local/share}/applications/autocopy-notify.desktop"
# The notification setting.
rm -rf "${XDG_CONFIG_HOME:-$HOME/.config}/autocopy"
# The updater's downloaded releases and what it last installed.
rm -rf "${XDG_DATA_HOME:-$HOME/.local/share}/autocopy"
# The apps autocopy learned paste-over for.
rm -rf "${XDG_STATE_HOME:-$HOME/.local/state}/autocopy"
systemctl --user daemon-reload
echo "autocopy removed."

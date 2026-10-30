#!/bin/sh
set -eu
systemctl --user disable --now autocopy.service autocopy-update.timer 2>/dev/null || true
rm -f "$HOME/.local/bin/autocopy" "$HOME/.local/bin/autocopy-update" \
	"$HOME/.config/systemd/user/autocopy.service" \
	"$HOME/.config/systemd/user/autocopy-update.service" "$HOME/.config/systemd/user/autocopy-update.timer"
# The updater's downloaded releases and what it last installed.
rm -rf "${XDG_DATA_HOME:-$HOME/.local/share}/autocopy"
# The apps autocopy learned paste-over for.
rm -rf "${XDG_STATE_HOME:-$HOME/.local/state}/autocopy"
systemctl --user daemon-reload
echo "autocopy removed."

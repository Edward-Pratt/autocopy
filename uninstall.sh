#!/bin/sh
set -eu
systemctl --user disable --now autocopy.service 2>/dev/null || true
rm -f "$HOME/.local/bin/autocopy" "$HOME/.config/systemd/user/autocopy.service"
systemctl --user daemon-reload
echo "autocopy removed."

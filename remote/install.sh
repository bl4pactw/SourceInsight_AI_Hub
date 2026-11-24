#!/bin/sh
set -e
dir="$HOME/.sihub"
chmod 755 "$dir/stop_hook.sh"
mkdir -p "$dir/stop" "$HOME/.claude"
command -v python3 >/dev/null || { echo "python3 is required on this host"; exit 1; }
python3 - "$dir/stop_hook.sh" <<'PY'
import json
import os
import shutil
import sys
import time

hook_command = sys.argv[1]
settings_path = os.path.expanduser("~/.claude/settings.json")
settings = {}
if os.path.exists(settings_path):
    with open(settings_path, encoding="utf-8") as settings_file:
        settings = json.load(settings_file)
stop_groups = settings.setdefault("hooks", {}).setdefault("Stop", [])
installed = any(hook.get("command") == hook_command for group in stop_groups for hook in group.get("hooks", []))
if not installed:
    if os.path.exists(settings_path):
        shutil.copy2(settings_path, settings_path + ".sihub-backup-" + time.strftime("%Y%m%d%H%M%S"))
    stop_groups.append({"hooks": [{"type": "command", "command": hook_command}]})
    with open(settings_path, "w", encoding="utf-8") as settings_file:
        json.dump(settings, settings_file, indent=2, ensure_ascii=False)
        settings_file.write("\n")
print("Stop hook " + ("already installed" if installed else "installed") + " in " + settings_path)
PY
echo "Restart the Claude CLI in tmux (exit, then: tmux new -A -s sihub claude) so it loads the hook."

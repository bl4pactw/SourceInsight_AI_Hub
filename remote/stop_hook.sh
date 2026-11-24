#!/bin/sh
[ -n "$TMUX_PANE" ] || exit 0
session=$(tmux display-message -p -t "$TMUX_PANE" '#S' 2>/dev/null) || exit 0
case "$session" in
	''|*/*|.*) exit 0 ;;
esac
dir="$HOME/.sihub/stop"
mkdir -p "$dir" || exit 0
tmp="$dir/.$session.$$"
if cat > "$tmp"; then
	mv -f "$tmp" "$dir/$session.json"
else
	rm -f "$tmp"
fi
exit 0

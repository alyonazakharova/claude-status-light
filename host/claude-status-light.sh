#!/bin/sh
# Send a status command to the Claude status light.
# Usage: claude-status-light.sh waiting|working|done|off|test
# Port: auto-detected, override with CLAUDE_STATUS_LIGHT_PORT=/dev/cu.usbmodemXXXX
#
# Claude Code has no hook for a manual interrupt (Esc), so while working a
# background watcher tails the session transcript and switches to "done"
# when it sees "[Request interrupted by user". The Stop hook ends the watcher.

case "$1" in
  waiting) cmd=R ;;
  working) cmd=B ;;
  done)    cmd=G ;;
  off)     cmd=O ;;
  test)    cmd=T ;;
  __watch) ;;
  *) echo "usage: $0 waiting|working|done|off|test" >&2; exit 0 ;;
esac

send() {
  port="${CLAUDE_STATUS_LIGHT_PORT:-$(ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null | head -n 1)}"
  [ -n "$port" ] && [ -e "$port" ] || return 0
  # Write in the background with a timeout so a stuck port can't stall Claude.
  ( printf '%s' "$1" > "$port" ) 2>/dev/null &
  pid=$!
  ( sleep 2; kill "$pid" 2>/dev/null ) &
  wait "$pid" 2>/dev/null
}

state_dir="${TMPDIR:-/tmp}/claude-status-light"

# ---- Interrupt watcher (runs detached) --------------------------------------
if [ "$1" = "__watch" ]; then
  transcript="$2"
  pidfile="$3"
  offset=$(wc -c < "$transcript" | tr -d ' ')
  deadline=$(( $(date +%s) + 6 * 3600 ))
  while [ "$(cat "$pidfile" 2>/dev/null)" = "$$" ] && [ "$(date +%s)" -lt "$deadline" ]; do
    sleep 1
    size=$(wc -c < "$transcript" 2>/dev/null | tr -d ' ')
    [ -n "$size" ] || break
    [ "$size" -gt "$offset" ] || continue
    # Re-read a little before the offset so a marker split across reads is still found.
    from=$(( offset > 200 ? offset - 200 : 0 ))
    if tail -c +"$(( from + 1 ))" "$transcript" | head -c "$(( size - from ))" \
        | grep -q 'Request interrupted by user'; then
      send G
      break
    fi
    offset=$size
  done
  [ "$(cat "$pidfile" 2>/dev/null)" = "$$" ] && rm -f "$pidfile"
  exit 0
fi

# ---- Hook entry point -------------------------------------------------------
# Claude Code hooks pipe JSON on stdin with session_id and transcript_path.
input=""
[ -t 0 ] || input=$(cat)
json_field() {
  printf '%s' "$input" | sed -n "s/.*\"$1\" *: *\"\([^\"]*\)\".*/\1/p" | head -n 1
}
session=$(json_field session_id)
transcript=$(json_field transcript_path)
pidfile="$state_dir/${session:-default}.pid"

send "$cmd"

case "$1" in
  working)
    # Start one watcher per session; later "working" calls reuse it.
    if [ -n "$transcript" ] && [ -f "$transcript" ]; then
      mkdir -p "$state_dir"
      running=$(cat "$pidfile" 2>/dev/null)
      if [ -z "$running" ] || ! kill -0 "$running" 2>/dev/null; then
        nohup "$0" __watch "$transcript" "$pidfile" </dev/null >/dev/null 2>&1 &
        echo $! > "$pidfile"
      fi
    fi
    ;;
  done|off)
    # Turn finished normally: stop this session's watcher.
    running=$(cat "$pidfile" 2>/dev/null)
    rm -f "$pidfile"
    [ -n "$running" ] && kill "$running" 2>/dev/null
    ;;
esac
exit 0

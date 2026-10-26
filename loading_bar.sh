#!/bin/bash
# ---------------------------------------------------------------------------------------------
# loading_bar.sh
# Generic, zero-dependency in-terminal progress bar for Bash build and task scripts.
# ---------------------------------------------------------------------------------------------

# Colors & Formatting
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
CYAN="\033[1;36m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
RESET="\033[0m"

# Internal configuration & state
_BAR_WIDTH="${PROGRESS_BAR_WIDTH:-30}"
_CURRENT_STEP=0
_TOTAL_STEPS=0

# Locate the calling script in the Bash source stack
_find_caller_script() {
    local this_file="${BASH_SOURCE[0]}"
    for src in "${BASH_SOURCE[@]}"; do
        if [ -n "$src" ] && [ "$src" != "$this_file" ] && [ -f "$src" ]; then
            echo "$src"
            return 0
        fi
    done
    echo "$0"
}

# Automatically count the steps defined in the calling script
_auto_detect_total_steps() {
    local caller
    caller="$(_find_caller_script)"
    if [ -f "$caller" ]; then
        grep -E -c '^[[:space:]]*(run_step|step)([[:space:]]|$)' "$caller" 2>/dev/null || echo 0
    else
        echo 0
    fi
}

# Renders the progress bar
draw_progress() {
    local step=$1
    local total=$2
    local label=$3
    local pct=0

    if [ "$total" -gt 0 ]; then
        pct=$(( step * 100 / total ))
    fi

    # Clamp percentage to 100 max
    if [ "$pct" -gt 100 ]; then
        pct=100
    fi

    local filled=$(( pct * _BAR_WIDTH / 100 ))
    local empty=$(( _BAR_WIDTH - filled ))

    local bar=""
    for ((i=0; i<filled; i++)); do bar+="█"; done
    for ((i=0; i<empty; i++)); do bar+="░"; done

    # Clear line to end (\033[K) and print bar
    printf "\r${BLUE}[%3d%%]${RESET} ${GREEN}[%s]${RESET} ${BOLD}%-42s${RESET}\033[K" "$pct" "$bar" "$label"
}

# Execute a single step with progress feedback
run_step() {
    local label="$1"
    shift

    # Auto-initialize total steps on the first step if not manually defined
    if [ "$_TOTAL_STEPS" -le 0 ]; then
        if [ -n "$TOTAL_STEPS" ] && [ "$TOTAL_STEPS" -gt 0 ] 2>/dev/null; then
            _TOTAL_STEPS="$TOTAL_STEPS"
        else
            _TOTAL_STEPS=$(_auto_detect_total_steps)
            if [ "$_TOTAL_STEPS" -le 0 ]; then
                _TOTAL_STEPS=1
            fi
        fi
    fi

    draw_progress "$_CURRENT_STEP" "$_TOTAL_STEPS" "$label"

    # Execute step command (handles both direct commands and compound expressions)
    local status=0
    if [ $# -eq 1 ] && [[ "$1" =~ [\|\&\;\>\<] ]]; then
        eval "$1" || status=$?
    else
        "$@" || status=$?
    fi

    if [ "$status" -ne 0 ]; then
        printf "\n${RED}✘ Failed step: %s (exit code %d)${RESET}\n" "$label" "$status" >&2
        return "$status"
    fi

    _CURRENT_STEP=$((_CURRENT_STEP + 1))
    draw_progress "$_CURRENT_STEP" "$_TOTAL_STEPS" "$label"
}

# Convenient alias for run_step
step() {
    run_step "$@"
}

# Manually override the total number of steps if needed
set_total_steps() {
    _TOTAL_STEPS="$1"
}

# Optional helper to reset progress bar state
reset_progress() {
    _CURRENT_STEP=0
    _TOTAL_STEPS="${1:-0}"
}

# Optional helper to cleanly complete progress output
finish_progress() {
    local msg="${1:-}"
    if [ -n "$msg" ]; then
        printf "\n${GREEN}✔ %s${RESET}\n\n" "$msg"
    else
        printf "\n"
    fi
}

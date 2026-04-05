#!/usr/bin/env zsh
# cc — Claude Code project launcher
# Discovers all Claude-enabled projects and opens one interactively

CLAUDE_PROJECTS="$HOME/.claude/projects"
CLAUDE_FLAGS="${CLAUDE_FLAGS:---dangerously-skip-permissions}"

# ── Discovery ────────────────────────────────────────────────────────────────

typeset -A seen
typeset -a projects

add_project() {
    local p="$1"
    p=$(cd "$p" 2>/dev/null && pwd -P) || return
    [[ -n "${seen[$p]}" ]] && return
    seen[$p]=1
    projects+=("$p")
}

# Method 1: CLAUDE.md files — the most reliable signal
while IFS= read -r md_file; do
    add_project "$(dirname "$md_file")"
done < <(find "$HOME" -maxdepth 5 \
    \( -path "*/node_modules" -o -path "*/.git" -o -path "*/vendor" \
       -o -path "*/.claude/projects" -o -path "*/.vscode" -o -path "*/.cursor" \) -prune \
    -o -name "CLAUDE.md" -print 2>/dev/null)

# Method 2: Decode ~/.claude/projects/* names to filesystem paths
if [[ -d "$CLAUDE_PROJECTS" ]]; then
    for encoded_path in "$CLAUDE_PROJECTS"/*/; do
        local encoded=$(basename "$encoded_path")

# Naive decode: leading '-' represents '/', each '-' is a '/'
        local candidate="/${encoded#-}"
        candidate="${candidate//-//}"

        if [[ ! -d "$candidate" ]]; then
            # Hyphens-in-dir-name fallback: treat suffix after home dir as literal
            local home_encoded="${HOME//\//\-}"
            local suffix="${encoded#${home_encoded#-}-}"
            [[ "$suffix" == "$encoded" ]] && continue
            candidate="$HOME/$suffix"
            [[ ! -d "$candidate" ]] && continue
        fi

        add_project "$candidate"
    done
fi

if [[ ${#projects[@]} -eq 0 ]]; then
    print -P "%F{red}No Claude Code projects found.%f" >&2
    exit 1
fi

# Sort alphabetically
projects=(${(o)projects})

# ── Selection ────────────────────────────────────────────────────────────────

if command -v fzf &>/dev/null; then
    chosen=$(printf '%s\n' "${projects[@]}" | fzf \
        --prompt="Claude project > " \
        --height=50% \
        --border \
        --preview='ls {}' \
        --preview-window=right:40%) || { echo "Cancelled."; exit 0; }
else
    echo ""
    print -P "%B%F{cyan}  Claude Code — Project Launcher%f%b"
    echo "──────────────────────────────────────────────────────"
    idx=1
    for p in "${projects[@]}"; do
        display="${p/#$HOME/~}"
        printf '  \033[1;33m%2d\033[0m  %s\n' "$idx" "$display"
        (( idx++ ))
    done
    echo "──────────────────────────────────────────────────────"
    echo ""
    read "choice?  Enter number (or q to quit): "

    [[ "$choice" == "q" || -z "$choice" ]] && { echo "Cancelled."; exit 0; }

    if ! [[ "$choice" =~ ^[0-9]+$ ]] || (( choice < 1 || choice > ${#projects[@]} )); then
        print -P "%F{red}Invalid selection.%f" >&2
        exit 1
    fi

    chosen="${projects[$choice]}"
fi

# ── Launch ───────────────────────────────────────────────────────────────────

display="${chosen/#$HOME/~}"
echo ""
print -P "%F{green}  Opening:%f $display"
print -P "%F{245}  claude $CLAUDE_FLAGS%f"
echo ""

cd "$chosen"
exec claude $CLAUDE_FLAGS

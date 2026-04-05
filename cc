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
        local candidate="/${encoded#-}"
        candidate="${candidate//-//}"
        if [[ ! -d "$candidate" ]]; then
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

# Sort by most recently modified
projects=($(for p in "${projects[@]}"; do
    mtime=$(stat -f "%m" "$p" 2>/dev/null || echo 0)
    echo "${mtime} ${p}"
done | sort -rn | awk '{print $2}'))

# ── Agents ───────────────────────────────────────────────────────────────────

get_agents() {
    local agents_dir="$1/.claude/agents"
    [[ ! -d "$agents_dir" ]] && return
    local names=() name
    for f in "$agents_dir"/*.md(N); do
        name=$(grep -m1 '^name:' "$f" 2>/dev/null | sed 's/^name:[[:space:]]*//' | tr -d '"')
        [[ -z "$name" ]] && name=$(basename "$f" .md)
        names+=("$name")
    done
    [[ ${#names[@]} -gt 0 ]] && echo "${(j:, :)names}"
}

# ── Selection ────────────────────────────────────────────────────────────────

if command -v fzf &>/dev/null; then
    local selected
    selected=$(for p in "${projects[@]}"; do
        local display="${p/#$HOME/~}"
        local mtime=$(stat -f "%Sm" -t "%b %d" "$p" 2>/dev/null)
        local agents=$(get_agents "$p")
        # Field 1 (hidden): path
        # Field 2 (shown): padded display + dimmed date
        printf '%s\t%-45s  \033[2m%s\033[0m\n' "$p" "$display" "$mtime"
        [[ -n "$agents" ]] && printf '%s\t  \033[2m· %s\033[0m\n' "$p" "$agents"
        printf '\0'
    done | fzf \
        --read0 \
        --ansi \
        --prompt=" Claude  " \
        --pointer="▶" \
        --marker="✓" \
        --height=70% \
        --min-height=15 \
        --border=rounded \
        --border-label=" 󱁤 Claude Code Projects " \
        --padding=1,2 \
        --delimiter=$'\t' \
        --with-nth=2 \
        --color='border:#585b70,label:#cba6f7,prompt:#cba6f7,pointer:#f38ba8,hl:yellow,hl+:yellow,info:#a6adc8' \
        --preview='
            p={1}
            if [ -f "$p/CLAUDE.md" ]; then
                cat "$p/CLAUDE.md"
            else
                ls "$p"
            fi
        ' \
        --preview-window=right:45%:wrap \
        --preview-label=" CLAUDE.md ") || { echo "Cancelled."; exit 0; }
    chosen=$(printf '%s' "$selected" | head -1 | cut -f1)
else
    echo ""
    print -P "%B%F{cyan}  Claude Code — Project Launcher%f%b"
    echo "──────────────────────────────────────────────────────"
    idx=1
    for p in "${projects[@]}"; do
        display="${p/#$HOME/~}"
        mtime=$(stat -f "%Sm" -t "%b %d" "$p" 2>/dev/null)
        printf '  \033[1;33m%2d\033[0m  %-50s \033[2m%s\033[0m\n' "$idx" "$display" "$mtime"
        agents=$(get_agents "$p")
        [[ -n "$agents" ]] && printf '       \033[2m· %s\033[0m\n' "$agents"
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

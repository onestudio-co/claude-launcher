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

# Sort by recency: history entries first (in history order), then rest alphabetically
LAUNCHER_HISTORY="$HOME/.claude/launcher_history"
typeset -a recent_projects other_projects
if [[ -f "$LAUNCHER_HISTORY" ]]; then
    while IFS= read -r hist_path; do
        [[ -z "$hist_path" ]] && continue
        if [[ -n "${seen[$hist_path]}" ]]; then
            recent_projects+=("$hist_path")
        fi
    done < "$LAUNCHER_HISTORY"
fi
for p in ${(o)projects}; do
    local in_history=0
    for rp in "${recent_projects[@]}"; do
        [[ "$rp" == "$p" ]] && in_history=1 && break
    done
    (( in_history )) || other_projects+=("$p")
done
projects=("${recent_projects[@]}" "${other_projects[@]}")

# ── Direct launch via query argument ─────────────────────────────────────────

if [[ -n "$1" ]]; then
    typeset -a matches
    for p in "${projects[@]}"; do
        local base="${p:t}"
        if [[ "${base:l}" == *"${1:l}"* ]]; then
            matches+=("$p")
        fi
    done

    if (( ${#matches[@]} == 0 )); then
        print -P "%F{red}No projects matching '%F{yellow}$1%F{red}'.%f" >&2
        exit 1
    elif (( ${#matches[@]} == 1 )); then
        chosen="${matches[1]}"
        display="${chosen/#$HOME/~}"
        echo ""
        print -P "%F{green}  Opening:%f $display"
        print -P "%F{245}  claude $CLAUDE_FLAGS%f"
        echo ""
        cd "$chosen"
        exec claude $CLAUDE_FLAGS
    fi
    # 2+ matches: fall through to picker with query pre-seeded
fi

# ── Selection ────────────────────────────────────────────────────────────────

if command -v fzf &>/dev/null; then
    # Build display lines: "path|||display  [branch] commit msg"
    typeset -a fzf_lines
    for p in "${projects[@]}"; do
        display="${p/#$HOME/~}"
        branch=$(git -C "$p" rev-parse --abbrev-ref HEAD 2>/dev/null)
        commit=$(git -C "$p" log -1 --format="%s" 2>/dev/null)
        if [[ -n "$branch" ]]; then
            meta="  \033[2;37m[$branch] $commit\033[0m"
        else
            meta=""
        fi
        fzf_lines+=("${p}|||${display}${meta}")
    done

    chosen=$(printf '%s\n' "${fzf_lines[@]}" | fzf \
        --prompt=" Claude  " \
        --pointer="▶" \
        --height=70% \
        --min-height=15 \
        --border=rounded \
        --border-label=" Claude Code Projects " \
        --padding=1,2 \
        --color='border:#585b70,label:#cba6f7,prompt:#cba6f7,pointer:#f38ba8,hl:yellow,hl+:yellow,info:#a6adc8' \
        --delimiter='|||' \
        --with-nth=2 \
        --nth=1 \
        --ansi \
        --preview='p={1}; if [ -f "$p/CLAUDE.md" ]; then cat "$p/CLAUDE.md"; else ls "$p"; fi' \
        --preview-window=right:45%:wrap \
        --preview-label=" CLAUDE.md " \
        ${1:+--query "$1"}) || { echo "Cancelled."; exit 0; }
    chosen="${chosen%%|||*}"
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

# Record chosen project in history (newest first, deduplicated, max 50)
{
    print -r -- "$chosen"
    [[ -f "$LAUNCHER_HISTORY" ]] && grep -Fxv "$chosen" "$LAUNCHER_HISTORY"
} | head -50 >| "${LAUNCHER_HISTORY}.tmp" && mv "${LAUNCHER_HISTORY}.tmp" "$LAUNCHER_HISTORY"

cd "$chosen"
exec claude $CLAUDE_FLAGS

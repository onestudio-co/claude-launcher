# claude-launcher

Interactive terminal launcher for Claude Code projects.

## What it does
- Scans `~/` for `CLAUDE.md` files (up to 5 levels deep)
- Decodes `~/.claude/projects/` directory names as a fallback
- Sorts projects by recency via `~/.claude/launcher_history`
- Shows git branch + last commit in the fzf picker
- Supports direct launch via `cc <query>` (no picker if single match)
- Supports `--tmux` / `--new-window` for multi-window workflows
- Opens the selected project with `claude --dangerously-skip-permissions`

## Files
- `cc` — the launcher script (symlinked to `~/.local/bin/cc`)
- `install.sh` — sets up the symlink

## Usage
```
cc                          # launch picker (sorted by recency)
cc myproject                # direct launch if single match, picker if multiple
cc --tmux                   # open in new tmux window
cc --new-window             # open in new Terminal.app window (or tmux window)
CLAUDE_FLAGS="--verbose" cc # override default flags
```

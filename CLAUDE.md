# claude-launcher

Interactive terminal launcher for Claude Code projects.

## What it does
- Scans `~/` for `CLAUDE.md` files (up to 5 levels deep)
- Decodes `~/.claude/projects/` directory names as a fallback
- Presents a fuzzy picker (fzf) or numbered list
- Opens the selected project with `claude --dangerously-skip-permissions`

## Files
- `cc` — the launcher script (symlinked to `~/.local/bin/cc`)
- `install.sh` — sets up the symlink

## Usage
```
cc                          # launch picker
CLAUDE_FLAGS="--verbose" cc # override default flags
```

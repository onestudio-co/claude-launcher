# claude-launcher

A fast terminal launcher for [Claude Code](https://claude.ai/code) projects. Run `cc` to pick a project and open it instantly.

## How it works

1. Scans `~/` for `CLAUDE.md` files (up to 5 levels deep)
2. Falls back to decoding `~/.claude/projects/` directory names
3. Sorts by recency — most recently opened projects appear first
4. Presents a fuzzy picker (fzf) with git branch + last commit, or a numbered list
5. Opens the selected project with `claude`

## Install

```sh
git clone https://github.com/onestudio-co/claude-launcher.git
cd claude-launcher
./install.sh
```

Make sure `~/.local/bin` is in your PATH:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

## Usage

```sh
cc                          # launch picker (sorted by recency)
cc myproject                # jump directly if one match, picker if multiple
cc --tmux                   # open in a new tmux window
cc --new-window             # open in a new Terminal.app window (or tmux window if in tmux)
CLAUDE_FLAGS="--verbose" cc # override default Claude flags
```

### Direct launch

Pass a query to skip the picker when there's a single match:

```sh
cc api        # opens ~/projects/my-api if it's the only match
cc dash       # opens picker pre-filtered to "dash" if multiple match
```

### Multi-window flags

| Flag | Behavior |
|------|----------|
| `--tmux` | Opens a new tmux window (warns and falls back to normal if not in tmux) |
| `--new-window` | Opens a new Terminal.app window on macOS, or new tmux window if in tmux |

### Recency tracking

Every time you open a project, it's recorded in `~/.claude/launcher_history` (max 50 entries). The next time you run `cc`, your most recent projects appear at the top.

By default, Claude is opened with `--dangerously-skip-permissions`. Override with:

```sh
CLAUDE_FLAGS="" cc
```

## Requirements

- [Claude Code](https://claude.ai/code) (`claude` CLI in PATH)
- zsh
- [fzf](https://github.com/junegunn/fzf) *(optional, for fuzzy picking — falls back to numbered list)*

## License

MIT

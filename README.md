# claude-launcher

A fast terminal launcher for [Claude Code](https://claude.ai/code) projects. Run `cc` to pick a project and open it instantly.

## How it works

1. Scans `~/` for `CLAUDE.md` files (up to 5 levels deep)
2. Falls back to decoding `~/.claude/projects/` directory names
3. Presents a fuzzy picker (fzf) or numbered list
4. Opens the selected project with `claude`

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
cc                          # launch picker
CLAUDE_FLAGS="--verbose" cc # override default Claude flags
```

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

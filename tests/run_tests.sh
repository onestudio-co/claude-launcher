#!/usr/bin/env zsh
# tests/run_tests.sh — Test suite for the cc launcher
#
# Strategy: black-box testing with a fake HOME and mock binaries.
# We stub fzf (prints a pre-chosen line), claude (prints args), and git.
# Each test gets an isolated tmp dir so tests are independent.

setopt nullglob

# ── Helpers ──────────────────────────────────────────────────────────────────

PASS=0; FAIL=0
CC="$(cd "$(dirname "$0")/.." && pwd)/cc"

pass() { print -P "%F{green}✓%f $1"; (( PASS++ )); }
fail() { print -P "%F{red}✗%f $1"; (( FAIL++ )); }

assert_eq() {
    local label="$1" expected="$2" actual="$3"
    if [[ "$actual" == "$expected" ]]; then
        pass "$label"
    else
        fail "$label"
        echo "    expected: $(print -r -- "$expected" | head -3)"
        echo "    actual:   $(print -r -- "$actual"   | head -3)"
    fi
}

assert_contains() {
    local label="$1" needle="$2" haystack="$3"
    if [[ "$haystack" == *"$needle"* ]]; then
        pass "$label"
    else
        fail "$label"
        echo "    expected to contain: $needle"
        echo "    actual: $(print -r -- "$haystack" | head -3)"
    fi
}

assert_not_contains() {
    local label="$1" needle="$2" haystack="$3"
    if [[ "$haystack" != *"$needle"* ]]; then
        pass "$label"
    else
        fail "$label"
        echo "    expected NOT to contain: $needle"
    fi
}

assert_exit() {
    local label="$1" expected="$2" actual="$3"
    if [[ "$actual" == "$expected" ]]; then
        pass "$label"
    else
        fail "$label"
        echo "    expected exit code: $expected, got: $actual"
    fi
}

# ── Test fixture setup ────────────────────────────────────────────────────────

# setup_env <project-names...>
# Creates a fake HOME with CLAUDE.md-bearing projects and a mock bin dir.
# Sets FAKE_HOME, FAKE_BIN, FZF_CHOICE (path cc will "select" via mock fzf).
setup_env() {
    FAKE_HOME=$(mktemp -d) && FAKE_HOME=$(cd "$FAKE_HOME" && pwd -P)
    FAKE_BIN=$(mktemp -d)  && FAKE_BIN=$(cd  "$FAKE_BIN"  && pwd -P)

    # Create ~/.claude dir (cc writes history there)
    mkdir -p "$FAKE_HOME/.claude"

    # Create requested projects
    for name in "$@"; do
        mkdir -p "$FAKE_HOME/projects/$name"
        echo "# $name" > "$FAKE_HOME/projects/$name/CLAUDE.md"
    done

    # Mock fzf: read all lines, return the one matching FZF_CHOICE (or first)
    cat > "$FAKE_BIN/fzf" <<'EOF'
#!/usr/bin/env zsh
lines=()
while IFS= read -r line; do lines+=("$line"); done
# Return the first line whose raw path field matches FZF_CHOICE, else first line
choice="${FZF_CHOICE:-}"
for line in "${lines[@]}"; do
    raw="${line%%$'\x01'*}"
    [[ -z "$raw" ]] && raw="$line"
    if [[ "$raw" == *"$choice"* ]]; then
        print -r -- "$line"
        exit 0
    fi
done
# fallback: first line
print -r -- "${lines[1]}"
EOF
    chmod +x "$FAKE_BIN/fzf"

    # Mock claude: just print "claude <args>" so we can assert on launch
    cat > "$FAKE_BIN/claude" <<'EOF'
#!/bin/sh
echo "claude $*"
EOF
    chmod +x "$FAKE_BIN/claude"

    # Mock git: return predictable branch/commit info
    cat > "$FAKE_BIN/git" <<'EOF'
#!/usr/bin/env zsh
if [[ "$*" == *"rev-parse --abbrev-ref HEAD"* ]]; then
    echo "main"
elif [[ "$*" == *"log -1 --format=%s"* ]]; then
    echo "initial commit"
fi
EOF
    chmod +x "$FAKE_BIN/git"
}

# run_cc [args...]
# Runs cc with the fake environment. Captures stdout+stderr in CC_OUT, exit in CC_EXIT.
run_cc() {
    CC_OUT=$(HOME="$FAKE_HOME" PATH="$FAKE_BIN:/usr/bin:/bin" zsh "$CC" "$@" 2>&1)
    CC_EXIT=$?
}

teardown_env() {
    rm -rf "$FAKE_HOME" "$FAKE_BIN"
}

# ── Tests ─────────────────────────────────────────────────────────────────────

echo ""
print -P "%B%F{cyan}  cc — test suite%f%b"
echo "──────────────────────────────────────"

# ── 1. Discovery ──────────────────────────────────────────────────────────────

setup_env alpha beta gamma
FZF_CHOICE="alpha" run_cc
assert_contains "discovery: opens selected project" "claude" "$CC_OUT"
assert_contains "discovery: shows project path in output" "alpha" "$CC_OUT"
teardown_env

# ── 2. No projects found ──────────────────────────────────────────────────────

FAKE_HOME=$(mktemp -d) && FAKE_HOME=$(cd "$FAKE_HOME" && pwd -P)
FAKE_BIN=$(mktemp -d)  && FAKE_BIN=$(cd "$FAKE_BIN"  && pwd -P)
cat > "$FAKE_BIN/fzf" <<'EOF'
#!/bin/sh
cat
EOF
chmod +x "$FAKE_BIN/fzf"
CC_OUT=$(HOME="$FAKE_HOME" PATH="$FAKE_BIN:/usr/bin:/bin" zsh "$CC" 2>&1)
CC_EXIT=$?
assert_exit "no projects: exits non-zero" "1" "$CC_EXIT"
assert_contains "no projects: prints error" "No Claude Code projects found" "$CC_OUT"
rm -rf "$FAKE_HOME" "$FAKE_BIN"

# ── 3. Direct launch — single match ───────────────────────────────────────────

setup_env my-api dashboard analytics
FZF_CHOICE="" run_cc "api"
assert_contains "direct launch: opens on single match" "claude" "$CC_OUT"
assert_contains "direct launch: correct project selected" "my-api" "$CC_OUT"
teardown_env

# ── 4. Direct launch — no match ───────────────────────────────────────────────

setup_env alpha beta
FZF_CHOICE="" run_cc "zzznomatch"
assert_exit "direct launch no match: exits 1" "1" "$CC_EXIT"
assert_contains "direct launch no match: error message" "No projects matching" "$CC_OUT"
teardown_env

# ── 5. Direct launch — multiple matches fall through to fzf ───────────────────

setup_env my-api my-admin
FZF_CHOICE="my-api" run_cc "my"
assert_contains "direct launch multi: opens fzf and picks" "claude" "$CC_OUT"
assert_contains "direct launch multi: correct project" "my-api" "$CC_OUT"
teardown_env

# ── 6. Recency sorting ────────────────────────────────────────────────────────

setup_env alpha beta gamma
# Prime history with gamma, then beta (beta is most recent)
mkdir -p "$FAKE_HOME/.claude"
printf '%s\n' \
    "$FAKE_HOME/projects/beta" \
    "$FAKE_HOME/projects/gamma" > "$FAKE_HOME/.claude/launcher_history"
# The mock fzf returns first line — which should be beta (most recent)
FZF_CHOICE="" run_cc
assert_contains "recency: most recent project opened first" "beta" "$CC_OUT"
teardown_env

# ── 7. History written on launch ──────────────────────────────────────────────

setup_env alpha beta
FZF_CHOICE="alpha" run_cc
hist=$(cat "$FAKE_HOME/.claude/launcher_history" 2>/dev/null)
assert_contains "history: records launched project" "alpha" "$hist"
teardown_env

# ── 8. History deduplication ──────────────────────────────────────────────────

setup_env alpha beta
mkdir -p "$FAKE_HOME/.claude"
printf '%s\n' \
    "$FAKE_HOME/projects/beta" \
    "$FAKE_HOME/projects/alpha" > "$FAKE_HOME/.claude/launcher_history"
FZF_CHOICE="alpha" run_cc
hist=$(cat "$FAKE_HOME/.claude/launcher_history")
count=$(grep -c "alpha" <<< "$hist")
assert_eq "history: no duplicates after relaunch" "1" "$count"
teardown_env

# ── 9. History capped at 50 ───────────────────────────────────────────────────

setup_env alpha
mkdir -p "$FAKE_HOME/.claude"
# Write 60 fake entries
for i in {1..60}; do echo "/fake/project$i"; done > "$FAKE_HOME/.claude/launcher_history"
FZF_CHOICE="alpha" run_cc
lines=$(wc -l < "$FAKE_HOME/.claude/launcher_history")
[[ "$lines" -le 50 ]] && pass "history: capped at 50 entries" || fail "history: capped at 50 entries (got $lines)"
teardown_env

# ── 10. fzf delimiter — path is clean (the ||| regex bug) ────────────────────

setup_env my-project
FZF_CHOICE="my-project" run_cc
assert_not_contains "fzf delimiter: path not corrupted to /" "Opening: /" "$CC_OUT"
assert_contains "fzf delimiter: shows correct tilde path" "my-project" "$CC_OUT"
teardown_env

# ── 11. --tmux flag outside tmux ─────────────────────────────────────────────

setup_env alpha
FZF_CHOICE="alpha" TMUX="" run_cc --tmux
assert_contains "--tmux outside tmux: warns user" "Warning" "$CC_OUT"
assert_contains "--tmux outside tmux: still launches" "claude" "$CC_OUT"
teardown_env

# ── 12. --tmux flag inside tmux ──────────────────────────────────────────────

setup_env alpha
FAKE_HOME=$(mktemp -d) && FAKE_HOME=$(cd "$FAKE_HOME" && pwd -P)
FAKE_BIN=$(mktemp -d)  && FAKE_BIN=$(cd "$FAKE_BIN"  && pwd -P)
mkdir -p "$FAKE_HOME/projects/alpha"
echo "# alpha" > "$FAKE_HOME/projects/alpha/CLAUDE.md"
cat > "$FAKE_BIN/fzf" <<'EOF'
#!/usr/bin/env zsh
while IFS= read -r line; do first="$line"; break; done
print -r -- "$first"
EOF
chmod +x "$FAKE_BIN/fzf"
cat > "$FAKE_BIN/git" <<'EOF'
#!/bin/sh
case "$*" in *"rev-parse"*) echo "main";; *"log"*) echo "commit";; esac
EOF
chmod +x "$FAKE_BIN/git"
# Mock tmux
cat > "$FAKE_BIN/tmux" <<'EOF'
#!/bin/sh
echo "tmux $*"
EOF
chmod +x "$FAKE_BIN/tmux"
CC_OUT=$(HOME="$FAKE_HOME" TMUX="/tmp/tmux-session" PATH="$FAKE_BIN:/usr/bin:/bin" zsh "$CC" --tmux 2>&1)
assert_contains "--tmux inside tmux: calls tmux new-window" "tmux new-window" "$CC_OUT"
rm -rf "$FAKE_HOME" "$FAKE_BIN"

# ── Summary ───────────────────────────────────────────────────────────────────

echo "──────────────────────────────────────"
total=$(( PASS + FAIL ))
if (( FAIL == 0 )); then
    print -P "%F{green}%B  All $total tests passed.%f%b"
else
    print -P "%F{red}%B  $FAIL/$total tests FAILED.%f%b"
    exit 1
fi

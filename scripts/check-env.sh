#!/usr/bin/env bash
# DSLab: read-only toolchain check. Prints PASS or FAIL for every tool and
# exits non-zero if anything FAILs. Changes nothing.
#
#     bash scripts/check-env.sh
#
# No 'set -e': one failing check must not stop the others.
# No 'set -u': nvm.sh is not written for it.
set -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${EMSDK:-$HOME/tools/emsdk}"
export NVM_DIR="$HOME/.nvm"
failures=0

pass() { printf 'PASS  %-20s %s\n' "$1" "$2"; }
fail() { printf 'FAIL  %-20s %s\n' "$1" "$2"; failures=$((failures + 1)); }
skip() { printf 'SKIP  %-20s %s\n' "$1" "$2"; }

# True when version $1 >= version $2 (e.g. 3.28.3 >= 3.20).
version_at_least() {
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" = "$2" ]
}

# First version-looking number (like 3.28.3) in some text.
first_version() {
  grep -oE '[0-9]+(\.[0-9]+)+' <<< "$1" | head -n 1
}

# check_min NAME MINIMUM COMMAND...  PASS when the command's version is >= MINIMUM.
check_min() {
  local name="$1" minimum="$2" output version
  shift 2
  output="$("$@" 2>&1)"
  version="$(first_version "$output")"
  if [ -n "$version" ] && version_at_least "$version" "$minimum"; then
    pass "$name" "$version (>= $minimum)"
  else
    fail "$name" "found '${version:-nothing}', need >= $minimum"
  fi
}

# check_any NAME COMMAND...  PASS when the command runs and reports a version.
check_any() {
  local name="$1" output version
  shift
  output="$("$@" 2>&1)"
  version="$(first_version "$output")"
  if [ -n "$version" ]; then
    pass "$name" "$version"
  else
    fail "$name" "not found"
  fi
}

# Load tools the way a new interactive shell does from ~/.bashrc:
# emsdk first, then nvm, so nvm's Node ends up first on PATH.
export PATH="$HOME/.local/bin:$PATH"
[ -s "$EMSDK_DIR/emsdk_env.sh" ] && source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1
[ -s "$NVM_DIR/nvm.sh" ] && source "$NVM_DIR/nvm.sh"

echo "DSLab environment check, $(date '+%Y-%m-%d %H:%M')"
echo

# --- System -------------------------------------------------------------------
arch="$(uname -m)"
if [ "$arch" = "aarch64" ]; then pass "CPU architecture" "$arch"; else fail "CPU architecture" "$arch (expected aarch64)"; fi

os_id="$(. /etc/os-release && echo "$ID")"
os_version="$(. /etc/os-release && echo "$VERSION_ID")"
if [ "$os_id" = "ubuntu" ] && version_at_least "$os_version" "22.04"; then
  pass "Ubuntu" "$os_version"
else
  fail "Ubuntu" "$os_id $os_version (expected Ubuntu 22.04 or newer)"
fi

kernel="$(uname -r)"
if [[ "$kernel" == *microsoft-standard-WSL2* ]]; then
  wsl_version="$(timeout 10 wsl.exe --version 2>/dev/null | tr -d '\0\r' | head -n 1)"
  pass "WSL" "WSL 2, kernel $kernel${wsl_version:+, $wsl_version}"
else
  fail "WSL" "kernel $kernel is not a WSL 2 kernel"
fi

free_gb="$(df -BG --output=avail "$HOME" | tail -n 1 | tr -dc '0-9')"
if [ "$free_gb" -ge 10 ]; then pass "Free disk" "${free_gb} GB in \$HOME"; else fail "Free disk" "${free_gb} GB in \$HOME (need 10 GB)"; fi

# --- Native C++ toolchain -----------------------------------------------------------
cpp17_test='#include <utility>
int main() { if constexpr (true) { auto [a, b] = std::pair<int, int>{1, 2}; return a + b - 3; } }'
if gpp_output="$(g++ --version 2>&1)" && g++ -std=c++17 -x c++ - -o /dev/null <<< "$cpp17_test" 2>/dev/null; then
  pass "g++ (C++17)" "$(first_version "$gpp_output"), C++17 test program compiles"
else
  fail "g++ (C++17)" "g++ missing or cannot compile C++17"
fi
check_min "cmake" "3.20" cmake --version
check_any "ninja" ninja --version
check_any "python3" python3 --version
check_any "git" git --version
check_any "curl" curl --version
check_any "unzip" unzip -v

# --- Node ---------------------------------------------------------------------------
if [ -s "$NVM_DIR/nvm.sh" ]; then pass "nvm" "$(nvm --version)"; else fail "nvm" "not found in $NVM_DIR"; fi
check_min "Node" "22.12.0" node --version
check_any "npm" npm --version

# A new interactive shell must report nvm's Node, not emsdk's bundled one.
shell_node="$(bash -ic 'echo "$(command -v node) $(node --version)"' 2>/dev/null | tail -n 1)"
shell_node_path="${shell_node%% *}"
shell_node_version="$(first_version "${shell_node#* }")"
if [[ "$shell_node_path" == "$NVM_DIR"/* ]] && version_at_least "${shell_node_version:-0}" "22.12.0"; then
  pass "Node in new shell" "$shell_node_version from nvm"
else
  fail "Node in new shell" "'$shell_node' (expected nvm's Node; check the order in ~/.bashrc)"
fi

# --- Emscripten -----------------------------------------------------------------------
pinned="$(sed -n 's/^emsdk: *//p' "$ROOT/docs/toolchain.md" 2>/dev/null | head -n 1)"
if command -v em++ > /dev/null; then
  found="$(first_version "$(em++ --version 2>&1 | head -n 1)")"
  if [ -n "$pinned" ] && [ "$found" = "$pinned" ]; then
    pass "em++ (emsdk)" "$found, matches docs/toolchain.md"
  else
    fail "em++ (emsdk)" "$found, but docs/toolchain.md pins '${pinned:-nothing}'"
  fi
else
  fail "em++ (emsdk)" "em++ not found (looked in $EMSDK_DIR)"
fi

# --- Playwright browser ---------------------------------------------------------------
chromium_dirs="$(ls -d "$HOME"/.cache/ms-playwright/chromium-* 2>/dev/null | xargs -r -n 1 basename | xargs)"
if [ -n "$chromium_dirs" ]; then pass "Playwright Chromium" "$chromium_dirs"; else fail "Playwright Chromium" "not installed in ~/.cache/ms-playwright"; fi

# --- Optional tools ---------------------------------------------------------------------
if command -v gh > /dev/null; then pass "gh (optional)" "$(first_version "$(gh --version)")"; else skip "gh (optional)" "not installed"; fi

# --- Claude Code and MCP servers --------------------------------------------------------
if command -v claude > /dev/null; then
  check_min "Claude Code" "2.1.280" claude --version
  mcp_list="$(cd "$ROOT" && timeout 120 claude mcp list 2>&1)"
  for server in playwright context7; do
    line="$(grep -m 1 "^$server:" <<< "$mcp_list")"
    if [[ "$line" == *Connected* ]]; then
      pass "MCP $server" "connected"
    elif [ -n "$line" ]; then
      fail "MCP $server" "${line##* - }"
    else
      fail "MCP $server" "not configured"
    fi
  done
else
  fail "Claude Code" "claude not found"
  fail "MCP servers" "cannot check without claude"
fi

# --- Repository ---------------------------------------------------------------------------
if git -C "$ROOT" rev-parse --is-inside-work-tree > /dev/null 2>&1 && [ -f "$ROOT/CLAUDE.md" ]; then
  pass "Git repo" "$ROOT contains CLAUDE.md"
else
  fail "Git repo" "$ROOT is not a git repository with CLAUDE.md"
fi

echo
if [ "$failures" -eq 0 ]; then
  echo "All checks passed."
  exit 0
fi
echo "$failures check(s) failed."
exit 1

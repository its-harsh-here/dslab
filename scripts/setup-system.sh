#!/usr/bin/env bash
# DSLab: one-time privileged setup for Ubuntu 24.04 on WSL2 (ARM64).
#
# Run it as yourself, NOT with sudo:
#     bash scripts/setup-system.sh
# It calls sudo itself only where it must. Safe to re-run: every step checks first.
set -euo pipefail

NVM_VERSION="v0.40.8"   # latest nvm release on 2026-10-22
NODE_MAJOR="24"         # Node 24 LTS (Krypton)
NODE_MINIMUM="22.12.0"  # oldest Node DSLab accepts
# Everything DSLab needs from apt. The 2026-10-22 audit found ninja-build and unzip missing;
# the rest are listed so a re-run on another machine still installs whatever is absent.
APT_PACKAGES=(build-essential cmake ninja-build python3 git curl unzip xz-utils ca-certificates)

step() { printf '\n==> %s\n' "$*"; }

# True when version $1 >= version $2 (both like 22.12.0, no leading v).
version_at_least() {
  [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n 1)" = "$2" ]
}

if [ "$(id -u)" -eq 0 ]; then
  echo "Run this as your normal user (bash scripts/setup-system.sh), not with sudo." >&2
  exit 1
fi

# ---------------------------------------------------------------------------
step "1/3 apt packages"
missing=()
for pkg in "${APT_PACKAGES[@]}"; do
  status="$(dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null || true)"
  if [ "$status" = "install ok installed" ]; then
    echo "    ok       $pkg"
  else
    echo "    missing  $pkg"
    missing+=("$pkg")
  fi
done
if [ "${#missing[@]}" -gt 0 ]; then
  echo "    installing: ${missing[*]} (sudo will ask for your password)"
  sudo apt-get update
  sudo apt-get install -y "${missing[@]}"
else
  echo "    nothing to install"
fi

# ---------------------------------------------------------------------------
step "2/3 nvm and Node ${NODE_MAJOR} LTS"
export NVM_DIR="$HOME/.nvm"
if [ -s "$NVM_DIR/nvm.sh" ]; then
  echo "    nvm already installed in $NVM_DIR"
else
  backup="$HOME/.bashrc.bak-before-nvm-$(date +%Y%m%d-%H%M%S)"
  cp "$HOME/.bashrc" "$backup"
  echo "    backed up ~/.bashrc to $backup (the nvm installer appends its loader lines)"
  echo "    installing nvm ${NVM_VERSION}"
  curl -fsSL "https://raw.githubusercontent.com/nvm-sh/nvm/${NVM_VERSION}/install.sh" | bash
fi

# nvm.sh is not written for 'set -e' or 'set -u', so relax them while nvm runs
# and check each nvm command explicitly instead.
set +eu
. "$NVM_DIR/nvm.sh"
current="$(nvm version default)"   # e.g. v24.22.1, or N/A when there is no default yet
if [ "$current" != "N/A" ] && version_at_least "${current#v}" "$NODE_MINIMUM"; then
  echo "    nvm default Node is $current (>= $NODE_MINIMUM), leaving it"
else
  echo "    nvm default Node is $current; installing Node ${NODE_MAJOR} and making it the default"
  nvm install "$NODE_MAJOR" || { echo "nvm install $NODE_MAJOR failed" >&2; exit 1; }
  nvm alias default "$NODE_MAJOR" || { echo "nvm alias default failed" >&2; exit 1; }
fi
nvm use default > /dev/null || { echo "nvm use default failed" >&2; exit 1; }
set -eu
echo "    node $(node --version) at $(command -v node)"
echo "    npm  $(npm --version)"

# ---------------------------------------------------------------------------
step "3/3 Playwright bundled Chromium and its system libraries (asks for sudo itself)"
npx -y playwright install --with-deps chromium

step "Done. Go back to Claude and type: done"

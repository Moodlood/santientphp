#!/usr/bin/env bash
# ==============================================================================
# SantientPHP (SPH) OS-Aware Installer for Linux & macOS
# Automatically detects OS/distro, checks and installs developer prerequisites,
# builds SantientPHP from source, and installs binaries to /usr/local/bin.
# ==============================================================================

set -e

# ANSI Color codes
BOLD="\033[1m"
GREEN="\033[0;32m"
CYAN="\033[0;36m"
YELLOW="\033[1;33m"
RED="\033[0;31m"
GRAY="\033[0;90m"
RESET="\033[0m"

log_step() { echo -e "${CYAN}==>${RESET} ${BOLD}$1${RESET}"; }
log_ok()   { echo -e "  ${GREEN}[OK]${RESET} $1"; }
log_warn() { echo -e "  ${YELLOW}[WARN]${RESET} $1"; }
log_err()  { echo -e "  ${RED}[ERROR]${RESET} $1"; }

echo ""
echo -e "${CYAN}============================================================${RESET}"
echo -e "${BOLD}       SantientPHP OS-Aware Installer (Linux & macOS)       ${RESET}"
echo -e "${CYAN}============================================================${RESET}"

# 1. OS & Architecture Detection
OS_TYPE="$(uname -s)"
ARCH_TYPE="$(uname -m)"
DISTRO="unknown"

if [ "$OS_TYPE" = "Linux" ]; then
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO="$ID"
    fi
    echo -e "  Operating System: ${BOLD}Linux ($DISTRO)${RESET} (${ARCH_TYPE})"
elif [ "$OS_TYPE" = "Darwin" ]; then
    echo -e "  Operating System: ${BOLD}macOS Darwin${RESET} (${ARCH_TYPE})"
else
    echo -e "  Operating System: ${BOLD}$OS_TYPE${RESET} (${ARCH_TYPE})"
fi
echo -e "${CYAN}============================================================${RESET}"
echo ""

# 2. Check and Install Missing Prerequisites
log_step "Auditing required developer prerequisites..."

MISSING_PKGS=()

has_cmd() { command -v "$1" >/dev/null 2>&1; }

# Check C++20 Compiler (GCC >= 14 or Clang >= 17)
if has_cmd g++ || has_cmd clang++; then
    COMPILER="$(command -v g++ || command -v clang++)"
    log_ok "C/C++ Compiler found: $COMPILER"
else
    log_warn "C/C++ Compiler missing (C++20 capable g++ or clang++ required)"
    MISSING_PKGS+=("compiler")
fi

# Check CMake
if has_cmd cmake; then
    log_ok "CMake found: $(cmake --version | head -n1)"
else
    log_warn "CMake missing"
    MISSING_PKGS+=("cmake")
fi

# Check Ninja
if has_cmd ninja; then
    log_ok "Ninja found: $(ninja --version)"
else
    log_warn "Ninja build tool missing"
    MISSING_PKGS+=("ninja")
fi

# Check Git
if has_cmd git; then
    log_ok "Git found: $(git --version)"
else
    log_warn "Git missing"
    MISSING_PKGS+=("git")
fi

# Check SQLite development headers (for runtime PDO/SQLite support)
if [ "$OS_TYPE" = "Linux" ]; then
    if ! pkg-config --exists sqlite3 2>/dev/null && [ ! -f /usr/include/sqlite3.h ]; then
        log_warn "SQLite3 development headers missing"
        MISSING_PKGS+=("sqlite")
    fi
fi

# Auto-install missing packages using detected system package manager
if [ ${#MISSING_PKGS[@]} -gt 0 ]; then
    log_step "Automatically installing missing prerequisites via system package manager..."

    if [ "$OS_TYPE" = "Linux" ]; then
        if has_cmd apt-get; then
            log_step "Running apt-get update & install..."
            sudo apt-get update
            PKGS_TO_INSTALL=()
            for p in "${MISSING_PKGS[@]}"; do
                case "$p" in
                    compiler) PKGS_TO_INSTALL+=("build-essential" "g++") ;;
                    cmake)    PKGS_TO_INSTALL+=("cmake") ;;
                    ninja)    PKGS_TO_INSTALL+=("ninja-build") ;;
                    git)      PKGS_TO_INSTALL+=("git") ;;
                    sqlite)   PKGS_TO_INSTALL+=("libsqlite3-dev") ;;
                esac
            done
            sudo apt-get install -y "${PKGS_TO_INSTALL[@]}"
        elif has_cmd dnf; then
            log_step "Running dnf install..."
            sudo dnf install -y gcc gcc-c++ cmake ninja-build git sqlite-devel
        elif has_cmd pacman; then
            log_step "Running pacman -S..."
            sudo pacman -S --noconfirm --needed base-devel cmake ninja git sqlite
        elif has_cmd zypper; then
            log_step "Running zypper install..."
            sudo zypper install -y gcc-c++ cmake ninja git sqlite3-devel
        elif has_cmd apk; then
            log_step "Running apk add..."
            sudo apk add build-base cmake ninja git sqlite-dev
        else
            log_err "No supported package manager found (apt, dnf, pacman, zypper, apk)."
            log_err "Please manually install: g++, cmake, ninja, git, sqlite3-dev"
            exit 1
        fi
    elif [ "$OS_TYPE" = "Darwin" ]; then
        if has_cmd brew; then
            log_step "Running brew install..."
            brew install cmake ninja git gcc sqlite
        else
            log_err "Homebrew (brew) is required to auto-install dependencies on macOS."
            log_err "Please install Homebrew from https://brew.sh or install cmake, ninja, git manually."
            exit 1
        fi
    fi
    log_ok "Prerequisites successfully installed."
else
    log_ok "All developer toolchain prerequisites are verified!"
fi

# 3. Locate Source Directory & Build
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

log_step "Building SantientPHP using CMake & Ninja..."
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

SPHC_BIN="build/bin/sphc"
if [ ! -f "$SPHC_BIN" ]; then
    log_err "Compiler binary '$SPHC_BIN' was not found after build!"
    exit 1
fi

# 4. Install to /usr/local
INSTALL_PREFIX="${PREFIX:-/usr/local}"
log_step "Installing SantientPHP to ${INSTALL_PREFIX}..."

sudo mkdir -p "${INSTALL_PREFIX}/bin"
sudo mkdir -p "${INSTALL_PREFIX}/lib"
sudo mkdir -p "${INSTALL_PREFIX}/share/santientphp/stdlib"
sudo mkdir -p "${INSTALL_PREFIX}/share/santientphp/examples"
sudo mkdir -p "${INSTALL_PREFIX}/share/santientphp/docs"

# Copy compiler and aliases
sudo cp -f "$SPHC_BIN" "${INSTALL_PREFIX}/bin/sphc"
sudo cp -f "$SPHC_BIN" "${INSTALL_PREFIX}/bin/sph"
sudo cp -f "$SPHC_BIN" "${INSTALL_PREFIX}/bin/santientphp"
sudo chmod 755 "${INSTALL_PREFIX}/bin/sphc" "${INSTALL_PREFIX}/bin/sph" "${INSTALL_PREFIX}/bin/santientphp"

# Copy runtime static libraries
if [ -d build/lib ]; then
    sudo cp -f build/lib/libsph*.a "${INSTALL_PREFIX}/lib/" 2>/dev/null || true
fi

# Copy standard library, examples, docs
if [ -d stdlib ]; then
    sudo cp -rf stdlib/* "${INSTALL_PREFIX}/share/santientphp/stdlib/"
fi
if [ -d examples ]; then
    sudo cp -rf examples/* "${INSTALL_PREFIX}/share/santientphp/examples/"
fi
if [ -d docs ]; then
    sudo cp -rf docs/* "${INSTALL_PREFIX}/share/santientphp/docs/"
fi

# Copy config
if [ -f sph.ini ]; then
    sudo mkdir -p /etc/santientphp
    sudo cp -f sph.ini /etc/santientphp/sph.ini
fi

# 5. Verify Toolchain
log_step "Verifying installation with sphc doctor..."
"${INSTALL_PREFIX}/bin/sphc" doctor || true

echo ""
echo -e "${GREEN}============================================================${RESET}"
echo -e "${BOLD}      SantientPHP Installation Completed Successfully!       ${RESET}"
echo -e "${GREEN}============================================================${RESET}"
echo -e "Installed binaries:"
echo -e "  * ${CYAN}${INSTALL_PREFIX}/bin/sphc${RESET}       (Primary compiler command)"
echo -e "  * ${CYAN}${INSTALL_PREFIX}/bin/sph${RESET}        (PHP-like CLI alias)"
echo -e "  * ${CYAN}${INSTALL_PREFIX}/bin/santientphp${RESET} (Full binary command)"
echo ""

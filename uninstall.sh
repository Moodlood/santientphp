#!/usr/bin/env bash
# ==============================================================================
# SantientPHP (SPH) OS-Aware Uninstaller for Linux & macOS
# Completely removes SantientPHP binaries, runtime libraries, and standard library.
# ==============================================================================

set -e

BOLD="\033[1m"
GREEN="\033[0;32m"
CYAN="\033[0;36m"
YELLOW="\033[1;33m"
RED="\033[0;31m"
RESET="\033[0m"

log_step() { echo -e "${CYAN}==>${RESET} ${BOLD}$1${RESET}"; }
log_ok()   { echo -e "  ${GREEN}[OK]${RESET} $1"; }
log_warn() { echo -e "  ${YELLOW}[WARN]${RESET} $1"; }

echo ""
echo -e "${CYAN}============================================================${RESET}"
echo -e "${BOLD}      SantientPHP OS-Aware Uninstaller (Linux & macOS)      ${RESET}"
echo -e "${CYAN}============================================================${RESET}"

OS_TYPE="$(uname -s)"
echo -e "  Detected OS: ${BOLD}$OS_TYPE${RESET}"
echo ""

INSTALL_PREFIX="${PREFIX:-/usr/local}"

log_step "Removing SantientPHP binaries from ${INSTALL_PREFIX}/bin..."
for bin in sphc sph santientphp sph_tests; do
    if [ -f "${INSTALL_PREFIX}/bin/$bin" ]; then
        sudo rm -f "${INSTALL_PREFIX}/bin/$bin"
        log_ok "Removed ${INSTALL_PREFIX}/bin/$bin"
    fi
done

log_step "Removing runtime static libraries from ${INSTALL_PREFIX}/lib..."
for lib in libsph_runtime.a libsph_core.a libsph_async.a libsph_compat.a libsphc_core.a; do
    if [ -f "${INSTALL_PREFIX}/lib/$lib" ]; then
        sudo rm -f "${INSTALL_PREFIX}/lib/$lib"
        log_ok "Removed ${INSTALL_PREFIX}/lib/$lib"
    fi
done

log_step "Removing SantientPHP shared assets and libraries..."
if [ -d "${INSTALL_PREFIX}/share/santientphp" ]; then
    sudo rm -rf "${INSTALL_PREFIX}/share/santientphp"
    log_ok "Removed ${INSTALL_PREFIX}/share/santientphp"
fi

if [ -d "/etc/santientphp" ]; then
    sudo rm -rf "/etc/santientphp"
    log_ok "Removed /etc/santientphp"
fi

echo ""
echo -e "${GREEN}============================================================${RESET}"
echo -e "${BOLD}    SantientPHP has been successfully uninstalled.          ${RESET}"
echo -e "${GREEN}============================================================${RESET}"
echo ""

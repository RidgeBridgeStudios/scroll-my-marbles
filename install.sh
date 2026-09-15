#!/usr/bin/env bash
# ==============================================================================
# Scroll My Marbles - Automated Easy Installer
# Repository: https://github.com/RidgeBridgeStudios/scroll-my-marbles
# ==============================================================================
set -euo pipefail

REPO="RidgeBridgeStudios/scroll-my-marbles"
DEFAULT_VERSION="1.2.0"
VERSION="${VERSION:-$DEFAULT_VERSION}"
PACKAGE_NAME="scroll-my-marbles"
PACKAGE_ARCH="amd64"
DEB_NAME="${PACKAGE_NAME}_${VERSION}_${PACKAGE_ARCH}.deb"

# Colors for terminal output
BOLD="\033[1m"
GREEN="\033[1;32m"
BLUE="\033[1;34m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
RESET="\033[0m"

info() {
    echo -e "${BLUE}==>${RESET} ${BOLD}$1${RESET}"
}

success() {
    echo -e "${GREEN}==>${RESET} ${BOLD}$1${RESET}"
}

warn() {
    echo -e "${YELLOW}Warning:${RESET} $1"
}

error() {
    echo -e "${RED}Error:${RESET} $1" >&2
}

# Banner
print_banner() {
    echo -e "${BOLD}${BLUE}"
    echo "=========================================================="
    echo "       Scroll My Marbles - Easy Installer                "
    echo "   Universal Scrolling & Remapping for Linux Trackballs   "
    echo "=========================================================="
    echo -e "${RESET}"
}

# Usage / Help
show_help() {
    print_banner
    echo "Usage: ./install.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --uninstall    Remove Scroll My Marbles and associated files"
    echo "  --help, -h     Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  VERSION        Specify version to download (default: ${DEFAULT_VERSION})"
    echo ""
}

# Handle uninstall
do_uninstall() {
    print_banner
    info "Uninstalling Scroll My Marbles..."

    if command -v apt-get >/dev/null 2>&1 && dpkg -l | grep -q "^ii  ${PACKAGE_NAME} "; then
        sudo apt-get remove -y "${PACKAGE_NAME}"
    else
        echo "Removing installed files manually..."
        sudo rm -f "/usr/bin/${PACKAGE_NAME}"
        sudo rm -f "/usr/share/applications/${PACKAGE_NAME}.desktop"
        sudo rm -f "/lib/udev/rules.d/99-scroll-my-marbles.rules"
        sudo rm -f "/usr/lib/udev/rules.d/99-scroll-my-marbles.rules"
        sudo rm -rf "/usr/share/icons/hicolor"/*/apps/"${PACKAGE_NAME}".*
        sudo rm -rf "/usr/share/scroll-my-marbles"
    fi

    if command -v udevadm >/dev/null 2>&1; then
        info "Reloading udev rules..."
        sudo udevadm control --reload-rules || true
        sudo udevadm trigger --subsystem-match=input || true
    fi

    success "Scroll My Marbles has been completely uninstalled."
    exit 0
}

# Check flags
if [[ "${1:-}" == "--uninstall" ]]; then
    do_uninstall
elif [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    show_help
    exit 0
fi

print_banner

# Step 1: Detect Operating System & Architecture
info "[1/4] Checking system compatibility..."

OS_TYPE="$(uname -s)"
if [ "$OS_TYPE" != "Linux" ]; then
    error "Scroll My Marbles is designed for Linux systems. Detected: $OS_TYPE"
    exit 1
fi

ARCH="$(uname -m)"
if [ "$ARCH" != "x86_64" ]; then
    error "Prebuilt packages are currently only available for x86_64 (amd64). Detected: $ARCH"
    echo "You can still compile from source using meson and ninja. See README.md for details."
    exit 1
fi

if [ -f /etc/os-release ]; then
    # shellcheck source=/dev/null
    source /etc/os-release
    OS_NAME="${PRETTY_NAME:-Linux}"
    echo "  Detected OS: $OS_NAME ($ARCH)"
else
    echo "  Detected generic Linux ($ARCH)"
fi

# Step 2: Locate or Download .deb package
info "[2/4] Obtaining installation package..."

WORK_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

DEB_DEST="${WORK_DIR}/${DEB_NAME}"
PACKAGE_FOUND=0

# Check for local .deb in current folder or build/
for candidate in "./${DEB_NAME}" "./${PACKAGE_NAME}_${VERSION}-1_${PACKAGE_ARCH}.deb" "./build/${DEB_NAME}" "../${DEB_NAME}"; do
    if [ -f "$candidate" ]; then
        echo "  Using existing local package: $candidate"
        cp "$candidate" "$DEB_DEST"
        PACKAGE_FOUND=1
        break
    fi
done

# If not local, download from GitHub Releases
if [ "$PACKAGE_FOUND" -eq 0 ]; then
    DIRECT_URL="https://github.com/${REPO}/releases/download/v${VERSION}/${DEB_NAME}"
    echo "  Downloading from: $DIRECT_URL"

    DOWNLOAD_SUCCESS=0
    if command -v curl >/dev/null 2>&1; then
        if curl -fL --progress-bar -o "$DEB_DEST" "$DIRECT_URL"; then
            DOWNLOAD_SUCCESS=1
        fi
    elif command -v wget >/dev/null 2>&1; then
        if wget -q --show-progress -O "$DEB_DEST" "$DIRECT_URL"; then
            DOWNLOAD_SUCCESS=1
        fi
    else
        error "Neither 'curl' nor 'wget' was found. Please install curl or wget."
        exit 1
    fi

    # Fallback to GitHub API lookup for latest release if direct URL fails
    if [ "$DOWNLOAD_SUCCESS" -eq 0 ]; then
        echo "  Direct download failed, checking latest GitHub release asset..."
        API_URL="https://api.github.com/repos/${REPO}/releases/latest"
        LATEST_URL=""
        if command -v curl >/dev/null 2>&1; then
            LATEST_URL="$(curl -fsSL "$API_URL" | grep "browser_download_url.*${PACKAGE_ARCH}\.deb" | cut -d '"' -f 4 | head -n 1 || true)"
        fi

        if [ -n "$LATEST_URL" ]; then
            echo "  Found latest release asset: $LATEST_URL"
            curl -fL --progress-bar -o "$DEB_DEST" "$LATEST_URL"
            DOWNLOAD_SUCCESS=1
        fi
    fi

    if [ "$DOWNLOAD_SUCCESS" -eq 0 ] || [ ! -s "$DEB_DEST" ]; then
        error "Failed to download Debian package from GitHub Releases."
        echo "Please visit https://github.com/${REPO}/releases to manually download the package,"
        echo "or build from source using: meson setup build && ninja -C build"
        exit 1
    fi
fi

# Step 3: Install Package
info "[3/4] Installing Scroll My Marbles..."

if command -v apt-get >/dev/null 2>&1; then
    echo "  Running apt-get install (requires sudo password)..."
    sudo apt-get update -qq || true
    sudo apt-get install -y "$DEB_DEST"
elif command -v dpkg >/dev/null 2>&1; then
    echo "  Running dpkg -i (requires sudo password)..."
    sudo dpkg -i "$DEB_DEST"
else
    error "Neither apt-get nor dpkg was found on this system."
    exit 1
fi

# Step 4: Refresh Permissions & Desktop Integration
info "[4/4] Configuring hardware permissions and system integration..."

if command -v udevadm >/dev/null 2>&1; then
    echo "  Reloading udev rules for trackball and uinput..."
    sudo udevadm control --reload-rules || true
    sudo udevadm trigger --subsystem-match=input || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    sudo update-desktop-database -q || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    sudo gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi

# Final Output & Next Steps
echo ""
echo -e "${GREEN}==========================================================${RESET}"
echo -e "${GREEN}    Scroll My Marbles has been installed successfully!    ${RESET}"
echo -e "${GREEN}==========================================================${RESET}"
echo ""
echo -e "${BOLD}How to run:${RESET}"
echo "  1. Search for ${BOLD}Scroll My Marbles${RESET} in your Application Menu / Dash"
echo "  2. Or run from terminal: ${BOLD}scroll-my-marbles${RESET}"
echo "  3. To open settings directly: ${BOLD}scroll-my-marbles --settings${RESET}"
echo ""
echo -e "${BOLD}💡 First-Time Tip for Beginners:${RESET}"
echo "  If scrolling doesn't respond right away on the first run, simply"
echo "  ${BOLD}unplug and reconnect your trackball${RESET} (or log out and back in)"
echo "  so Linux activates the new permission rule."
echo ""
echo -e "${BOLD}How to scroll:${RESET}"
echo "  Hold down your modifier button (${BOLD}Middle Button${RESET} by default) and roll"
echo "  the trackball to scroll. A quick click without moving gives a normal middle click!"
echo ""
echo "To uninstall anytime, run:"
echo "  curl -fsSL https://raw.githubusercontent.com/${REPO}/main/install.sh | bash -s -- --uninstall"
echo "  or: sudo apt remove scroll-my-marbles"
echo ""

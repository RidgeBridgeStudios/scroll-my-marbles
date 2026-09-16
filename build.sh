#!/usr/bin/env bash
# ==============================================================================
# Scroll My Marbles - Build & Packaging Script
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_NAME="scroll-my-marbles"
PACKAGE_VERSION="1.2.1"
PACKAGE_ARCH="amd64"
PACKAGE_FULLNAME="${PACKAGE_NAME}_${PACKAGE_VERSION}_${PACKAGE_ARCH}"
DEB_FILE="${SCRIPT_DIR}/${PACKAGE_FULLNAME}.deb"
TAR_FILE="${SCRIPT_DIR}/${PACKAGE_NAME}-${PACKAGE_VERSION}-linux-x86_64.tar.gz"
BUILD_DIR="${SCRIPT_DIR}/build"
STAGING_DIR="${BUILD_DIR}/staging/${PACKAGE_FULLNAME}"

echo "=== Building Scroll My Marbles: ${PACKAGE_FULLNAME} ==="

# 1. Check for required build tools
echo "[1/6] Checking required build tools..."
MISSING_TOOLS=0

for tool in meson ninja gcc dpkg-deb tar gzip sha256sum; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Error: Required build tool '$tool' is not installed." >&2
        MISSING_TOOLS=1
    fi
done

if [ "$MISSING_TOOLS" -ne 0 ]; then
    echo "Install build dependencies with:" >&2
    echo "  sudo apt install -y build-essential meson ninja-build pkg-config libglib2.0-dev libevdev-dev libgtk-4-dev libadwaita-1-dev debhelper" >&2
    exit 1
fi

# 2. Configure Meson & Compile
echo "[2/6] Configuring and compiling with Meson..."
if [ -d "$BUILD_DIR/meson-private" ]; then
    ninja -C "$BUILD_DIR"
else
    meson setup "$BUILD_DIR" --prefix=/usr --buildtype=release
    ninja -C "$BUILD_DIR"
fi

# 3. Run Unit Tests
echo "[3/6] Running unit tests..."
ninja -C "$BUILD_DIR" test

# 4. Assemble Debian Package Staging Directory
echo "[4/6] Assembling Debian package staging tree..."
rm -rf "$STAGING_DIR"
mkdir -p "${STAGING_DIR}/DEBIAN"
mkdir -p "${STAGING_DIR}/usr/bin"
mkdir -p "${STAGING_DIR}/usr/share/applications"
mkdir -p "${STAGING_DIR}/usr/share/icons/hicolor"
mkdir -p "${STAGING_DIR}/usr/share/scroll-my-marbles"
mkdir -p "${STAGING_DIR}/lib/udev/rules.d"
mkdir -p "${STAGING_DIR}/usr/share/doc/${PACKAGE_NAME}"

# Install compiled binary
cp -a "${BUILD_DIR}/scroll-my-marbles" "${STAGING_DIR}/usr/bin/"

# Install desktop entry, udev rule and branding asset
cp -a "${SCRIPT_DIR}/data/scroll-my-marbles.desktop" "${STAGING_DIR}/usr/share/applications/"
cp -a "${SCRIPT_DIR}/data/70-scroll-my-marbles.rules" "${STAGING_DIR}/lib/udev/rules.d/"
cp -a "${SCRIPT_DIR}/data/branding/ridgebridge-studios.png" "${STAGING_DIR}/usr/share/scroll-my-marbles/"

# Install icons
cp -r "${SCRIPT_DIR}/data/icons/hicolor/"* "${STAGING_DIR}/usr/share/icons/hicolor/"

# Install Debian control and maintainer files
cat << EOF > "${STAGING_DIR}/DEBIAN/control"
Package: scroll-my-marbles
Version: ${PACKAGE_VERSION}
Architecture: amd64
Maintainer: RidgeBridgeStudios <contact@example.com>
Section: utils
Priority: optional
Depends: libc6, libglib2.0-0t64 | libglib2.0-0, libgtk-4-1, libadwaita-1-0, libevdev2
Recommends: udev
Homepage: https://github.com/RidgeBridgeStudios/scroll-my-marbles
Description: Scroll emulation and button remapping for Logitech TrackMan Marble FX
 Scroll My Marbles provides an alternative scrolling solution for pointing
 devices without a dedicated scroll wheel, specifically targeting the Logitech
 TrackMan Marble FX trackball. It allows scrolling by holding a configurable
 button and moving the trackball, with customizable sensitivities, smooth
 high-resolution scrolling, reverse scrolling, and button remapping. It operates
 natively at the evdev/uinput level, working seamlessly on both X11 and Wayland
 sessions without root privileges.
EOF
chmod 0644 "${STAGING_DIR}/DEBIAN/control"
[ -f "${SCRIPT_DIR}/debian/postinst" ] && cp -a "${SCRIPT_DIR}/debian/postinst" "${STAGING_DIR}/DEBIAN/postinst"
[ -f "${SCRIPT_DIR}/debian/postrm" ]  && cp -a "${SCRIPT_DIR}/debian/postrm"  "${STAGING_DIR}/DEBIAN/postrm"

# Documentation
if [ -f "${SCRIPT_DIR}/debian/changelog" ]; then
    gzip -9 -n -c "${SCRIPT_DIR}/debian/changelog" > "${STAGING_DIR}/usr/share/doc/${PACKAGE_NAME}/changelog.Debian.gz"
fi
if [ -f "${SCRIPT_DIR}/LICENSE" ]; then
    cp -a "${SCRIPT_DIR}/LICENSE" "${STAGING_DIR}/usr/share/doc/${PACKAGE_NAME}/copyright"
fi

# Set permissions
find "$STAGING_DIR" -type d -exec chmod 0755 {} +
find "$STAGING_DIR" -type f -exec chmod 0644 {} +
chmod 0755 "${STAGING_DIR}/usr/bin/scroll-my-marbles"
[ -f "${STAGING_DIR}/DEBIAN/postinst" ] && chmod 0755 "${STAGING_DIR}/DEBIAN/postinst"
[ -f "${STAGING_DIR}/DEBIAN/postrm" ]  && chmod 0755 "${STAGING_DIR}/DEBIAN/postrm"

# Build .deb package
echo "[5/6] Building Debian package (.deb)..."
dpkg-deb --root-owner-group --build "$STAGING_DIR" "$DEB_FILE"

# 5. Build Portable Tarball Archive
echo "[6/6] Packaging portable tarball archive..."
TAR_STAGING="${BUILD_DIR}/staging/tarball/${PACKAGE_NAME}-${PACKAGE_VERSION}"
rm -rf "$TAR_STAGING"
mkdir -p "${TAR_STAGING}/bin"
mkdir -p "${TAR_STAGING}/share/applications"
mkdir -p "${TAR_STAGING}/share/icons"
mkdir -p "${TAR_STAGING}/share/scroll-my-marbles"
mkdir -p "${TAR_STAGING}/udev"

cp -a "${BUILD_DIR}/scroll-my-marbles" "${TAR_STAGING}/bin/"
cp -a "${SCRIPT_DIR}/data/scroll-my-marbles.desktop" "${TAR_STAGING}/share/applications/"
cp -r "${SCRIPT_DIR}/data/icons/hicolor" "${TAR_STAGING}/share/icons/"
cp -a "${SCRIPT_DIR}/data/branding/ridgebridge-studios.png" "${TAR_STAGING}/share/scroll-my-marbles/"
cp -a "${SCRIPT_DIR}/data/70-scroll-my-marbles.rules" "${TAR_STAGING}/udev/"
cp -a "${SCRIPT_DIR}/install.sh" "${TAR_STAGING}/"
cp -a "${SCRIPT_DIR}/README.md" "${TAR_STAGING}/"
cp -a "${SCRIPT_DIR}/LICENSE" "${TAR_STAGING}/"

tar -czf "$TAR_FILE" -C "${BUILD_DIR}/staging/tarball" "${PACKAGE_NAME}-${PACKAGE_VERSION}"

# Generate Checksums
cd "$SCRIPT_DIR"
sha256sum "$(basename "$DEB_FILE")" "$(basename "$TAR_FILE")" > "${SCRIPT_DIR}/SHA256SUMS.txt"

echo ""
echo "=== Build Complete! ==="
echo "Debian Package: $DEB_FILE ($(du -h "$DEB_FILE" | cut -f1))"
echo "Tarball:        $TAR_FILE ($(du -h "$TAR_FILE" | cut -f1))"
echo "Checksums saved to SHA256SUMS.txt:"
cat "${SCRIPT_DIR}/SHA256SUMS.txt"

#!/bin/bash
# ============================================================================
# release.sh - Compila y publica un release de firmware en GitHub
#
# Uso (desde la carpeta del proyecto PlatformIO):
#   1. Cambia la versión en platformio.ini  →  -DFIRMWARE_VERSION=\"1.0.1\"
#   2. ./release.sh
#
# Requiere GitHub CLI:  brew install gh   →   gh auth login
# ============================================================================
set -e

FW_REPO="jota0018/LEGO_MOD-firmware"      # Repo PÚBLICO de firmware
BIN=".pio/build/esp32dev/firmware.bin"

# --- Leer versión desde platformio.ini ---
VERSION=$(sed -n 's/.*FIRMWARE_VERSION=\\"\([0-9][0-9.]*\)\\".*/\1/p' platformio.ini)
if [ -z "$VERSION" ]; then
  echo "❌ No encontré FIRMWARE_VERSION en platformio.ini"
  exit 1
fi
echo "📦 Versión: v$VERSION"

# --- Compilar ---
PIO=$(command -v pio || echo "$HOME/.platformio/penv/bin/pio")
echo "🔨 Compilando..."
"$PIO" run

# --- SHA256 y tamaño ---
SHA=$(shasum -a 256 "$BIN" | cut -d' ' -f1)
SIZE=$(stat -f%z "$BIN" 2>/dev/null || stat -c%s "$BIN")
echo "🔐 SHA256: $SHA"
echo "📏 Tamaño: $SIZE bytes"

# --- Manifest ---
cat > version.json <<EOF_JSON
{
  "version": "$VERSION",
  "sha256": "$SHA",
  "size": $SIZE,
  "file": "firmware.bin"
}
EOF_JSON
echo "📝 version.json generado"

# --- Publicar release en GitHub ---
echo "🚀 Publicando v$VERSION en $FW_REPO..."
gh release create "v$VERSION" "$BIN" version.json \
  --repo "$FW_REPO" \
  --title "v$VERSION" \
  --notes "Firmware v$VERSION"

echo ""
echo "✅ Release v$VERSION publicado."
echo "   Los dispositivos lo verán con triple click."

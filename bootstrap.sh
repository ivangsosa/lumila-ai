#!/bin/bash
# Bootstrap script for Lumila AI plugin

set -e

echo "Bootstrapping Lumila AI plugin..."

# Create build-aux directory
mkdir -p build-aux m4

# Run autoreconf
autoreconf -fvi

echo ""
echo "Bootstrap complete. Now run:"
echo "  ./configure"
echo "  make"
echo "  sudo make install"

#!/bin/bash

# Script to install proot-distro configurations
# Copies files from distros directory to $PREFIX/etc/proot-distro/

set -e  # Exit on any error

# Check if running in Termux
if [ -z "$PREFIX" ]; then
    echo "Error: PREFIX environment variable is not set. This script is intended for use in Termux."
    exit 1
fi

DISTROS_DIR="$PREFIX/etc/proot-distro"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Installing proot-distro configurations..."

# Create the target directory if it doesn't exist
mkdir -p "$DISTROS_DIR"

# Copy all files from the distros directory to the target directory
if [ -d "$SCRIPT_DIR/distros" ]; then
    cp -r "$SCRIPT_DIR/distros/"* "$DISTROS_DIR/"
    echo "Files copied successfully to $DISTROS_DIR"
else
    echo "Warning: distros directory does not exist or is empty"
    exit 1
fi

echo "Installation completed!"
echo "You can now use proot-distro with the installed configurations."
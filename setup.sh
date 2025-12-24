#!/bin/bash

# Stop on any error
set -e

echo "=========================================="
echo "   Uhbik Wrapper - Setup & Build Script"
echo "   (Clean Rebuild Version)"
echo "=========================================="

echo "[1/4] Installing System Dependencies..."
echo "Please enter your password if prompted:"

# Ensure all dependencies are definitely installed
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    pkg-config \
    libasound2-dev \
    libjack-jackd2-dev \
    libfreetype6-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxext-dev \
    libxrandr-dev \
    libglu1-mesa-dev \
    libgtk-3-dev \
    mesa-common-dev

echo "Dependencies installed."
echo ""

# Navigate to the script's directory (UhbikWrapper root)
cd "$(dirname "$0")"

echo "[2/4] Cleaning previous build..."
rm -rf build
mkdir -p build
cd build

echo "[3/4] Configuring Project with CMake..."
# FORCE include path for freetype2 directly into CXX flags
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-I/usr/include/freetype2"

echo ""
echo "[4/4] Compiling..."
# Build using all available CPU cores
make -j$(nproc)

echo ""
echo "=========================================="
echo "   BUILD COMPLETE!"
echo "=========================================="
echo "Your VST3 plugin is located here:"
find . -name "Uhbik Wrapper.vst3" -type d
echo ""
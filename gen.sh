#!/usr/bin/env bash
set -e
export DEBIAN_FRONTEND=noninteractive
export PAGER=cat

echo "Updating package lists..."
sudo apt-get update

echo "Installing required dependencies..."
sudo apt-get install -y \
    build-essential \
    g++-14 \
    clang \
    lld \
    cmake \
    ninja-build \
    pkg-config \
    git \
    make \
    tar \
    expect \
    python3 \
    python3-pip \
    python3-venv \
    libvulkan-dev \
    vulkan-tools \
    mesa-vulkan-drivers \
    libwayland-dev \
    libxkbcommon-dev \
    xorg-dev \
    libasound2-dev \
    libpulse-dev \
    libaudio-dev \
    libfribidi-dev \
    libjack-dev \
    libsndio-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libxss-dev \
    libxtst-dev \
    libdrm-dev \
    libgbm-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libdbus-1-dev \
    libibus-1.0-dev \
    libudev-dev \
    libthai-dev \
    libusb-1.0-0-dev \
    zlib1g-dev \
    libfmt-dev \
    libxml2-dev \
    libopenexr-dev \
    libimath-dev \
    libglib2.0-dev \
    gdb \
    libgmock-dev \
    zenity \
    dotnet-sdk-10.0 \
    libshaderc-dev \
    spirv-tools \
    spirv-cross \
    libspirv-cross-c-shared-dev \
    libspirv-cross-c-shared0 \

# Ensure Autodesk FBX SDK dependency libxml2.so.2 is symlinked on modern Linux systems
if [ ! -f /usr/lib/x86_64-linux-gnu/libxml2.so.2 ] && [ -f /usr/lib/x86_64-linux-gnu/libxml2.so ]; then
    echo "Creating symlink for libxml2.so.2 (needed by Autodesk FBX SDK)..."
    sudo ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so /usr/lib/x86_64-linux-gnu/libxml2.so.2
fi
# Ensure git submodules are updated recursively
echo "Updating git submodules..."
git submodule update --init --recursive

# Run the python setup script to download dependencies & generate project makefiles
echo "Running project setup script..."
PAGER=cat python3 scripts/setup.py

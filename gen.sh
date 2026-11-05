#!/usr/bin/env bash
set -e
export DEBIAN_FRONTEND=noninteractive
export PAGER=cat

# Configure git safe.directory for containerized builds
git config --global --add safe.directory "*" 2>/dev/null || true

# Fallback for container environments where sudo may not be installed
if ! command -v sudo &> /dev/null; then
    sudo() { "$@"; }
fi

echo "=== Detecting Package Manager ==="

if command -v pacman &> /dev/null; then
    echo "[Ignite] Arch Linux / Garuda detected! Installing packages via pacman..."
    sudo pacman -S --needed --noconfirm \
        base-devel gcc clang lld cmake ninja pkgconf git make tar \
        python python-pip vulkan-headers vulkan-icd-loader \
        sdl3 wayland libxkbcommon libx11 libxext libxrandr libxcursor \
        libxfixes libxi libxss libxtst libdrm mesa alsa-lib \
        pulseaudio zlib fmt libxml2 openexr imath glib2 gdb zenity \
        dotnet-sdk shaderc spirv-tools spirv-cross rust
elif command -v apt-get &> /dev/null; then
    echo "[Ignite] Debian / Ubuntu detected! Installing packages via apt-get..."
    export DEBIAN_FRONTEND=noninteractive
    sudo apt-get update
    sudo apt-get install -y \
        build-essential g++-14 clang lld cmake ninja-build pkg-config \
        git make tar expect python3 python3-pip python3-venv \
        libvulkan-dev vulkan-tools mesa-vulkan-drivers libwayland-dev \
        libxkbcommon-dev xorg-dev libasound2-dev libpulse-dev \
        libaudio-dev libfribidi-dev libjack-dev libsndio-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
        libxfixes-dev libxi-dev libxss-dev libxtst-dev libdrm-dev \
        libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
        libdbus-1-dev libibus-1.0-dev libudev-dev libthai-dev \
        libusb-1.0-0-dev zlib1g-dev libfmt-dev libxml2-dev \
        libopenexr-dev libimath-dev libglib2.0-dev gdb libgmock-dev \
        zenity dotnet-sdk-10.0 libshaderc-dev spirv-tools \
        spirv-cross libspirv-cross-c-shared-dev libspirv-cross-c-shared0
else
    echo "[Ignite] Warning: Unknown package manager. Continuing with existing system packages..."
fi


# Ensure Clang is the default compiler for CMake and build tools
export CC=clang
export CXX=clang++
if [ -n "$GITHUB_ENV" ]; then
    echo "CC=clang" >> "$GITHUB_ENV"
    echo "CXX=clang++" >> "$GITHUB_ENV"
fi
if command -v update-alternatives &> /dev/null; then
    sudo update-alternatives --set cc /usr/bin/clang 2>/dev/null || sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100
    sudo update-alternatives --set c++ /usr/bin/clang++ 2>/dev/null || sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100
fi

# Ensure Autodesk FBX SDK dependency libxml2.so.2 is symlinked on modern Linux systems
if [ ! -f /usr/lib/x86_64-linux-gnu/libxml2.so.2 ] && [ -f /usr/lib/x86_64-linux-gnu/libxml2.so ]; then
    echo "Creating symlink for libxml2.so.2 (needed by Autodesk FBX SDK)..."
    sudo ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so /usr/lib/x86_64-linux-gnu/libxml2.so.2
elif [ ! -f /usr/lib/x86_64-linux-gnu/libxml2.so ] && [ -f /usr/lib/x86_64-linux-gnu/libxml2.so.2 ]; then
    echo "Creating symlink for libxml2.so (needed by Autodesk FBX SDK)..."
    sudo ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so.2 /usr/lib/x86_64-linux-gnu/libxml2.so
fi

# Ensure libnethost is symlinked into /usr/local/lib for C++ linking (-lnethost)
NETHOST_SO=$(find /usr/lib/dotnet /usr/share/dotnet -name 'libnethost.so' 2>/dev/null | head -1)
if [ -n "$NETHOST_SO" ]; then
    echo "Creating symlink for libnethost.so..."
    sudo ln -sf "$NETHOST_SO" /usr/local/lib/libnethost.so
fi
NETHOST_A=$(find /usr/lib/dotnet /usr/share/dotnet -name 'libnethost.a' 2>/dev/null | head -1)
if [ -n "$NETHOST_A" ]; then
    echo "Creating symlink for libnethost.a..."
    sudo ln -sf "$NETHOST_A" /usr/local/lib/libnethost.a
fi
sudo ldconfig

# Ensure FBX SDK directory structure is compatible
if [ -d /opt/fbxsdk ] && [ ! -d /opt/fbxsdk/2020.3.9 ]; then
    echo "Creating symlink for /opt/fbxsdk/2020.3.9..."
    sudo ln -sf /opt/fbxsdk /opt/fbxsdk/2020.3.9
fi
export FBX_SDK="${FBX_SDK:-/opt/fbxsdk}"

# Configure Vulkan SDK runtime libraries and environment
configure_vulkan_sdk() {
    if [ -z "$VULKAN_SDK" ] || [ ! -d "$VULKAN_SDK" ]; then
        if [ -d "/vulkan/1.4.350.0/x86_64" ]; then
            export VULKAN_SDK="/vulkan/1.4.350.0/x86_64"
        elif [ -d "$PWD/scripts/downloads/vulkansdk/1.4.341.1/x86_64" ]; then
            export VULKAN_SDK="$PWD/scripts/downloads/vulkansdk/1.4.341.1/x86_64"
        else
            local found_sdk=$(find "$PWD/scripts/downloads/vulkansdk" -maxdepth 2 -type d -name "x86_64" 2>/dev/null | head -1)
            if [ -n "$found_sdk" ] && [ -d "$found_sdk" ]; then
                export VULKAN_SDK="$found_sdk"
            fi
        fi
    fi

    if [ -n "$VULKAN_SDK" ] && [ -d "$VULKAN_SDK" ]; then
        echo "Configuring Vulkan SDK runtime libraries from $VULKAN_SDK..."
        if [ -f "$VULKAN_SDK/lib/libdxcompiler.so" ]; then
            sudo ln -sf "$VULKAN_SDK/lib/libdxcompiler.so" /usr/local/lib/libdxcompiler.so
        fi
        if [ -f "$VULKAN_SDK/lib/libshaderc_shared.so" ]; then
            sudo ln -sf "$VULKAN_SDK/lib/libshaderc_shared.so" /usr/local/lib/libshaderc_shared.so
        fi
        echo "$VULKAN_SDK/lib" | sudo tee /etc/ld.so.conf.d/vulkansdk.conf > /dev/null
        sudo ldconfig

        if [ -n "$GITHUB_ENV" ]; then
            echo "VULKAN_SDK=$VULKAN_SDK" >> "$GITHUB_ENV"
            echo "LD_LIBRARY_PATH=$VULKAN_SDK/lib:${LD_LIBRARY_PATH:-}" >> "$GITHUB_ENV"
        fi
    fi
}

configure_vulkan_sdk

# Ensure C# csc wrapper exists for Premake gmake builds
if [ ! -f /usr/local/bin/csc ]; then
    echo "Creating /usr/local/bin/csc wrapper for Premake C# builds..."
    sudo tee /usr/local/bin/csc > /dev/null << 'CSCEOF'
#!/bin/bash
CSPROJ=$(ls *.csproj 2>/dev/null | head -n 1)
if [ -n "$CSPROJ" ]; then
    CONFIG="Debug"
    for arg in "$@"; do
        case "$arg" in
            *Shipping-Profiling*|*shipping-profiling*) CONFIG="Release" ;;
            *Release-Profiling*|*release-profiling*) CONFIG="Release" ;;
            *Shipping*|*shipping*) CONFIG="Release" ;;
            *Release*|*release*) CONFIG="Release" ;;
            *Debug-Profiling*|*debug-profiling*) CONFIG="Debug" ;;
            *Debug*|*debug*) CONFIG="Debug" ;;
        esac
    done
    exec dotnet build "$CSPROJ" -c "$CONFIG" --nologo -v q
else
    CSC_DLL=$(find /usr/share/dotnet /usr/lib/dotnet -name "csc.dll" 2>/dev/null | head -1)
    if [ -n "$CSC_DLL" ]; then
        exec dotnet "$CSC_DLL" "$@"
    else
        echo "Error: csc compiler not found" >&2
        exit 1
    fi
fi
CSCEOF
    sudo chmod +x /usr/local/bin/csc
fi

# Ensure C# msbuild wrapper exists on Linux
if [ ! -f /usr/local/bin/msbuild ]; then
    echo "Creating /usr/local/bin/msbuild wrapper for .NET builds..."
    sudo tee /usr/local/bin/msbuild > /dev/null << 'MSBEOF'
#!/bin/bash
exec dotnet msbuild "$@"
MSBEOF
    sudo chmod +x /usr/local/bin/msbuild
fi

# Ensure git submodules are updated recursively
echo "Updating git submodules..."
git submodule update --init --recursive

# Fix workspace file ownership if previous runs used sudo
echo "Fixing file ownership in workspace..."
sudo chown -R $(id -u):$(id -g) .

# Run the python setup script to download dependencies & generate project makefiles
echo "Running project setup script..."
python3 scripts/setup.py

# Re-run Vulkan SDK configuration in case SDK was just downloaded by setup.py
configure_vulkan_sdk

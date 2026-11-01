# syntax=docker/dockerfile:1
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------------
# Base system utilities
# ---------------------------------------------------------------------------
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates wget curl gnupg xz-utils apt-transport-https \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Install Python 3 + pip
# ---------------------------------------------------------------------------
RUN apt-get update \
    && apt-get install -y --no-install-recommends python3 python3-pip python3-venv \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Microsoft package repository — dotnet SDK
# ---------------------------------------------------------------------------
RUN wget -qO /tmp/packages-microsoft-prod.deb \
        "https://packages.microsoft.com/config/ubuntu/24.04/packages-microsoft-prod.deb" \
    && dpkg -i /tmp/packages-microsoft-prod.deb \
    && rm /tmp/packages-microsoft-prod.deb \
    && apt-get update \
    && apt-get install -y dotnet-sdk-10.0 \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Core development packages  (includes `expect` for the FBX SDK installer)
# ---------------------------------------------------------------------------
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential clang lld cmake ninja-build pkg-config \
       git tar \
       libvulkan-dev vulkan-tools mesa-vulkan-drivers \
       libshaderc-dev spirv-tools spirv-cross \
       libspirv-cross-c-shared-dev libspirv-cross-c-shared0 \
       libwayland-dev libxkbcommon-dev xorg-dev \
       libasound2-dev libpulse-dev libaudio-dev libfribidi-dev libjack-dev libsndio-dev \
       libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
       libxi-dev libxss-dev libxtst-dev \
       libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev \
       libdbus-1-dev libibus-1.0-dev libudev-dev libthai-dev libusb-1.0-0-dev \
       zlib1g-dev libfmt-dev libxml2-dev \
       libopenexr-dev libimath-dev \
       libglib2.0-dev \
       gdb libgmock-dev zenity expect \
    && rm -rf /var/lib/apt/lists/*

# Symlink libxml2.so.2 for FBX SDK compatibility if needed
RUN if [ ! -f /usr/lib/x86_64-linux-gnu/libxml2.so.2 ] && [ -f /usr/lib/x86_64-linux-gnu/libxml2.so ]; then \
        ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so /usr/lib/x86_64-linux-gnu/libxml2.so.2; \
    elif [ ! -f /usr/lib/x86_64-linux-gnu/libxml2.so ] && [ -f /usr/lib/x86_64-linux-gnu/libxml2.so.2 ]; then \
        ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so.2 /usr/lib/x86_64-linux-gnu/libxml2.so; \
    fi

# ---------------------------------------------------------------------------
# VULKAN Linux docs ref https://vulkan.lunarg.com/doc/sdk/latest/linux/getting_started.html
# $VULKAN_SDK/bin = SDK executables
# $VULKAN_SDK/lib = Dynamically-loaded libraries (e.g, layers)
# ---------------------------------------------------------------------------

RUN wget -qO /tmp/vulkan_sdk.tar.xz \
        "https://sdk.lunarg.com/sdk/download/1.4.350.0/linux/vulkansdk-linux-x86_64-1.4.350.0.tar.xz" \
    && mkdir -p /vulkan \
    && tar -xf /tmp/vulkan_sdk.tar.xz -C /vulkan \
    && rm /tmp/vulkan_sdk.tar.xz

ENV VULKAN_SDK=/vulkan/1.4.350.0/x86_64
ENV PATH="$VULKAN_SDK/bin:$PATH"

# Configure Vulkan SDK library paths and symlink libvulkan loader
RUN if [ -d "$VULKAN_SDK/lib/VulkanLoader/lib" ]; then \
        ln -sf "$VULKAN_SDK/lib/VulkanLoader/lib/"libvulkan.so* "$VULKAN_SDK/lib/"; \
    fi \
    && ln -sf "$VULKAN_SDK/lib/"libvulkan.so* /usr/local/lib/ 2>/dev/null || true \
    && echo "$VULKAN_SDK/lib" > /etc/ld.so.conf.d/vulkan.conf \
    && ldconfig

ENV LD_LIBRARY_PATH="$VULKAN_SDK/lib:/usr/local/lib:$LD_LIBRARY_PATH"

# ---------------------------------------------------------------------------
# Install Rust toolchain (stable) via rustup
# ---------------------------------------------------------------------------
ENV RUSTUP_HOME=/opt/rustup \
    CARGO_HOME=/opt/cargo \
    PATH="/opt/cargo/bin:$PATH"

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable --no-modify-path \
    && chmod -R a+w /opt/rustup /opt/cargo

# ---------------------------------------------------------------------------
# Install Microsoft DirectX Compiler
# ---------------------------------------------------------------------------
# RUN wget -qO /tmp/linux_dxcompiler.tar.gz \
#         "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz" \
#     && tar -xzf /tmp/linux_dxcompiler.tar.gz -C "$VULKAN_SDK" \
#     && rm /tmp/linux_dxcompiler.tar.gz

# ---------------------------------------------------------------------------
# Python virtual environment (avoids PEP 668 issues)
# ---------------------------------------------------------------------------
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir requests

ENV PATH="/opt/venv/bin:$PATH"

# ---------------------------------------------------------------------------
# Symlink libnethost for Premake / C++ linking (-lnethost)
# dotnet-sdk-10.0 ships libnethost.{a,so} under /usr/share/dotnet
# ---------------------------------------------------------------------------
RUN set +e; \
    NETHOST_A=$(find /usr/lib/dotnet /usr/share/dotnet -name 'libnethost.a' 2>/dev/null | head -1); \
    [ -n "$NETHOST_A" ] && ln -sfv "$NETHOST_A" /usr/local/lib/libnethost.a; \
    NETHOST_SO=$(find /usr/lib/dotnet /usr/share/dotnet -name 'libnethost.so' 2>/dev/null | head -1); \
    [ -n "$NETHOST_SO" ] && ln -sfv "$NETHOST_SO" /usr/local/lib/libnethost.so; \
    set -e; \
    ldconfig

# ---------------------------------------------------------------------------
# Premake5 — Linux binary (system-wide, available before source is mounted)
# ---------------------------------------------------------------------------
RUN wget -qO /tmp/premake.tar.gz \
        "https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-linux.tar.gz" \
    && tar -xzf /tmp/premake.tar.gz -C /usr/local/bin premake5 \
    && chmod +x /usr/local/bin/premake5 \
    && rm /tmp/premake.tar.gz

# ---------------------------------------------------------------------------
# C# csc wrapper for Premake gmake Makefile builds
# ---------------------------------------------------------------------------
RUN cat <<'EOF' > /usr/local/bin/csc
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
EOF
RUN chmod +x /usr/local/bin/csc

# ---------------------------------------------------------------------------
# FBX SDK — GCC/Linux (installed at image-build time for layer caching)
#
# The Autodesk makeself installer reads from /dev/tty (not stdin), so
# piping printf/yes is completely ignored.  We use `expect` instead,
# which spawns a real pseudo-TTY and drives the two interactive prompts:
#   1. "Do you want to read the ReadMe file [y/n] ?"  -> n
#   2. "...agree to the terms..."                      -> yes
#
# BuildKit heredoc RUN (# syntax=docker/dockerfile:1) is used here so
# that the nested "cat > file << 'EXPECTEOF'" is evaluated by the shell
# rather than by Docker's Dockerfile parser (which caused "unknown
# instruction: &&" in classic line-continuation mode).
#
# The SDK lands at /opt/fbxsdk/2020.3.9/.
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# FBX SDK — GCC/Linux (installed at image-build time for layer caching)
# ---------------------------------------------------------------------------
ENV FBX_SDK=/opt/fbxsdk/2020.3.9

RUN <<'SHELL'
set -eux


# Create working directory
mkdir -p /tmp/fbxsdk

# Download the SDK installer archive (contains a makeself installer)
wget -qO /tmp/fbxsdk/fbxsdk.tar.gz \
    "https://damassets.autodesk.net/content/dam/autodesk/www/files/fbx202039_fbxsdk_gcc_linux.tar.gz"

# Extract the makeself installer
tar -xzf /tmp/fbxsdk/fbxsdk.tar.gz -C /tmp/fbxsdk

# Locate the installer executable (skip the README and tar.gz itself)
PKG=$(find /tmp/fbxsdk -type f ! -name "Install_FbxSdk.txt" ! -name "fbxsdk.tar.gz" | head -1)

echo "FBX SDK installer: $PKG"
chmod +x "$PKG"

# Ensure destination directory exists
mkdir -p /opt/fbxsdk/2020.3.9

# Write expect script to answer prompts automatically
cat > /tmp/fbxsdk_install.exp << 'EXPECTEOF'
#!/usr/bin/expect -f
set timeout 300
set installer [lindex $argv 0]
set dest [lindex $argv 1]

spawn $installer $dest

expect {
    -re {\[y/n\]} { send "n\r"; exp_continue }
    -re {(?i)(agree|yes/no)} { send "yes\r"; exp_continue }
    eof
}
EXPECTEOF

# Run the installer via expect (ignoring its non‑zero exit code)
expect /tmp/fbxsdk_install.exp "$PKG" /opt/fbxsdk/2020.3.9 || true

# Verify installation
if [ -f "${FBX_SDK}/include/fbxsdk.h" ]; then
  echo "FBX SDK installed successfully at ${FBX_SDK}"
else
  echo "ERROR: fbxsdk.h not found after installation"
  exit 1
fi

# Cleanup
rm -rf /tmp/fbxsdk /tmp/fbxsdk_install.exp
SHELL


# ---------------------------------------------------------------------------
# Working directory — source code is bind-mounted at runtime, not COPY'd.
#
# Build the dev image once:
#   docker build -t ignite-dev .
#
# Then attach with the source tree mounted:
#   docker run -it --rm -v "$(pwd):/workspace" ignite-dev
#
# Inside the container, generate makefiles and build:
#   python3 scripts/setup.py   # downloads nothing; FBX_SDK already set
#   make -j6 config=debug # or whatever make target you use
# ---------------------------------------------------------------------------
WORKDIR /workspace

CMD ["/bin/bash"]

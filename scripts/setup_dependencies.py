import os
import platform
import stat
import subprocess
from pathlib import Path

import utils


def install_vulkan_sdk(target_directory):
    """Install the Vulkan SDK. Windows and Linux support."""
    vulkan_version = "1.4.341.1"

    if platform.system() == "Windows":
        vulkan_sdk_link_windows = f"https://sdk.lunarg.com/sdk/download/{vulkan_version}/windows/vulkansdk-windows-X64-{vulkan_version}.exe"
        vulkan_executable_path = target_directory / f"vulkansdk-{vulkan_version}.exe"
        vulkan_install_root = Path(f"C:/VulkanSDK/{vulkan_version}")

        if not vulkan_install_root.exists() or not (vulkan_install_root / "Include").exists():
            # download installer if not exists
            if not vulkan_executable_path.exists():
                print(f"Downloading Vulkan SDK {vulkan_version}...")
                utils.download_file(vulkan_sdk_link_windows, str(vulkan_executable_path))

            # See https://vulkan.lunarg.com/doc/view/1.3.283.0/windows/getting_started.html for argument details
            # install
            print(f"Installing Vulkan SDK {vulkan_version} to {vulkan_install_root}...")
            # For modern LunarG installers (1.3.216+), use these flags for silent install
            install_args = [
                str(vulkan_executable_path),
                "--root", str(vulkan_install_root),
                "--accept-licenses",
                "--default-answer",
                "--confirm-command", "install",
            ]

            # Only install debug symbols if NOT in GitHub Actions
            if os.environ.get("GITHUB_ACTIONS") != "true":
                install_args.append("com.lunarg.vulkan.debug")
            else:
                print("Skipping Vulkan debug symbols on GitHub Actions")

            try:
                result = subprocess.run(install_args, check=True).returncode
                print(f"Vulkan SDK installation completed with exit code: {result}")
            except subprocess.CalledProcessError as e:
                print(f"Vulkan SDK installation failed with error: {e}")
                # Fallback to /S if it's an older installer style for some reason
                print("Attempting fallback installation with /S...")
                fallback_args = [str(vulkan_executable_path), "/S", f"/D={vulkan_install_root}"]
                subprocess.run(fallback_args)

            if not (vulkan_install_root / "Include").exists():
                print(f"Warning: Vulkan SDK Include folder not found at {vulkan_install_root / 'Include'}")
                # Check for lowercase 'include'
                if (vulkan_install_root / "include").exists():
                    print("Found lowercase 'include' folder, updating path...")
                else:
                    print("Critical: Installation seems incomplete or failed to create Include directory.")

            utils.set_env_variable("VULKAN_SDK", str(vulkan_install_root))
            os.environ["VULKAN_SDK"] = str(vulkan_install_root)

            return True
        else:
            # get existing from VULKAN_SDK
            existing_vulkan_sdk = utils.get_env_variable('VULKAN_SDK')

            if existing_vulkan_sdk is None:
                existing_vulkan_sdk = str(vulkan_install_root)

            if not existing_vulkan_sdk is None:
                print(f"Vulkan SDK already installed at: {existing_vulkan_sdk}")
                if existing_vulkan_sdk:
                    utils.set_env_variable("VULKAN_SDK", str(existing_vulkan_sdk))
                    os.environ["VULKAN_SDK"] = str(existing_vulkan_sdk)
                return True
            else:
                return False

    elif platform.system() == "Linux":
        existing_vulkan_sdk = os.environ.get("VULKAN_SDK") or utils.get_env_variable("VULKAN_SDK")
        if existing_vulkan_sdk and ":" in existing_vulkan_sdk[:3]:
            # Convert Windows path (e.g. C:/VulkanSDK/...) to WSL mount path
            drive = existing_vulkan_sdk[0].lower()
            existing_vulkan_sdk = f"/mnt/{drive}{existing_vulkan_sdk[existing_vulkan_sdk.index(':')+1:]}"

        def is_valid_linux_vulkan_sdk(path):
            if not path:
                return False
            p = Path(path)
            return (p / "include" / "dxc" / "dxcapi.h").exists() or (p / "Include" / "dxc" / "dxcapi.h").exists()

        if is_valid_linux_vulkan_sdk(existing_vulkan_sdk):
            print(f"Vulkan SDK (Linux) already configured at: {existing_vulkan_sdk}")
            utils.set_env_variable("VULKAN_SDK", str(existing_vulkan_sdk))
            os.environ["VULKAN_SDK"] = str(existing_vulkan_sdk)
            return True

        # Check default paths
        default_linux_paths = [
            Path("/vulkan") / vulkan_version / "x86_64",
            target_directory / "vulkansdk" / vulkan_version / "x86_64"
        ]
        for path in default_linux_paths:
            if is_valid_linux_vulkan_sdk(path):
                print(f"Vulkan SDK (Linux) found at: {path}")
                utils.set_env_variable("VULKAN_SDK", str(path))
                os.environ["VULKAN_SDK"] = str(path)
                return True

        # Download Vulkan SDK for Linux
        print(f"Downloading Vulkan SDK {vulkan_version} for Linux...")
        vulkan_sdk_link_linux = f"https://sdk.lunarg.com/sdk/download/{vulkan_version}/linux/vulkansdk-linux-x86_64-{vulkan_version}.tar.xz"
        archive_path = target_directory / f"vulkansdk-linux-{vulkan_version}.tar.xz"

        if not archive_path.exists():
            utils.download_file(vulkan_sdk_link_linux, str(archive_path))

        print("Extracting Vulkan SDK for Linux...")
        import tarfile
        vulkan_extract_dir = target_directory / "vulkansdk"
        vulkan_extract_dir.mkdir(parents=True, exist_ok=True)
        with tarfile.open(str(archive_path)) as tf:
            tf.extractall(str(vulkan_extract_dir))

        archive_path.unlink()

        detected_path = vulkan_extract_dir / vulkan_version / "x86_64"
        if is_valid_linux_vulkan_sdk(detected_path):
            utils.set_env_variable("VULKAN_SDK", str(detected_path))
            os.environ["VULKAN_SDK"] = str(detected_path)
            print(f"Vulkan SDK (Linux) installed at: {detected_path}")
            return True
        else:
            print("Error: Could not verify Linux Vulkan SDK installation.")
            return False


def install_premake5(target_directory):
    import shutil

    # On Linux the Docker image already installs premake5 to /usr/local/bin.
    # Reuse that binary instead of downloading a second copy.
    if platform.system() == "Linux":
        system_premake = shutil.which("premake5")
        if system_premake:
            print(f"Premake5 found in PATH: {system_premake}")
            return Path(system_premake)

    premake_version = "5.0.0-beta8"
    premake_link_windows = f"https://github.com/premake/premake-core/releases/download/v{premake_version}/premake-{premake_version}-windows.zip"
    premake_link_linux = f"https://github.com/premake/premake-core/releases/download/v{premake_version}/premake-{premake_version}-linux.tar.gz"
    premake_archive_path = target_directory / ("premake.zip" if platform.system() == "Windows" else "premake.tar.gz")
    premake_download_url = premake_link_windows if platform.system() == "Windows" else premake_link_linux

    premake_binary = target_directory / ("premake5.exe" if platform.system() == "Windows" else "premake5")
    if not premake_binary.exists():
        if not premake_archive_path.exists():
            utils.download_file(premake_download_url, str(premake_archive_path))
        utils.extract_archive(str(premake_archive_path), delete_after_extraction=True)

    if not premake_binary.exists():
        raise FileNotFoundError(f"Premake executable not found at {premake_binary}")

    # Ensure the binary is executable on Linux
    if platform.system() == "Linux":
        premake_binary.chmod(premake_binary.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    print("\nPremake Installed\n")
    return premake_binary


# ---------------------------------------------------------------------------
# FBX SDK — Windows
# ---------------------------------------------------------------------------

def install_fbx_sdk(target_directory):
    """Dispatch FBX SDK installation to the correct platform handler."""
    if platform.system() == "Windows":
        return _install_fbx_sdk_windows(target_directory)
    elif platform.system() == "Linux":
        return install_fbx_sdk_linux(target_directory)
    else:
        print(f"FBX SDK auto-install is not supported on {platform.system()}")
        return None


def _install_fbx_sdk_windows(target_directory):
    fbx_sdk_version = "fbx202037"

    target_directory = Path(target_directory)
    target_directory.mkdir(parents=True, exist_ok=True)

    def is_valid_fbx_root(path: str | Path | None):
        if not path:
            return False
        root = Path(path)
        return (root / "include" / "fbxsdk.h").exists() and (root / "lib" / "x64").exists()

    def find_existing_fbx_root():
        env_path = utils.get_env_variable("FBX_SDK") or utils.get_user_env_variable("FBX_SDK")
        if is_valid_fbx_root(env_path):
            return Path(env_path)

        candidates = [
            Path("C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7"),
            Path("C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.1"),
            Path("C:/Program Files/Autodesk/FBX/FBX SDK"),
        ]

        for candidate in candidates:
            if is_valid_fbx_root(candidate):
                return candidate

            if candidate.exists() and candidate.is_dir():
                for child in candidate.iterdir():
                    if child.is_dir() and is_valid_fbx_root(child):
                        return child

        return None

    existing_path = find_existing_fbx_root()
    if existing_path is not None:
        utils.set_env_variable("FBX_SDK", str(existing_path))
        os.environ["FBX_SDK"] = str(existing_path)
        print(f"FBX SDK Installed: {existing_path}")
        return existing_path

    fbx_link_windows = f"https://damassets.autodesk.net/content/dam/autodesk/www/files/{fbx_sdk_version}_fbxsdk_vs2022_win.exe"
    fbx_binary_path = target_directory / f"{fbx_sdk_version}_fbxsdk_vs2022_win.exe"

    if not fbx_binary_path.exists():
        utils.download_file(fbx_link_windows, str(fbx_binary_path))

    install_root = Path("C:/Program Files/Autodesk/FBX/FBX SDK")
    install_attempts = [
        [str(fbx_binary_path), "/S", f"/D={install_root}"],
        [str(fbx_binary_path), "/VERYSILENT", "/NORESTART", f"/DIR={install_root}"],
        [str(fbx_binary_path)],
    ]

    for args in install_attempts:
        try:
            mode_desc = ' '.join(args[1:]) if len(args) > 1 else 'interactive mode'
            print(f"Installing FBX SDK using: \"{mode_desc}\"")
            print("Please wait...", end="\n\n")
            result = subprocess.call(args)
            print(f"FBX SDK installer exited with code: {result}")
        except OSError as exc:
            print(f"FBX installer launch failed with args {args[1:]}: {exc}")
            continue

        detected_path = find_existing_fbx_root()
        if detected_path is not None:
            utils.set_env_variable("FBX_SDK", str(detected_path))
            os.environ["FBX_SDK"] = str(detected_path)
            print(f"FBX SDK Installed: {detected_path}")
            return detected_path

    raise RuntimeError(
        "FBX SDK installation could not be verified. "
        "Please install manually and set FBX_SDK to the SDK root (contains include/fbxsdk.h and lib/x64)."
    )


# ---------------------------------------------------------------------------
# FBX SDK — Linux (GCC tarball from Autodesk)
# ---------------------------------------------------------------------------

def install_fbx_sdk_linux(target_directory):
    """
    Download and install the Autodesk FBX SDK for Linux (GCC build).

    The Autodesk tarball (fbx202039_fbxsdk_gcc_linux.tar.gz) contains a
    makeself-based installer script (.pkg.1).  We pipe 'yes' to it and pass
    the destination directory as a positional argument.

    Expected post-install layout:
        /opt/fbxsdk/2020.3.9/
            include/fbxsdk.h
            lib/gcc/x64/debug/libfbxsdk.so
            lib/gcc/x64/release/libfbxsdk.so
    """
    fbx_version_tag  = "fbx202039"
    fbx_version_name = "2020.3.9"
    fbx_url = (
        f"https://damassets.autodesk.net/content/dam/autodesk/www/files/"
        f"{fbx_version_tag}_fbxsdk_gcc_linux.tar.gz"
    )

    target_directory = Path(target_directory)
    target_directory.mkdir(parents=True, exist_ok=True)

    install_base = Path("/opt/fbxsdk")
    install_root = install_base / fbx_version_name

    # ---------- helpers ----------

    def is_valid_fbx_root(path):
        if not path:
            return False
        p = Path(path)
        return (p / "include" / "fbxsdk.h").exists()

    def detect_existing():
        # 1. Environment variable
        env_path = os.environ.get("FBX_SDK") or utils.get_env_variable("FBX_SDK")
        if is_valid_fbx_root(env_path):
            return Path(env_path)
        # 2. Default install location
        for candidate in [install_root, install_base]:
            if is_valid_fbx_root(candidate):
                return candidate
            if candidate.is_dir():
                for child in sorted(candidate.iterdir(), reverse=True):
                    if child.is_dir() and is_valid_fbx_root(child):
                        return child
        return None

    # ---------- check if already installed ----------

    existing = detect_existing()
    if existing is not None:
        utils.set_env_variable("FBX_SDK", str(existing))
        os.environ["FBX_SDK"] = str(existing)
        print(f"FBX SDK (Linux) already installed at: {existing}")
        return existing

    # ---------- download ----------

    archive_path = target_directory / f"{fbx_version_tag}_fbxsdk_gcc_linux.tar.gz"
    if not archive_path.exists():
        print(f"Downloading FBX SDK for Linux ({fbx_version_tag})...")
        utils.download_file(fbx_url, str(archive_path))

    # ---------- extract installer ----------

    extract_dir = target_directory / "fbxsdk_linux_extract"
    extract_dir.mkdir(parents=True, exist_ok=True)

    print("Extracting FBX SDK installer archive...")
    import tarfile
    with tarfile.open(str(archive_path)) as tf:
        tf.extractall(str(extract_dir))

    # ---------- locate the installer ----------
    # The FBX SDK tarball contains two files:
    #   Install_FbxSdk.txt  — README (not the installer)
    #   <installer>         — makeself installer (extension varies by version)
    #
    # We CANNOT rely on the extension (e.g. .pkg.1) because:
    #   - Path.suffix only returns the LAST extension, so ".pkg.1" → suffix ".1"
    #   - Some versions ship the installer without an extension at all
    # Strategy: exclude known non-installer files and pick whatever remains.

    KNOWN_NON_INSTALLERS = {"Install_FbxSdk.txt", archive_path.name}

    installer = None
    all_files = [f for f in extract_dir.rglob("*") if f.is_file()]

    for f in sorted(all_files):
        if f.name not in KNOWN_NON_INSTALLERS and not f.name.endswith((".tar.gz", ".gz")):
            installer = f
            break

    if installer is None:
        contents = [str(p) for p in all_files]
        raise FileNotFoundError(
            f"Could not locate an installer inside {extract_dir}.\n"
            f"Archive contents: {contents}"
        )

    print(f"Found FBX SDK installer: {installer.name}")


    # Make installer executable
    installer.chmod(installer.stat().st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    # ---------- run installer ----------

    install_base.mkdir(parents=True, exist_ok=True)

    print(f"Installing FBX SDK (Linux) to {install_base} ...")
    # The FBX SDK makeself installer expects:
    #   - First positional arg: destination directory
    #   - ReadMe prompt answered with "n", license prompt answered with "yes"
    expect_script_path = target_directory / "fbxsdk_install.exp"
    expect_content = r"""#!/usr/bin/expect -f
set timeout 300
set installer [lindex $argv 0]
set dest [lindex $argv 1]

spawn $installer $dest

expect {
    -re {\[y/n\]} { send "n\r"; exp_continue }
    -re {(?i)(agree|yes/no)} { send "yes\r"; exp_continue }
    eof
}
"""
    with open(expect_script_path, "w") as f:
        f.write(expect_content)

    cmd = f'expect "{expect_script_path}" "{installer}" "{install_base}"'
    result = subprocess.run(cmd, shell=True, check=False)
    print(f"FBX SDK installer exited with code {result.returncode}")

    try:
        expect_script_path.unlink()
    except Exception:
        pass

    # ---------- verify ----------

    detected = detect_existing()
    if detected is None:
        raise RuntimeError(
            "FBX SDK installation could not be verified on Linux. "
            "Please install manually and set FBX_SDK to the SDK root "
            "(directory must contain include/fbxsdk.h and lib/gcc/x64/)."
        )

    utils.set_env_variable("FBX_SDK", str(detected))
    os.environ["FBX_SDK"] = str(detected)
    print(f"FBX SDK (Linux) installed at: {detected}")
    return detected


def build_sdl3():
    """Build the SDL3 submodule using CMake."""
    print("Building SDL3 from source using CMake...")
    root_dir = Path(__file__).resolve().parent.parent
    sdl_dir = root_dir / "thirdparty" / "SDL3"
    build_dir = sdl_dir / "build"
    
    configure_args = [
        "cmake",
        "-S", str(sdl_dir),
        "-B", str(build_dir),
        "-DSDL_SHARED=ON",
        "-DSDL_STATIC=OFF",
        "-DSDL_TESTS=OFF",
        "-DSDL_TEST_LIBRARY=OFF",
        "-DCMAKE_BUILD_TYPE=Release"
    ]
    
    build_args = [
        "cmake",
        "--build", str(build_dir),
        "--config", "Release"
    ]
    
    try:
        subprocess.run(configure_args, check=True)
        subprocess.run(build_args, check=True)
        print("SDL3 build completed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"SDL3 build failed: {e}")
        raise e

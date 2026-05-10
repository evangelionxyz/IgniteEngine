import os
import platform
import subprocess
from pathlib import Path

import utils

def install_vulkan_sdk(target_directory):
    vulkan_version = "1.4.341.1"
    vulkan_sdk_link_windows = f"https://sdk.lunarg.com/sdk/download/{vulkan_version}/windows/vulkansdk-windows-X64-{vulkan_version}.exe"
    vulkan_executable_path = target_directory / f"vulkansdk-{vulkan_version}.exe"
    vulkan_install_root = Path(f"C:/VulkanSDK/{vulkan_version}")

    if platform.system() == "Windows":
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
                    # We might need to handle this in premake scripts too, 
                    # but let's see if we can just use the path as is.
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
            else: return False


def install_premake5(target_directory):
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
    
    print("\nPremake Installed\n")
    return premake_binary


def install_fbx_sdk(target_directory):
    fbx_sdk_version = "fbx202037"

    if platform.system() != "Windows":
        print("FBX SDK auto-install is currently supported on Windows only")
        return None

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
            print(f"Installing FBX SDK using: {mode_desc}")
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

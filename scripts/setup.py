import os
import platform
import sys
import ctypes
import subprocess
from pathlib import Path

import utils

ROOT_DIR = Path(__file__).resolve().parent.parent
SCRIPTS_DIR = ROOT_DIR / "scripts"
DOWNLOADS_DIR = SCRIPTS_DIR / "downloads"

def ensure_admin():
    if os.name != "nt":
        return

    try:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin()
    except AttributeError:
        return

    if not is_admin:
        script = os.path.abspath(sys.argv[0])
        params = " ".join([f'"{arg}"' for arg in sys.argv[1:]])
        ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, f'"{script}" {params}'.strip(), None, 1)
        sys.exit()

def run():
    ensure_admin()

    system_name = platform.system()
    premake_version = "5.0.0-beta7"
    premake_link_windows = f"https://github.com/premake/premake-core/releases/download/v{premake_version}/premake-{premake_version}-windows.zip"
    premake_link_linux = f"https://github.com/premake/premake-core/releases/download/v{premake_version}/premake-{premake_version}-linux.tar.gz"
    premake_archive_path = DOWNLOADS_DIR / ("premake.zip" if system_name == "Windows" else "premake.tar.gz")
    premake_download_url = premake_link_windows if system_name == "Windows" else premake_link_linux

    vulkan_version = "1.4.328.1"
    vulkan_sdk_link_windows = f"https://sdk.lunarg.com/sdk/download/{vulkan_version}/windows/vulkansdk-windows-X64-{vulkan_version}.exe"
    vulkan_executable_path = DOWNLOADS_DIR / f"vulkansdk-{vulkan_version}.exe"

    # check vulkan sdk
    if system_name == "Windows":
        if utils.get_env_variable("VULKAN_SDK") is None:
            if not vulkan_executable_path.exists():
                utils.download_file(vulkan_sdk_link_windows, str(vulkan_executable_path))

            subprocess.call([str(vulkan_executable_path)])
            print("Re-run this script after installing Vulkan SDK")
        else:
            print("Vulkan SDK Installed")

    # download premake and extract
    print("Running premake5...")
    premake_binary = DOWNLOADS_DIR / ("premake5.exe" if system_name == "Windows" else "premake5")
    if not premake_binary.exists():
        if not premake_archive_path.exists():
            utils.download_file(premake_download_url, str(premake_archive_path))
        utils.extract_archive(str(premake_archive_path), delete_after_extraction=True)

    if not premake_binary.exists():
        raise FileNotFoundError(f"Premake executable not found at {premake_binary}")

    premake_args = [str(premake_binary), "--file=scripts/premake5.lua"]
    if system_name == "Windows":
        premake_args.append("vs2022")
    else:
        premake_args.append("gmake2")

    subprocess.call(premake_args, cwd=ROOT_DIR)

if __name__ == "__main__":
    run()

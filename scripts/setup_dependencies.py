import platform
import subprocess

import utils

def install_vulkan_sdk(target_directory):
    vulkan_version = "1.4.341.1"
    vulkan_sdk_link_windows = f"https://sdk.lunarg.com/sdk/download/{vulkan_version}/windows/vulkansdk-windows-X64-{vulkan_version}.exe"
    vulkan_executable_path = target_directory / f"vulkansdk-{vulkan_version}.exe"

    if platform.system() == "Windows":
        if utils.get_env_variable("VULKAN_SDK") is None:
            if not vulkan_executable_path.exists():
                utils.download_file(vulkan_sdk_link_windows, str(vulkan_executable_path))

            subprocess.call([str(vulkan_executable_path)])
            print("Re-run this script after installing Vulkan SDK")
        else:
            print("Vulkan SDK Installed")


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
    
    return premake_binary
    

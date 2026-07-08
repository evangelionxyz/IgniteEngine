import os
import platform
import sys
import ctypes
import subprocess
from pathlib import Path
import setup_dependencies as dp

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


def generate_project_files(premake_binary):
    # Generate project files using premake5 for the current platform.
    if platform.system() == "Windows":
        premake_scripts = ["premake5.lua", "premake5-managed.lua"]
    else:
        premake_scripts = ["premake5.lua"]

    for script in premake_scripts:
        premake_args = [str(premake_binary), f"--file=scripts/{script}"]
        if platform.system() == "Windows":
            premake_args.append("vs2026")
        else:
            # gmake is the modern GNU Makefile generator in premake5
            premake_args.append("gmake")
            premake_args.append("--cc=clang")
        subprocess.call(premake_args, cwd=ROOT_DIR)


def run():
    ensure_admin()

    DOWNLOADS_DIR.mkdir(parents=True, exist_ok=True)

    # Install Vulkan SDK (on Linux we download and extract it to get the DXC compiler)
    dp.install_vulkan_sdk(DOWNLOADS_DIR)

    # On Linux inside the dev container the FBX SDK is pre-installed by the
    # Dockerfile and FBX_SDK env var is already set correctly.  Skip the
    # download/install if the env var already points to a valid SDK root.
    fbx_sdk_env = os.environ.get("FBX_SDK", "")
    fbx_sdk_preinstalled = (
        platform.system() == "Linux"
        and fbx_sdk_env
        and (Path(fbx_sdk_env) / "include" / "fbxsdk.h").exists()
    )

    if fbx_sdk_preinstalled:
        print(f"FBX SDK already available at: {fbx_sdk_env} (skipping download)")
    else:
        dp.install_fbx_sdk(DOWNLOADS_DIR)

    premake_binary = dp.install_premake5(DOWNLOADS_DIR)

    # Always generate project files regardless of whether deps were freshly installed.
    print("\nGenerating project files...")
    generate_project_files(premake_binary)

    if os.environ.get("GITHUB_ACTIONS") != "true":
        input("Press any key to continue")


if __name__ == "__main__":
    run()

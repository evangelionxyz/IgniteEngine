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


def run():
    ensure_admin()

    dp.install_vulkan_sdk(DOWNLOADS_DIR)
    premake_binary = dp.install_premake5(DOWNLOADS_DIR)

    # Generate Solution for premake native and managed
    premake_scripts = ["premake5.lua", "premake5-managed.lua"]
    for script in premake_scripts:
        premake_args = [str(premake_binary), f"--file=scripts/{script}"]
        if platform.system() == "Windows":
            premake_args.append("vs2026")
        else:
            premake_args.append("gmake")
            premake_args.append("--cc=clang")
        premake_args.append("pause")
        subprocess.call(premake_args, cwd=ROOT_DIR)

    input("Press any key to continue")
    
if __name__ == "__main__":
    run()

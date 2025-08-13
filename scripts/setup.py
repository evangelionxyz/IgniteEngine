import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


def run(cmd: List[str], cwd: Optional[Path] = None) -> int:
    print(f":: Running: {' '.join(cmd)}")
    return subprocess.call(cmd, cwd=str(cwd) if cwd else None)


def cmake_exists() -> bool:
    try:
        out = subprocess.check_output(["cmake", "--version"], stderr=subprocess.STDOUT)
        print(out.decode(errors="ignore").splitlines()[0])
        return True
    except (OSError, subprocess.CalledProcessError):
        return False


def detect_default_generator() -> tuple[str, list[str]]:
    system = platform.system()
    if system == "Windows":
        # Prefer Visual Studio 2022 on Windows
        return "Visual Studio 17 2022", ["-A", "x64"]
    # On Linux/macOS prefer Ninja Multi-Config when available, else fall back
    if shutil.which("ninja"):
        return "Ninja Multi-Config", []
    return ("Unix Makefiles" if system != "Darwin" else "Xcode"), []


def is_multi_config(generator: str) -> bool:
    g = generator.lower()
    return any(k in g for k in ["visual studio", "multi-config", "xcode"])


def main(argv: list[str]) -> int:
    repo_root = Path(__file__).resolve().parents[1]
    default_build = repo_root / "build"

    parser = argparse.ArgumentParser(description="Configure CMake build for Ignite")
    parser.add_argument("--build-dir", default=str(default_build), help="Build directory (default: ./build)")
    parser.add_argument("--generator", "-G", default=None, help="CMake generator to use")
    parser.add_argument("--config", "-c", default="Debug", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"], help="Configuration (used for single-config generators at configure time)")
    parser.add_argument("--with-build", action="store_true", help="Also run 'cmake --build' after configure")
    parser.add_argument("--clean", action="store_true", help="Delete the build directory before configuring")
    parser.add_argument("--extra", nargs=argparse.REMAINDER, help="Extra arguments passed to cmake after '--'")

    args = parser.parse_args(argv)

    if not cmake_exists():
        print("Error: CMake is not installed or not found in PATH.")
        print("Please install CMake and try again.")
        return 2

    src_dir = repo_root
    build_dir = Path(args.build_dir).resolve()

    if args.clean and build_dir.exists():
        print(f":: Removing build directory: {build_dir}")
        shutil.rmtree(build_dir)

    build_dir.mkdir(parents=True, exist_ok=True)

    gen, gen_args = detect_default_generator()
    if args.generator:
        gen = args.generator
        gen_args = []

    cmake_cmd = [
        "cmake",
        "-S",
        str(src_dir),
        "-B",
        str(build_dir),
        "-G",
        gen,
    ] + gen_args

    # Favor compile commands export when supported
    if not is_multi_config(gen):
        cmake_cmd += ["-D", f"CMAKE_BUILD_TYPE={args.config}"]
        cmake_cmd += ["-D", "CMAKE_EXPORT_COMPILE_COMMANDS=ON"]
    else:
        # For multi-config generators, some support exporting compile commands via toolchains only.
        pass

    if args.extra:
        # Allow passing extra CMake cache entries after a '--'
        # Example: -- -DOPTION=ON -DOTHER=OFF
        cmake_cmd += [x for x in args.extra if x != "--"]

    code = run(cmake_cmd)
    if code != 0:
        return code

    if args.with_build:
        build_cmd = [
            "cmake",
            "--build",
            str(build_dir),
        ]
        # For multi-config generators, build the chosen config
        if is_multi_config(gen):
            build_cmd += ["--config", args.config]
        return run(build_cmd)

    print(":: CMake configuration completed.")
    print(f"   Generator: {gen}")
    print(f"   Build dir: {build_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

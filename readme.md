# Ignite Engine

Work in progress C++ Game Engine

[![Build Windows](https://github.com/evangelionxyz/IgniteEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/evangelionxyz/IgniteEngine/actions/workflows/ci.yml)
[![GitHub Stars](https://img.shields.io/github/stars/evangelionxyz/IgniteEngine?style=flat&label=stars)](https://github.com/evangelionxyz/IgniteEngine/stargazers)

## Installation

### Clone Repository Recursively

We actively develop on the `dev` branch. You can use `-b dev` when cloning the repository. <br>
```bash
git clone --recursively https://github.com/evangelionxyz/IgniteEngine.git -b master
```

## Windows Build

Run bootstrap to download dependencies and generate Visual Studio project files
```
gen.bat
```

Run premake to generate Visual Studio project files
```
premake5 vs2026 --file=scripts/premake5.lua
```

Restore and Build with MSBuild
```
msbuild IGN.slnx /t:Restore /p:Configuration=Debug /p:Platform=x64
msbuild IGN.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
```

## Linux Build

Run bootstrap to download dependencies and generate makefiles
```
gen.linux.sh
```

Restore and Build with Make
```
make
```


## Docker build

```bash
-------------------------------------------------------------
1. Build the image once (downloads FBX SDK, installs everything)
command: docker build -t ignite-dev .
-------------------------------------------------------------

-------------------------------------------------------------
2. Attach with source mounted
command: docker run -it --rm -v "${PWD}:/workspace" ignite-dev
-------------------------------------------------------------

-------------------------------------------------------------
3. Inside the container — generate makefiles and build
command: python3 scripts/setup.py
-------------------------------------------------------------

-------------------------------------------------------------
4. FBX_SDK already set, premake5 in PATH → instant
now, lets build.
-------------------------------------------------------------

-------------------------------------------------------------
5. Build
   5.1. This is C++ Project Build
        command: make -j6 config=debug Ignite.Editor

    5.2. We also need to build the C# Project
         command: make -j6 config=debug Ignite.ScriptEngine
```

## Preview

<div style='display:flex;flex-direction:column;width:80%;margin:auto; gap:12px'>
  <img src="ignite/editor/resources/examples/image_09.png">
  <img src="ignite/editor/resources/examples/image_08.png">
  <img src="ignite/editor/resources/examples/image_01.png">
  <img src="ignite/editor/resources/examples/image_05.png">
  <img src="ignite/editor/resources/examples/image_06.png">
  <img src="ignite/editor/resources/examples/image_04.png">
  <img src="ignite/editor/resources/examples/image_03.png">
</div>

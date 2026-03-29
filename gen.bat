@echo off

rem ensure python requests module is available
python -c "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('requests') else 1)" >nul 2>&1
if errorlevel 1 (
	python -m pip install requests
) else (
	echo Python module 'requests' already installed.
)

rem ensure submodules are ready
git submodule update --init --recursive

rem keep pip up to date
python -m pip install --upgrade pip

rem push directory to scripts dir
rem and running setup.py scripts
pushd %~dp0\scripts
python setup.py
popd
pause
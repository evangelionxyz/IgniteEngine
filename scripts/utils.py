import os
import requests
import urllib
import tarfile
from zipfile import ZipFile
import platform
import time

if platform.system() == "Windows":
    import winreg

def set_env_variable(variable_name, directory_path):
    if platform.system() == "Windows":
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment", 0, winreg.KEY_SET_VALUE)
            winreg.SetValueEx(key, variable_name, 0, winreg.REG_EXPAND_SZ, directory_path)
            winreg.CloseKey(key)
        except Exception as e:
            print(f"Warning: Could not set system environment variable {variable_name}: {e}")
            
    elif platform.system() == "Linux":
        os.environ[variable_name] = directory_path
        with open('/etc/environment', 'a') as file:
            file.write(f'\n{variable_name}="{directory_path}"\n')

    # Persist for GitHub Actions
    if "GITHUB_ENV" in os.environ:
        with open(os.environ["GITHUB_ENV"], "a") as f:
            f.write(f"{variable_name}={directory_path}\n")
            print(f"Added {variable_name} to GITHUB_ENV")


def get_env_variable(name):
    if platform.system() == "Windows":
        try:
            with winreg.OpenKey(
                winreg.HKEY_LOCAL_MACHINE,
                r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment",
                0,
                winreg.KEY_READ,
            ) as key:
                return winreg.QueryValueEx(key, name)[0]
        except FileNotFoundError:
            return None
        except OSError:
            return None
    elif platform.system() == "Linux":
        value = os.environ.get(name)
        if value is not None:
            return value
        try:
            with open('/etc/environment', 'r') as file:
                for line in file:
                    if line.startswith(f'{name}='):
                        return line.split('=', 1)[1].strip().strip('"')
        except FileNotFoundError:
            return None
    return None


def get_user_env_variable(name):
    if platform.system() == "Windows":
        key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Environment")
        try:
            return winreg.QueryValueEx(key, name)[0]
        except:
            return None
    elif platform.system() == "Linux":
        return os.environ.get(name)
    return None


def set_system_path_variable(new_path):
    if platform.system() == "Windows":
        current_path = os.environ.get('PATH', '')
        if new_path not in current_path:
            updated_path = f'{current_path};{new_path}'
            set_env_variable("Path", updated_path)
            
            # Update current process environment for immediate use in same step
            os.environ['PATH'] = updated_path
            
            # Persist for GitHub Actions
            if "GITHUB_PATH" in os.environ:
                with open(os.environ["GITHUB_PATH"], "a") as f:
                    f.write(f"{new_path}\n")
                    print(f"Added {new_path} to GITHUB_PATH")
            return True
    if platform.system() == "Linux":
        current_path = os.environ.get('PATH', '')
        if new_path not in current_path.split(':'):
            updated_path = f'{current_path}:{new_path}'
            os.environ['PATH'] = updated_path
            with open('/etc/environment', 'a') as file:
                file.write(f'\nPATH="{updated_path}"\n')
            
            # Persist for GitHub Actions
            if "GITHUB_PATH" in os.environ:
                with open(os.environ["GITHUB_PATH"], "a") as f:
                    f.write(f"{new_path}\n")
            return True
    return False


def download_file(url, filepath):
    filepath = os.path.abspath(filepath)
    os.makedirs(os.path.dirname(filepath), exist_ok=True)

    if (type(url) is list):
        for url_option in url:
            try:
                download_file(url_option, filepath)
                return
            except urllib.error.URLError as e:
                print(f"URL Error encountered: {e.reason}. Proceeding with backup...\n\n")
                if os.path.exists(filepath):
                    os.remove(filepath)
            except urllib.error.HTTPError as e:
                print(f"HTTP Error  encountered: {e.code}. Proceeding with backup...\n\n")
                if os.path.exists(filepath):
                    os.remove(filepath)
            except requests.exceptions.RequestException as e:
                print(f"Request error encountered: {e}. Proceeding with backup...\n\n")
                if os.path.exists(filepath):
                    os.remove(filepath)
            except Exception:
                print(f"Something went wrong. Proceeding with backup...\n\n")
                if os.path.exists(filepath):
                    os.remove(filepath)
        raise ValueError(f"Failed to download {filepath}")
    
    if (not(type(url) is str)):
        raise ValueError(f"Argument 'url' must be type of list or string")
    
    with open(filepath, 'wb') as f:
        headers = {'User-Agent': "Mozilla/5.0 (Macintosh Intel Mac Os X 10_15_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/83.0.4103.97 Safari/537.36"}
        with requests.get(url, headers=headers, stream=True) as response:
            response.raise_for_status()
            total = response.headers.get('content-length')
            total_bytes = int(total) if total is not None else None
            chunk_size = max(int(total_bytes / 1000), 1024 * 1024) if total_bytes else 1024 * 1024
            downloaded = 0
            start_time = time.time()
            has_output = False

            for data in response.iter_content(chunk_size=chunk_size):
                if not data:
                    continue

                f.write(data)
                downloaded += len(data)

                elapsed = time.time() - start_time
                elapsed = elapsed if elapsed > 0 else 1e-6
                speed = downloaded / elapsed

                if total_bytes:
                    percent = (downloaded / total_bytes) * 100
                    status_line = (
                        f"\rDownloading {os.path.basename(filepath)}: "
                        f"{percent:6.2f}% | {downloaded / (1024 * 1024):.2f} MB / "
                        f"{total_bytes / (1024 * 1024):.2f} MB | {speed / 1024:.2f} KB/s"
                    )
                else:
                    status_line = (
                        f"\rDownloading {os.path.basename(filepath)}: "
                        f"{downloaded / (1024 * 1024):.2f} MB | {speed / 1024:.2f} KB/s"
                    )

                print(status_line, end='', flush=True)
                has_output = True

            if has_output:
                print()

def extract_archive(filepath, delete_after_extraction=True):
    arch_filepath = os.path.abspath(filepath)
    arch_file_location = os.path.dirname(arch_filepath)

    if platform.system() == "Windows":
        zip_file = dict()
        with ZipFile(arch_filepath, 'r') as zip_file_folder:
            for name in zip_file_folder.namelist():
                zip_file[name] = zip_file_folder.getinfo(name).file_size

            for zipped_filename, _ in zip_file.items():
                unzipped_filepath = os.path.abspath(f"{arch_file_location}/{zipped_filename}")
                os.makedirs(os.path.dirname(unzipped_filepath), exist_ok=True)

                # Check if the file already exists
                if not os.path.isfile(unzipped_filepath):
                    zip_file_folder.extract(zipped_filename, path = arch_file_location, pwd = None)

    elif platform.system() == "Linux":
        file = tarfile.open(arch_filepath)
        file.extractall(arch_file_location)
        file.close()

    if delete_after_extraction:
        os.remove(arch_filepath)
        print(f"Deleted archive file: {arch_filepath}")
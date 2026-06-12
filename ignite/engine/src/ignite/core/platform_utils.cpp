/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "platform_utils.hpp"

#include "application.hpp"
#include "ignite/graphics/window.hpp"

#ifdef PLATFORM_WINDOWS
    #include <Windows.h>
    #include <ShObjIdl.h>
    #include <commdlg.h>
    #include <objbase.h> // for CoCreateGuid
#elif PLATFORM_LINUX
    #include <iostream>
    #include <memory>
    #include <stdexcept>
    #include <array>
    #include <unistd.h>
#endif

#include <filesystem>

namespace ignite
{
    // --- Executable helpers ---
#ifdef PLATFORM_WINDOWS
    ignite::Path GetExecutablePath()
    {
        char buffer[MAX_PATH] = { 0 };
        DWORD size = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (size == 0 || size == MAX_PATH)
            return ignite::Path(std::filesystem::current_path().string());
        return ignite::Path(std::string(buffer, buffer + size));
    }
#elif defined(PLATFORM_LINUX)
    ignite::Path GetExecutablePath()
    {
        std::array<char, 1024> buffer;
        ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
        if (len == -1)
            return ignite::Path(std::filesystem::current_path().string());
        buffer[len] = '\0';
        return ignite::Path(buffer.data());
    }
#else
    ignite::Path GetExecutablePath()
    {
        return ignite::Path(std::filesystem::current_path().string());
    }
#endif

    ignite::Path GetExecutableDirectory()
    {
        auto exe = GetExecutablePath();
        if (exe.empty())
            return ignite::Path(std::filesystem::current_path().string());
        return exe.parent_path();
    }

#ifdef PLATFORM_WINDOWS

    std::vector<std::string> FileDialogs::OpenFiles(const char *filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[8192] = { 0 };
        CHAR currentDir[256] = { 0 };
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = Application::GetInstance()->GetWindow()->GetNativeWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);

        if (GetCurrentDirectoryA(256, currentDir))
        {
            ofn.lpstrInitialDir = currentDir;
        }

        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

        std::vector<std::string> files;

        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            char *ptr = ofn.lpstrFile;

            std::string directory = ptr;
            ptr += directory.size() + 1;

            if (*ptr == '\0')
            {
                // Only one file was selected
                files.push_back(directory);
            }
            else
            {
                // Multiple files selected
                while (*ptr)
                {
                    std::string filename = ptr;
                    ptr += filename.size() + 1;
                    files.push_back(directory + "\\" + filename);
                }
            }
        }

        return files;
    }

    std::string FileDialogs::OpenFile(const char *filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = { 0 };
        CHAR currentDir[256] = { 0 };
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = Application::GetInstance()->GetWindow()->GetNativeWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        if (GetCurrentDirectoryA(256, currentDir))
        {
            ofn.lpstrInitialDir = currentDir;
        }

        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE)
            return ofn.lpstrFile;

        return {};
    }

    std::string FileDialogs::SelectFolder()
    {
        std::string folderPath;

        IFileDialog *pFileDialog = nullptr;

        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));

        if (SUCCEEDED(hr))
        {
            // Set the dialog to pick folders
            DWORD options;
            pFileDialog->GetOptions(&options);
            pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

            // show the dialg
            hr = pFileDialog->Show(nullptr);
            if (SUCCEEDED(hr))
            {
                IShellItem *pItem;
                hr = pFileDialog->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    if (SUCCEEDED(hr))
                    {
                        // convert wide string to std::string
                        char path[MAX_PATH];
                        WideCharToMultiByte(CP_ACP, 0, pszFilePath, -1, path, MAX_PATH, nullptr, nullptr);
                        folderPath = path;
                        CoTaskMemFree(pszFilePath);
                    }

                    pItem->Release();
                }
            }
            pFileDialog->Release();
        }

        return folderPath;
    }

    std::string FileDialogs::SaveFile(const char *filter)
    {
        OPENFILENAMEA ofn;
        CHAR szFile[260] = { 0 };
        CHAR currentDir[256] = { 0 };
        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = Application::GetInstance()->GetWindow()->GetNativeWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        if (GetCurrentDirectoryA(256, currentDir))
            ofn.lpstrInitialDir = currentDir;
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        // Sets the default extension by extracting it from the filter
        ofn.lpstrDefExt = strchr(filter, '\0') + 1;

        if (GetSaveFileNameA(&ofn) == TRUE)
            return ofn.lpstrFile;

        return {};
    }

#elif PLATFORM_LINUX
    std::string ExecCommand(const char *cmd)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe)
        {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }

        if (!result.empty() && result[result.length() - 1] == '\n')
        {
            result.erase(result.length() - 1);
        }
        return result;
    }

    std::string FileDialogs::OpenFile(const char *filter)
    {
        std::string command = "zenity --file-selection";
        if (filter)
        {
            command += " --file-filter=\"";
            command += filter;
            command += "\"";
        }
        std::string result = ExecCommand(command.c_str());
        if (result.empty())
        {
            return {};
        }
        return result;
    }

    std::string FileDialogs::SaveFile(const char *filter)
    {
        std::string command = "zenity --file-selection --save";
        if (filter)
        {
            command += " --file-filter=\"";
            command += filter;
            command += "\"";
        }
        std::string result = ExecCommand(command.c_str());
        if (result.empty())
        {
            return {};
        }
        return result;
    }
#endif
}

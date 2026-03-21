/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
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

#pragma once

#include "ignite/core/types.hpp"
#include "Host.h"

#include <string>
#include <memory>
#include <filesystem>
#include <unordered_map>

namespace ignite
{
    // Method signature IDs for Ignite scripting
    enum class ScriptMethodSignature : int
    {
        Void = 0,
        Void_Float = 1,
        Void_UInt64 = 2,
        Bool_Type = 3,
        Void_UInt64_Type = 4,
        UInt64_String = 5,
        UInt64_UInt64_Vec3 = 6,
        Void_UInt64_Bool = 8,
        Void_UInt64_OutBool = 9,
        Void_UInt64_OutVec3 = 10,
        Void_UInt64_Vec3 = 11,
        Void_UInt64_OutQuat = 12,
        Void_UInt64_Quat = 13,
        Object_UInt64 = 14,
    };

    // Wrapper around MochiSharp's DotNetHost for Ignite scripting
    class ScriptHost
    {
    public:
        ScriptHost();
        ~ScriptHost();

        // Initialize the .NET runtime with the specified config
        bool Init(const std::filesystem::path &configPath);

        // Load a .NET assembly (core or app)
        bool LoadAssembly(const std::filesystem::path &assemblyPath);

        // Register method signatures for script methods
        void RegisterSignatures();

        // Initialize C# InternalCalls bridge with native callbacks
        bool InitializeInternalCalls();

        // Create a script instance with a GUID
        bool CreateInstance(const std::string &guid, const std::string &typeName);

        // Destroy a script instance
        void DestroyInstance(const std::string &guid);

        // Bind an instance method and return a method handle
        int BindInstanceMethod(const std::string &guid, const std::string &methodName, ScriptMethodSignature signature);

        // Bind a static method and return a method handle
        int BindStaticMethod(const std::string &typeName, const std::string &methodName, ScriptMethodSignature signature);

        // Invoke a method with arguments
        bool Invoke(int methodId, const void *argsPtr, int argCount, void *returnPtr);

        // Get all non-abstract classes derived from baseType in an assembly
        std::string GetDerivedTypes(const std::filesystem::path &assemblyPath, const std::string &baseType);

        // Check if initialized
        bool IsInitialized() const { return m_Initialized; }

        // Get the underlying MochiSharp host
        MochiSharp::DotNetHost *GetHost() { return m_Host.get(); }

    private:
        std::unique_ptr<MochiSharp::DotNetHost> m_Host;
        bool m_Initialized = false;
        std::filesystem::path m_BaseDir;
        std::unordered_map<std::string, int> m_InstanceMap; // GUID -> instance ID
    };
}

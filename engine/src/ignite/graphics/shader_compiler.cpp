#include "shader_compiler.hpp"
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

#include "shader_compiler.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
	const uint32_t SPIRV_SPACES_NUM = 8;

	DataOutputContext::DataOutputContext(const char* file, bool textMode)
	{
		stream = fopen(file, textMode ? "w" : "wb");
		LOG_ASSERT("Cannot open file '{}' for writing", file);
	}

	DataOutputContext::~DataOutputContext()
	{
		if (stream)
		{
			fclose(stream);
			stream = nullptr;
		}
	}

	bool DataOutputContext::WriteDataAsText(const void* data, size_t size)
	{
		for (size_t i = 0; i < size; i++)
		{
			uint8_t value = ((const uint8_t*)data)[i];

			if (m_lineLength > 128)
			{
				fprintf(stream, "\n    ");
				m_lineLength = 0;
			}

			fprintf(stream, "%u,", value);

			if (value < 10)
				m_lineLength += 3;
			else if (value < 100)
				m_lineLength += 4;
			else
				m_lineLength += 5;
		}

		return true;
	}

	void DataOutputContext::WriteTextPreamble(const char* shaderName, const std::string& combinedDefines)
	{
		fprintf(stream, "// {%s}\n", combinedDefines.c_str());
		fprintf(stream, "const uint8_t %s[] = {", shaderName);
	}

	void DataOutputContext::WriteTextEpilog()
	{
		fprintf(stream, "\n};\n");
	}

	bool DataOutputContext::WriteDataAsBinary(const void* data, size_t size)
	{
		if (size == 0)
			return true;

		return fwrite(data, size, 1, stream) == 1;
	}

	// For use as a callback in "WriteFileHeader" and "WritePermutation" functions
	bool DataOutputContext::WriteDataAsTextCallback(const void* data, size_t size, void* context)
	{
		return ((DataOutputContext*)context)->WriteDataAsText(data, size);
	}

	bool DataOutputContext::WriteDataAsBinaryCallback(const void* data, size_t size, void* context)
	{
		return ((DataOutputContext*)context)->WriteDataAsBinary(data, size);
	}

	Ref<DXCInstance> ShaderCompiler::CreateDXCCompiler()
	{
		Ref<DXCInstance> instance = CreateRef<DXCInstance>();
		HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&instance->compiler));
		LOG_ASSERT(SUCCEEDED(hr), "Failed to create IDxcCompiler3 instance. HRESULT {} {}", hr, std::system_category().message(hr).c_str());
		if (FAILED(hr)) return nullptr;

		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&instance->utils));
		LOG_ASSERT(SUCCEEDED(hr), "Failed to create IDxcUtils instance. HRESULT {} {}", hr, std::system_category().message(hr).c_str());
		if (FAILED(hr)) return nullptr;

		return instance;
	}

	std::vector<uint8_t> ShaderCompiler::CompileDXC(Ref<DXCInstance> instance, const CompilerOptions& options)
	{
		using namespace Microsoft::WRL;

		static const wchar_t* dxcOptimizationLevelRemap[] =
		{
			// Note: if you're getting errors like "error C2065: 'DXC_ARG_SKIP_OPTIMIZATIONS': undeclared identifier" here,
			// please update the Windows SDK to at least version 10.0.20348.0.
			DXC_ARG_SKIP_OPTIMIZATIONS,
			DXC_ARG_OPTIMIZATION_LEVEL1,
			DXC_ARG_OPTIMIZATION_LEVEL2,
			DXC_ARG_OPTIMIZATION_LEVEL3,
		};

		// Gather SPIRV register shifts once
		static const wchar_t* dxcRegShiftArgs[] =
		{
			L"-fvk-t-shift",
			L"-fvk-s-shift",
			L"-fvk-b-shift",
			L"-fvk-u-shift",
		};

		std::vector<std::wstring> regShifts;
		for (uint32_t reg = 0; reg < 4; reg++)
		{
			for (uint32_t space = 0; space < SPIRV_SPACES_NUM; space++)
			{
				wchar_t buf[64];
				regShifts.push_back(dxcRegShiftArgs[reg]);

				swprintf(buf, std::size(buf), L"%u", (&options.tRegShift)[reg]);
				regShifts.push_back(std::wstring(buf));

				swprintf(buf, std::size(buf), L"%u", space);
				regShifts.push_back(std::wstring(buf));
			}
		}

		// Compile shader
		std::wstring wsourceFile = options.filepath.wstring();
		std::vector<uint8_t> resultCode;
		
		ComPtr<IDxcBlobEncoding> sourceBlob;
		HRESULT hr = instance->utils->LoadFile(wsourceFile.c_str(), nullptr, &sourceBlob);
		if (SUCCEEDED(hr))
		{
			std::vector<std::wstring> args;
			args.reserve(16 + (options.defines.size()
				+ options.defines.size()
				+ options.includeDirectories.size()) * 2
				+ (options.platformType == ShaderPlatformType::SPIRV ? regShifts.size()
				+ options.spirvExtensions.size() : 0));

			
			args.push_back(wsourceFile); // Source file
			args.push_back(L"-T"); // Profile
			args.push_back(AnsiToWide(ShaderTypeToProfile(options.shaderDesc.shaderType) + "_" + options.shaderDesc.shaderModel));
			args.push_back(L"-E"); // Entry Point
			args.push_back(AnsiToWide(options.shaderDesc.entryPoint));

			// Defines
			for (const std::string& define : options.defines)
			{
				args.push_back(L"-D");
				args.push_back(AnsiToWide(define));
			}

			// Include directories
			for (const std::filesystem::path& path : options.includeDirectories)
			{
				args.push_back(L"-I");
				args.push_back(path.wstring());
			}

			// Arguments
			args.push_back(dxcOptimizationLevelRemap[static_cast<uint32_t>(options.shaderDesc.optLevel)]);

			uint32_t shaderModelIndex = (options.shaderDesc.shaderModel[0] - '0') * 10 + (options.shaderDesc.shaderModel[2] - '0');
			if (shaderModelIndex >= 62)
				args.push_back(L"-enable-16bit-types");

			if (options.warningsAreErrors)
				args.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);

			if (options.allResourcesBound)
				args.push_back(DXC_ARG_ALL_RESOURCES_BOUND);

			if (options.matrixRowMajor)
				args.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

			if (options.hlsl2021)
			{
				args.push_back(L"-HV");
				args.push_back(L"2021");
			}

			if (options.embedPdb)
			{
				args.push_back(L"-Qembed_debug");
			}

			if (options.platformType == ShaderPlatformType::SPIRV)
			{
				args.push_back(L"-spirv");
				args.push_back(std::wstring(L"-fspv-target-env=vulkan") + AnsiToWide(options.shaderDesc.vulkanVersion));

				if (!options.shaderDesc.vulkanMemoryLayout.empty())
				{
					args.push_back(std::wstring(L"-fvk-use-") + AnsiToWide(options.shaderDesc.vulkanMemoryLayout) + std::wstring(L"-layout"));
				}

				for (const std::string& ext : options.spirvExtensions)
				{
					args.push_back(std::wstring(L"-fspv-extension=") + AnsiToWide(ext));
				}

				for (const std::wstring& arg : regShifts)
				{
					args.push_back(arg);
				}
			}
			else // Not supported by SPIRV Gen
			{
				if (options.stripReflection)
				{
					args.push_back(L"-Qstrip_reflect");
				}
			}

			for (std::string const& opts : options.compilerOptions)
			{
				TokenizeCompilerOptions(opts.c_str(), args);
			}

			// Debug output
			if (options.verbose)
			{
				std::wstringstream cmd;
				for (const std::wstring& arg : args)
				{
					cmd << arg;
					cmd << L" ";
				}

				// LOG_TRACE("{}", cmd.str().c_str());
			}

			// Now that args are finalized, get their C-string pointer into vector
			std::vector<const wchar_t*> argPointers;
			argPointers.reserve(args.size());
			for (const std::wstring& arg : args)
			{
				argPointers.push_back(arg.c_str());
			}

			// Compile the shader
			DxcBuffer sourceBuffer = {};
			sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
			sourceBuffer.Size = sourceBlob->GetBufferSize();

			ComPtr<IDxcIncludeHandler> pDefaultIncludeHandler;
			instance->utils->CreateDefaultIncludeHandler(&pDefaultIncludeHandler);

			ComPtr<IDxcBlob> shaderBlob;
			ComPtr<IDxcBlobEncoding> errorBlob;
			ComPtr<IDxcResult> dxcResult;
			hr = instance->compiler->Compile(&sourceBuffer, argPointers.data(), (uint32_t)argPointers.size(), pDefaultIncludeHandler.Get(), IID_PPV_ARGS(&dxcResult));

			if (SUCCEEDED(hr))
			{
				dxcResult->GetStatus(&hr);
			}

			if (dxcResult)
			{
				dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
				dxcResult->GetErrorBuffer(&errorBlob);
			}

			bool isSucceeded = SUCCEEDED(hr) && shaderBlob;

			if (errorBlob && errorBlob->GetBufferPointer() && errorBlob->GetBufferSize() > 0)
			{
				std::string compilerOutput(
					reinterpret_cast<const char*>(errorBlob->GetBufferPointer()),
					errorBlob->GetBufferSize());

				if (!compilerOutput.empty())
				{
					LOG_ERROR("[ShaderCompiler][DXC] Compiler output for '{}':\n{}", options.filepath.generic_string(), compilerOutput);
				}
			}

			if (!isSucceeded)
			{
				std::string cmdLine;
				cmdLine.reserve(args.size() * 16);
				for (const std::wstring& arg : args)
				{
					cmdLine += std::string(arg.begin(), arg.end());
					cmdLine += " ";
				}

				LOG_ERROR("[ShaderCompiler][DXC] Failed to compile shader '{}' (entry='{}', profile='{}_{}', platform='{}').",
					options.filepath.generic_string(),
					options.shaderDesc.entryPoint,
					ShaderTypeToProfile(options.shaderDesc.shaderType),
					options.shaderDesc.shaderModel,
					ShaderPlatformToString(options.platformType));
				LOG_ASSERT(false, "[ShaderCompiler][DXC] Full command line : {}", cmdLine);
			}

			// Dump PDB
			if (isSucceeded && options.pdb)
			{
				ComPtr<IDxcBlob> pdb;
				ComPtr<IDxcBlobUtf16> pdbName;
				if (SUCCEEDED(dxcResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbName)))
				{
					std::wstring file = options.filepath.parent_path().wstring() + L"/" + L"PDB" + L"/" + std::wstring(pdbName->GetStringPointer());
					FILE* fp = _wfopen(file.c_str(), L"wb");
					if (fp)
					{
						fwrite(pdb->GetBufferPointer(), pdb->GetBufferSize(), 1, fp);
						fclose(fp);
					}
				}
			}

			// Dump output
			if (isSucceeded)
			{
				std::string outputExtension = ShaderPlatformExtension(options.platformType);
				std::filesystem::path parentPath = options.filepath.parent_path();
				if (!options.outputFilepath.empty())
				{
					parentPath = options.outputFilepath;
				}

				std::filesystem::path filename = parentPath / options.filepath.filename().replace_extension(outputExtension);

				size_t bufferSize = shaderBlob->GetBufferSize();
				const void* bufferPtr = shaderBlob->GetBufferPointer();
				resultCode.resize(bufferSize);
				std::memcpy(resultCode.data(), bufferPtr, bufferSize);

				DumpShader(options, resultCode, filename.generic_string());
			}
		}

		return resultCode;
	}

	void ShaderCompiler::DumpShader(const CompilerOptions& options, std::vector<uint8_t>& shaderCode, const std::string& outputPath)
	{
		if (options.binary || options.binaryBlob || (options.headerBlob))
		{
			DataOutputContext context(outputPath.c_str(), false);
			if (!context.stream)
			{
				return;
			}

			context.WriteDataAsBinary(shaderCode.data(), shaderCode.size());
			LOG_WARN("[WRITING TO BINARY] {}: {}", ShaderPlatformToString(options.platformType), outputPath);
		}

		if (options.header || options.headerBlob)
		{
			std::string headerOutput = outputPath + ".h"; // .h extension
		
			DataOutputContext context(headerOutput.c_str(), true);
			if (!context.stream)
				return;

			std::string shaderName = options.filepath.filename().generic_string();

			context.WriteTextPreamble(shaderName.c_str(), options.shaderDesc.combinedDefines);
			context.WriteDataAsText(shaderCode.data(), shaderCode.size());
			context.WriteTextEpilog();

			LOG_WARN("[WRITING TO BINARY] {}: {}", ShaderPlatformToString(options.platformType), headerOutput);
		}
	}


}
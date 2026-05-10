// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SHADER_COMPILER_HPP
#define SHADER_COMPILERH_PP

#pragma once

#include <cstdint>
#include <string>
#include "ignite/core/path.hpp"
#include <vector>
#include <unordered_map>

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "ignite/core/types.hpp"

#ifdef PLATFORM_WINDOWS
	#include <d3dcompiler.h>
	#include <d3dcommon.h>
	#include <combaseapi.h>
	#include <wrl/client.h>
	#include <dxcapi.h>
#endif

namespace ignite
{
	enum class ShaderType
	{
		Vertex,
		Pixel,
		Geometry,
		Compute,
		Tessellation,
	};

	static std::string ShaderTypeToProfile(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::Vertex:
			return "vs";
		case ShaderType::Pixel:
			return "ps";
		case ShaderType::Geometry:
			return "gs";
		case ShaderType::Compute:
			return "cs";
		case ShaderType::Tessellation:
			return "ts";
		default:
			return "invalid";
		}
	}

	enum class ShaderPlatformType
	{
		DXBC, DXIL, SPIRV
	};

	enum class CompilerType
	{
		DXC, FXC, SLANG
	};

	enum class OptimizationLevel
	{
		LEVEL_0 = 0,
		LEVEL_1 = 1,
		LEVEL_2 = 2,
		LEVEL_3 = 3,
	};

	static std::string ShaderPlatformToString(ShaderPlatformType type)
	{
		switch (type)
		{
			case ShaderPlatformType::DXIL: return "DXIL";
			case ShaderPlatformType::DXBC: return "DXBC";
			case ShaderPlatformType::SPIRV: return "SPIRV";
			default: return "Unknown";
		}
	}

	static std::string ShaderPlatformExtension(ShaderPlatformType type)
	{
		switch (type)
		{
		case ShaderPlatformType::DXIL: return ".dxil";
		case ShaderPlatformType::DXBC: return ".dxbc";
		case ShaderPlatformType::SPIRV: return ".spirv";
		default: return "Unknown";
		}
	}

	static std::string CompilerExecutablePath(CompilerType type)
	{
		switch (type)
		{
#ifdef PLATFORM_WINDOWS
		case CompilerType::DXC: return "dxc.exe";
		case CompilerType::FXC: return "fxc.exe";
		case CompilerType::SLANG: return "slangc.exe";
#else PLATFORM_LINUX
		case CompilerTyoe::DXC: return "dxc";
		case CompilerTyoe::FXC: return "fxc";
		case CompilerTyoe::SLANG: return "slangc";
#endif
		default: return "Unknown";
		}
	}

#ifdef _WIN32
	static void TokenizeDefineStrings(std::vector<std::string>& in, std::vector<D3D_SHADER_MACRO>& out)
	{
		if (in.empty())
			return;

		out.reserve(out.size() + in.size());
		for (const std::string& defineString : in)
		{
			D3D_SHADER_MACRO& define = out.emplace_back();
			char* s = (char*)defineString.c_str(); // IMPORTANT: "defineString" gets split into tokens divided by '\0'
			define.Name = strtok(s, "=");
			define.Definition = strtok(nullptr, "=");
		}
	}

	// Parses a string with command line options into a vector of wstring, one wstring per option.
	// Options are separated by spaces and may be quoted with "double quotes".
	// Backslash (\) means the next character is inserted literally into the output.
	static void TokenizeCompilerOptions(const char* in, std::vector<std::wstring>& out)
	{
		std::wstring current;
		bool quotes = false;
		bool escape = false;
		const char* ptr = in;
		while (char ch = *ptr++)
		{
			if (escape)
			{
				current.push_back(wchar_t(ch));
				escape = false;
				continue;
			}

			if (ch == ' ' && !quotes)
			{
				if (!current.empty())
					out.push_back(current);
				current.clear();
			}
			else if (ch == '\\')
			{
				escape = true;
			}
			else if (ch == '"')
			{
				quotes = !quotes;
			}
			else
			{
				current.push_back(wchar_t(ch));
			}
		}

		if (!current.empty())
		{
			out.push_back(current);
		}
	}
#endif

	static uint32_t HashToUint(size_t hash)
	{ 
		return uint32_t(hash) ^ (uint32_t(hash >> 32));
	}

	static std::wstring AnsiToWide(const std::string& s)
	{
		return std::wstring(s.begin(), s.end());
	}

	static bool IsSpace(char ch) 
	{ 
		return strchr(" \t\r\n", ch) != nullptr; 
	}

	static bool HasRepeatingSpace(char a, char b)
	{
		return (a == b) && a == ' ';
	}

#if PLATFORM_WINDOWS
	struct DXCInstance
	{
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
		Microsoft::WRL::ComPtr<IDxcUtils> utils;
	};
#endif

	struct ShaderDesc
	{
		std::string entryPoint = "main";
		std::string shaderModel = "6_5";
		std::string vulkanVersion = "1.3";
		std::string vulkanMemoryLayout;
		std::string combinedDefines;
		ShaderType shaderType;
		OptimizationLevel optLevel = OptimizationLevel::LEVEL_3;
	};

	struct CompilerOptions
	{
		CompilerType compilerType;
		ShaderPlatformType platformType;
		ignite::Path filepath;
		ignite::Path outputFilepath;

		void AddDefine(const std::string& define) { defines.push_back(define); }
		void AddSPIRVExtension(const std::string& ext) { spirvExtensions.push_back(ext); }
		void AddCompilerOptions(const std::string& opt) { compilerOptions.push_back(opt); }

		std::vector<ignite::Path> includeDirectories;
		std::vector<ignite::Path> relaxedIncludes;
		std::vector<std::string> spirvExtensions = { "SPV_EXT_descriptor_indexing", "KHR" };
		std::vector<std::string> compilerOptions;
		std::vector<std::string> defines;

		uint32_t tRegShift = 0; // must be first (or change "DxcCompile" code)
		uint32_t sRegShift = 128;
		uint32_t bRegShift = 256;
		uint32_t uRegShift = 384;

		ShaderDesc shaderDesc;

		bool serial = false;
		bool flatten = false;
		bool help = false;
		bool binary = true;
		bool header = false;
		bool binaryBlob = true;
		bool headerBlob = false;
		bool continueOnError = false;
		bool warningsAreErrors = false;
		bool allResourcesBound = false;
		bool pdb = false;
		bool embedPdb = false;
		bool stripReflection = false;
		bool matrixRowMajor = false;
		bool hlsl2021 = false;
		bool verbose = false;
		bool colorize = true;
		bool useAPI = false;
		bool slangHlsl = false;
		bool noRegShifts = false;
		int retryCount = 10; // default 10 retries for compilation task sub-process failures
	};

	class DataOutputContext
	{
	public:
		FILE* stream = nullptr;

		DataOutputContext(const char* file, bool textMode);
		~DataOutputContext();
		bool WriteDataAsText(const void* data, size_t size);
		void WriteTextPreamble(const char* shaderName, const std::string& combinedDefines);
		void WriteTextEpilog();
		bool WriteDataAsBinary(const void* data, size_t size);
		static bool WriteDataAsTextCallback(const void* data, size_t size, void* context);
		static bool WriteDataAsBinaryCallback(const void* data, size_t size, void* context);

	private:
		uint32_t m_lineLength = 129;
	};

	class ShaderCompiler
	{
	public:
		static Ref<DXCInstance> CreateDXCCompiler();
		static std::vector<uint8_t> CompileDXC(Ref<DXCInstance> instance, const CompilerOptions &options);
		static void DumpShader(const CompilerOptions &options, std::vector<uint8_t> &shaderCode, const std::string &outputPath);
	};
}

#endif
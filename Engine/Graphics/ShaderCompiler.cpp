#include "Graphics/ShaderCompiler.h"
#include "Core/Profiling.h"
#include "Core/Logger.h"
#include "Core/StringUtils.h"
#include "Memory/MemoryTracker.h"
#include <d3d12shader.h>
#include <unordered_map>
#include <fstream>
#include <string_view>
#include <span>

namespace koi
{
	namespace
	{
		static const std::unordered_map<ShaderType, std::wstring_view> s_shaderProfile
		{
			{ ShaderType::Vertex  , L"vs_6_6" },
			{ ShaderType::Hull    , L"hs_6_6" },
			{ ShaderType::Domain  , L"ds_6_6" },
			{ ShaderType::Geometry, L"gs_6_6" },
			{ ShaderType::Pixel   , L"ps_6_6" },
			{ ShaderType::Compute , L"cs_6_6" },
		};

		static const std::unordered_map<ShaderType, std::wstring_view> s_entryPoint
		{
			{ ShaderType::Vertex  , L"VSMain" },
			{ ShaderType::Hull    , L"HSMain" },
			{ ShaderType::Domain  , L"DSMain" },
			{ ShaderType::Geometry, L"GSMain" },
			{ ShaderType::Pixel   , L"PSMain" },
			{ ShaderType::Compute , L"CSMain" },
		};
	}

	ShaderCompiler& ShaderCompiler::Get()
	{
		static ShaderCompiler instance;
		return instance;
	}

	ComPtr<IDxcBlob> ShaderCompiler::CompileFromFile(const fs::path& filePath, const Args& args)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		KOI_LOG_INFO("Trying to compile shader from file: {}", filePath.string());

		assert(args.Type != ShaderType::Unknown && "Unknown shader type");
		if (args.Type == ShaderType::Unknown) return nullptr;

		ComPtr<IDxcBlobEncoding> sourceBlob;
		DX_CHECK(m_dxcUtils->LoadFile(filePath.wstring().c_str(), nullptr, &sourceBlob));

		assert(sourceBlob && "Failed to load shader file");

		DxcBuffer source
		{
			.Ptr      = sourceBlob->GetBufferPointer(),
			.Size     = sourceBlob->GetBufferSize(),
			.Encoding = DXC_CP_ACP,
		};

		const std::wstring shaderName = filePath.stem().wstring();

		const wchar_t* defaultArgs[] =
		{
			DXC_ARG_WARNINGS_ARE_ERRORS,
			DXC_ARG_ALL_RESOURCES_BOUND,
#if KOI_DEBUG
			L"-D", L"KOI_DEBUG=1",
			DXC_ARG_DEBUG,    // -Zi: emit a separate PDB output (extracted below)
#else
			L"-D", L"KOI_DEBUG=0",
			DXC_ARG_OPTIMIZATION_LEVEL3,
#endif
			L"-Qstrip_reflect"  // Strip reflection into a separate blob
		};
		std::span argsSpan(defaultArgs);

		// Build args
		ComPtr<IDxcCompilerArgs> compilerArgs;
		DX_CHECK(m_dxcUtils->BuildArguments(
			shaderName.c_str(),
			s_entryPoint.at(args.Type).data(),
			s_shaderProfile.at(args.Type).data(),
			argsSpan.data(),
			static_cast<u32>(argsSpan.size()),
			args.Defines.data(),
			static_cast<u32>(args.Defines.size()),
			&compilerArgs));

		// Compile
		ComPtr<IDxcResult> results;
		DX_CHECK(m_dxcCompiler->Compile(
			&source,
			compilerArgs->GetArguments(),
			compilerArgs->GetCount(),
			m_dxcIncludeHandler.Get(),
			IID_PPV_ARGS(&results)));

		// Print errors or warnings
		ComPtr<IDxcBlobUtf8> errors;
		DX_CHECK(results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
		if (errors && errors->GetStringLength() != 0)
		{
			const std::string msg(static_cast<const char*>(errors->GetBufferPointer()), errors->GetStringLength());
			KOI_LOG_ERROR("Failed to compile shader from file: {}\n\tError: {}", filePath.string(), msg);
			return nullptr;
		}

		auto writeFile = [](const fs::path& path, const ComPtr<IDxcBlob>& blob) -> bool
		{
			if (!fs::exists(path.parent_path()))
			{
				fs::create_directories(path.parent_path());
			}

			std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);

			if (out.is_open())
			{
				out.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
				out.close();
				return true;
			}
			out.close();
			return false;
		};

		ComPtr<IDxcBlob> shader;
		DX_CHECK(results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr));
		if (shader && args.OutDir)
		{
			// Save CSO to file
			{
				fs::path outPath = args.OutDir.value() / (shaderName + L".cso");
				outPath = outPath.lexically_normal();
				if (!writeFile(outPath, shader))
				{
					KOI_LOG_ERROR("Failed to write shader CSO file: {}", outPath.string());
				}
			}

			// Save PDB next to the CSO (prioritizing the hash over shader name for the PDB name)s
			{
				ComPtr<IDxcBlob> pdbBlob;
				ComPtr<IDxcBlobWide> pdbName;
				DX_CHECK(results->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdbBlob), &pdbName));
				if (pdbBlob && pdbBlob->GetBufferSize() != 0)
				{
					std::wstring pdbFileName;
					if (pdbName && pdbName->GetStringLength() != 0)
						pdbFileName = std::wstring(pdbName->GetStringPointer(), pdbName->GetStringLength());
					else
						pdbFileName = shaderName + L".pdb";

					fs::path outPath = (args.OutDir.value() / pdbFileName).lexically_normal();
					if (!writeFile(outPath, pdbBlob))
						KOI_LOG_ERROR("Failed to write shader PDB file: {}", outPath.string());
				}
			}

			// Print hash information
			{
				ComPtr<IDxcBlob> hash;
				DX_CHECK(results->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&hash), nullptr));
				if (hash && hash->GetBufferSize() != 0)
				{
					DxcShaderHash* hashBuf = static_cast<DxcShaderHash*>(hash->GetBufferPointer());
					std::span hashDigest(hashBuf->HashDigest);

					std::wstring hashStr;
					for (size_t i = 0; i < hashDigest.size(); i++)
					{
						hashStr += fmt::format(L"{:02x}", hashDigest[i]);
					}
					
					KOI_LOG_INFO("Compiled shader ({}) hash: {}", StringUtils::FromWide(shaderName), StringUtils::FromWide(hashStr));
				}
			}
		}

		return shader;
	}

	ShaderCompiler::ShaderCompiler()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		// Ensure the map size is correct.
		assert(s_shaderProfile.size() == (size_t)ShaderType::MAX_COUNT);
		assert(s_entryPoint.size() == (size_t)ShaderType::MAX_COUNT);

		DX_CHECK(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_dxcUtils)));
		DX_CHECK(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler)));
		DX_CHECK(m_dxcUtils->CreateDefaultIncludeHandler(&m_dxcIncludeHandler));
	}
}

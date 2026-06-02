#pragma once
#include "Graphics/RHI/DX12Common.h"
#include "Graphics/Types.h"
#include <filesystem>
#include <optional>
#include <dxcapi.h>

namespace koi
{
	namespace fs = std::filesystem;

	class ShaderCompiler
	{
	public:
		// Args to pass to the compiler
		struct Args
		{
			ShaderType Type = ShaderType::Unknown;
			std::vector<DxcDefine> Defines;
			std::optional<fs::path> OutDir;  // Directory to write shader CSO + PDB to
		};

	public:
		static ShaderCompiler& Get();

		ComPtr<IDxcBlob> CompileFromFile(const fs::path& filePath, const Args& args = {});

	private:
		ShaderCompiler();

	private:
		ComPtr<IDxcUtils> m_dxcUtils;
		ComPtr<IDxcCompiler3> m_dxcCompiler;
		ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;
	};
}

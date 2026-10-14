#pragma once
#include "Koi/Engine/Graphics/RHI/DX12Common.h"
#include "Koi/Engine/Graphics/Types.h"
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

		// When outIncludes is given, it receives every file resolved through #include during this compile, in the order DXC asked for them
		ComPtr<IDxcBlob> CompileFromFile(const fs::path& filePath, const Args& args = {}, std::vector<fs::path>* outIncludes = nullptr, ShaderInfo* outInfo = nullptr);

		// From a reflection blob: a .refl file (dxc -Fre) or DXC_OUT_REFLECTION
		[[nodiscard]] ShaderInfo Reflect(std::span<const byte> reflection) const;

	private:
		ShaderCompiler();

	private:
		ComPtr<IDxcUtils> m_dxcUtils;
		ComPtr<IDxcCompiler3> m_dxcCompiler;
	};
}

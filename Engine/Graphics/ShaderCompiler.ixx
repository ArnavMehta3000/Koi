module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <filesystem>
#include <optional>
#include <vector>
export module Koi.Graphics.ShaderCompiler;

import Koi.Core.Types;
import Koi.Graphics.Types;
import Koi.Graphics.RHI.DX12Common;

export namespace koi
{
	namespace fs = std::filesystem;

	class ShaderCompiler
	{
	public:
		struct Args
		{
			ShaderType               Type = ShaderType::Unknown;
			std::vector<DxcDefine>   Defines;
			std::optional<fs::path>  OutDir;
		};

		static ShaderCompiler& Get();
		ComPtr<IDxcBlob> CompileFromFile(const fs::path& filePath, const Args& args = {});

	private:
		ShaderCompiler();

		ComPtr<IDxcUtils>          m_dxcUtils;
		ComPtr<IDxcCompiler3>      m_dxcCompiler;
		ComPtr<IDxcIncludeHandler> m_dxcIncludeHandler;
	};
}

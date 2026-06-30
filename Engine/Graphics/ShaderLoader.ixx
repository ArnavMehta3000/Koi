module;
#include <filesystem>
#include <string_view>
#include <vector>
export module Koi.Graphics.ShaderLoader;

import Koi.Core.Types;
import Koi.Graphics.Types;
import Koi.Core.VirtualFileSystem;

export namespace koi
{
	[[nodiscard]] std::vector<byte> LoadShaderBytecode(std::string_view path);
	[[nodiscard]] ShaderType        InferShaderTypeFromName(std::string_view path);
	[[nodiscard]] std::vector<byte> LoadShader(std::string_view virtualPathNoExt);
	void                            InvalidateShaderCache();
}

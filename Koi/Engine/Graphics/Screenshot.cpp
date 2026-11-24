#include "Koi/Engine/Graphics/Screenshot.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/Device.h"
#include <ScreenGrab12.h>
#include <wincodec.h>
#include <format>
#include <system_error>

namespace koi::utils
{
	namespace
	{
		// WIC needs COM. This keeps the MTA alive without picking an apartment for the calling thread
		void EnsureCom()
		{
			[[maybe_unused]] static const HRESULT s_result = []
			{
				CO_MTA_USAGE_COOKIE cookie = nullptr;
				return ::CoIncrementMTAUsage(&cookie);
			}();
		}
	}

	Result<void> SaveScreenshot(GfxDevice& device, TextureHandle texture, const fs::path& path)
	{
		ZoneScoped;

		const Texture* tex = device.Get(texture);
		if (!tex || !tex->Resource)
		{
			return Error::Make("the texture is gone");
		}

		std::error_code ec;
		fs::create_directories(fs::absolute(path).parent_path(), ec);
		if (ec)
		{
			return Error::Make("couldn't create the folder: {}", ec.message());
		}

		EnsureCom();

		// The target is UNORM but the tonemapper already sRGB-encoded it, so the PNG must be tagged sRGB
		const HRESULT hr = DirectX::SaveWICTextureToFile(device.GetGraphicsQueue(), tex->Resource.Get(),
			GUID_ContainerFormatPng, path.c_str(), tex->CurrentState, tex->CurrentState, nullptr, nullptr, true);
		if (FAILED(hr))
		{
			return Error::Make("HRESULT 0x{:08X}", static_cast<u32>(hr));
		}
		return {};
	}

	fs::path TimestampedScreenshotPath(const fs::path& dir)
	{
		SYSTEMTIME t{};
		::GetLocalTime(&t);
		return dir / std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}-{:03}.png",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
	}
}

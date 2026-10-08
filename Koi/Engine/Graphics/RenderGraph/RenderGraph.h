#pragma once
#include "Koi/Engine/Graphics/RenderGraph/RGBuilder.h"
#include "Koi/Engine/Graphics/RenderGraph/RGContext.h"
#include "Koi/Engine/Graphics/RenderGraph/TransientPool.h"
#include <array>
#include <concepts>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace koi
{
	class GfxDevice;
	class CommandList;

	// What the last executed frame produced, for the editor's target viewers
	struct RGInspection
	{
		struct Output  // Pass output
		{
			std::string   Name;
			u16           Version = 0;
			TextureHandle Texture;
		};

		struct Pass
		{
			std::string_view    Name;
			std::vector<Output> Writes;
		};

		std::vector<Pass> Passes;
	};

	// Told about every write as passes are added, and may add passes of its own in response
	class IRGObserver
	{
	public:
		virtual ~IRGObserver() = default;
		virtual void OnWrite(RenderGraph& graph, std::string_view resource, RGTexture written) = 0;
	};

	class RenderGraph : NonCopyable
	{
		friend class RGBuilder;

		enum class Access : u8 { ShaderRead, ColorTarget, DepthTarget };

		struct Resource
		{
			std::string   Name;
			TextureDesc   Desc;
			TextureHandle Texture;
			u16           Version = 0;
			bool          Imported = false;
		};

		struct Use
		{
			RGTexture Texture;
			Access    Access;
		};

		struct Attachment
		{
			u16    Resource = RGTexture::kInvalid;
			LoadOp Load = LoadOp::Load;
			Color  Clear;
			f32    ClearDepth = 1.0f;
		};

		struct Pass
		{
			const char* Name = nullptr;  // interned, GPU zones keep the pointer
			std::vector<Use>                              Uses;
			std::vector<Attachment>                       ColorTargets;
			std::optional<Attachment>                     DepthTarget;
			std::move_only_function<void(RasterContext&)> Execute;
		};

	public:
		void Init(GfxDevice& device);
		void Shutdown();

		// Drops anything declared but never executed
		void Reset();

		[[nodiscard]] RGTexture CreateTexture(const TextureDesc& desc, std::string_view name);
		[[nodiscard]] RGTexture ImportTexture(TextureHandle texture, std::string_view name);
		[[nodiscard]] const TextureDesc& GetDesc(RGTexture texture) const;

		template <PassKind Kind, std::invocable<RGBuilder&> Setup, std::invocable<typename Kind::Context&> Exec>
		void AddPass(std::string_view name, Setup&& setup, Exec&& exec)
		{
			const u32 index = BeginPass(name);                   // Gets pass index
			RGBuilder builder(*this, index);                     // Prepare builder
			std::invoke(std::forward<Setup>(setup), builder);    // Invoke setup callback
			m_passes[index].Execute = std::forward<Exec>(exec);  // Save execute function
			NotifyWrites(index);
		}

		void Execute(CommandList& cmd);

		[[nodiscard]] TextureHandle Resolve(RGTexture texture) const;
		[[nodiscard]] inline const RGInspection& GetInspection() const noexcept { return m_inspection; }
		[[nodiscard]] inline GfxDevice& GetDevice() const noexcept { return *m_device; }

		inline void SetObserver(IRGObserver* observer) noexcept { m_observer = observer; }

	private:
		[[nodiscard]] u32 BeginPass(std::string_view name);
		[[nodiscard]] RGTexture Declare(u32 passIndex, RGTexture texture, Access access);
		void BindTargets(CommandList& cmd, const Pass& pass);
		void Inspect();
		void NotifyWrites(u32 passIndex);

	private:
		GfxDevice*                         m_device = nullptr;
		TransientPool                      m_pool;
		std::vector<Resource>              m_resources;
		std::vector<Pass>                  m_passes;
		std::set<std::string, std::less<>> m_names;
		RGInspection                       m_inspection;
		IRGObserver*                       m_observer  = nullptr;
		bool                               m_notifying = false;
	};
}

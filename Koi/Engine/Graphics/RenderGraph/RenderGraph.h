#pragma once
#include "Koi/Engine/Graphics/RenderGraph/RGBlackboard.h"
#include "Koi/Engine/Graphics/RenderGraph/RGBuilder.h"
#include "Koi/Engine/Graphics/RenderGraph/RGContext.h"
#include "Koi/Engine/Graphics/RenderGraph/TransientPool.h"
#include <array>
#include <concepts>
#include <functional>
#include <optional>
#include <set>
#include <span>
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
			std::string_view Name;  // interned by the graph
			u16              Version = 0;
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

		enum class Access : u8 { ShaderRead, ShaderWrite, ColorTarget, DepthTarget };

		struct Resource
		{
			std::string_view Name;
			TextureDesc   Desc;
			TextureHandle Texture;
			u16           Version = 0;
			bool          Imported = false;
		};

		struct BufferResource
		{
			std::string_view Name;
			BufferDesc   Desc;
			BufferHandle Buffer;
			u16          Version = 0;
			bool         Imported = false;
		};

		struct Use
		{
			u16    Index   = 0;
			u16    Version = 0;
			Access Access;
			bool   IsBuffer = false;
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
			u32                                           FirstUse = 0;  // into m_uses, contiguous per pass
			u32                                           UseCount = 0;
			std::array<Attachment, MAX_RENDER_TARGETS>    ColorTargets{};
			u32                                           ColorCount = 0;
			std::optional<Attachment>                     DepthTarget;
			bool                                          Compute = false;
			std::move_only_function<void(CommandList&, const TargetFormats&)> Execute;

			[[nodiscard]] std::span<const Attachment> Colors() const { return { ColorTargets.data(), ColorCount }; }
		};

	public:
		void Init(GfxDevice& device, PipelineCache& pipelines);
		void Shutdown();

		// Drops anything declared but never executed
		void Reset();

		[[nodiscard]] RGTexture CreateTexture(const TextureDesc& desc, std::string_view name);
		[[nodiscard]] RGTexture ImportTexture(TextureHandle texture, std::string_view name);
		[[nodiscard]] const TextureDesc& GetDesc(RGTexture texture) const;

		// Always structured buffer, R+W from shaders. Contents start undefined
		[[nodiscard]] RGBuffer CreateBuffer(const BufferDesc& desc, std::string_view name);
		[[nodiscard]] RGBuffer ImportBuffer(BufferHandle buffer, std::string_view name);
		[[nodiscard]] const BufferDesc& GetDesc(RGBuffer buffer) const;

		template <PassKind Kind, std::invocable<RGBuilder&> Setup, std::invocable<typename Kind::Context&> Exec>
		void AddPass(std::string_view name, Setup&& setup, Exec&& exec)
		{
			const u32 index = BeginPass(name);                 // Gets pass index
			RGBuilder builder(*this, index);                   // Prepare builder
			std::invoke(std::forward<Setup>(setup), builder);  // Invoke setup callback

			// Save execute function, wrapped to build the context its kind asks for
			Pass& pass   = m_passes[index];
			pass.Compute = std::same_as<Kind, ComputePass>;
			pass.Execute = [this, exec = std::forward<Exec>(exec)](CommandList& cmd, [[maybe_unused]] const TargetFormats& formats) mutable
			{
				if constexpr (std::same_as<Kind, ComputePass>)
				{
					ComputeContext ctx(cmd, *this);
					std::invoke(exec, ctx);
				}
				else
				{
					RasterContext ctx(cmd, *this, formats);
					std::invoke(exec, ctx);
				}
			};
			NotifyWrites(index);
		}

		void Execute(CommandList& cmd);

		[[nodiscard]] TextureHandle Resolve(RGTexture texture) const;
		[[nodiscard]] BufferHandle Resolve(RGBuffer buffer) const;
		[[nodiscard]] inline RGBlackboard& GetBlackboard() noexcept { return m_blackboard; }
		[[nodiscard]] inline const RGInspection& GetInspection() const noexcept { return m_inspection; }
		[[nodiscard]] inline GfxDevice& GetDevice() const noexcept { return *m_device; }
		[[nodiscard]] inline PipelineCache& GetPipelines() const noexcept { return *m_pipelines; }

		inline void SetObserver(IRGObserver* observer) noexcept { m_observer = observer; }

	private:
		[[nodiscard]] const std::string& Intern(std::string_view name);
		[[nodiscard]] std::span<const Use> UsesOf(const Pass& pass) const { return { m_uses.data() + pass.FirstUse, pass.UseCount }; }
		[[nodiscard]] u32 BeginPass(std::string_view name);
		[[nodiscard]] RGTexture Declare(u32 passIndex, RGTexture texture, Access access);
		[[nodiscard]] RGBuffer Declare(u32 passIndex, RGBuffer buffer, Access access);
		void PushUse(u32 passIndex, const Use& use);
		void BindTargets(CommandList& cmd, const Pass& pass);
		[[nodiscard]] TargetFormats FormatsOf(const Pass& pass) const;
		void Inspect();
		void NotifyWrites(u32 passIndex);

	private:
		GfxDevice*                         m_device    = nullptr;
		PipelineCache*                     m_pipelines = nullptr;
		TransientPool                      m_pool;
		std::vector<Resource>              m_resources;
		std::vector<BufferResource>        m_buffers;
		std::vector<Pass>                  m_passes;
		std::vector<Use>                   m_uses;
		std::set<std::string, std::less<>> m_names;
		RGBlackboard                       m_blackboard;
		RGInspection                       m_inspection;
		IRGObserver*                       m_observer  = nullptr;
		bool                               m_notifying = false;
	};
}

# Customizing the renderer

The `Renderer` runs a **pipeline**: an ordered list of **features**. Every frame, each feature records passes into a **render graph**. The graph creates temporary textures, inserts barriers, binds render targets and then executes the passes.

The engine's default is `DeferredPipeline::Build`: Shadow → GBuffer → Lighting → Tonemap → Grid → DebugDraw → Blit. The source for each one is in `Koi/Engine/Graphics/RenderPasses/`.

## Swapping the pipeline

A pipeline is just a function. Pass yours to `SetPipeline`, which clears the feature list and calls it:

```cpp
void MyPipeline(koi::Renderer& renderer, koi::FeatureList& features)
{
	koi::DeferredPipeline::Build(renderer, features);  // start from the engine's features
	features.Add<VignettePass>();                      // then add your own
}

// in OnInit
GetEngine()->GetSubsystem<koi::Renderer>().SetPipeline(&MyPipeline);
```

The order you add features in is the order they run. You can also skip `DeferredPipeline::Build` and add only the engine features you want. To reach a feature later, for example to change its settings, use `renderer.GetFeatures().Find<GridPass>()`.

## Writing a feature

A feature is any type with a `kName` and a `Record` method. It doesn't need a base class:

```cpp
class VignettePass
{
public:
	static constexpr std::string_view kName = "Vignette";
	void Record(koi::RenderGraph& graph, const koi::RenderView& view, koi::World& world);
};
```

`RenderView` gives you the camera and the output target for this frame. `World` is the scene, if you need to read entities.

## The blackboard

Features never point at each other. They share data through the graph's blackboard, which is cleared every frame:

| Entry | Published by | What it holds |
|---|---|---|
| `ShadowData` | Shadow | Shadow map and light matrix. Missing when nothing casts a shadow |
| `GBufferData` | GBuffer | Albedo, normals, material |
| `SceneDepth` | GBuffer | D32 depth |
| `SceneColor` | Lighting | Lit HDR color |
| `DisplayColor` | Tonemap | Tonemapped LDR color. Overlays draw on this, and Blit copies it to the output |

Use `bb.TryGet<T>()` for data that might be missing, `bb.Get<T>()` when it must be there, and `bb.Add(T{...})` to publish your own. Any struct works as an entry, so you can define your own types to pass between your features.

## Adding a pass

`graph.AddPass<RasterPass>` or `graph.AddPass<ComputePass>` takes a name and two lambdas. The **setup** lambda declares what the pass reads and writes. The **execute** lambda records the GPU work later, after the graph has placed barriers:

```cpp
namespace
{
	constexpr koi::ComputePipeline kVignette{ .ComputeShader = "project://Content/Shaders/VignetteCS" };
}

void VignettePass::Record(koi::RenderGraph& graph, const koi::RenderView&, koi::World&)
{
	koi::DisplayColor* display = graph.GetBlackboard().TryGet<koi::DisplayColor>();
	if (!display) return;

	const koi::TextureDesc desc = graph.GetDesc(display->Color);

	graph.AddPass<koi::ComputePass>("Vignette",
		[&](koi::RGBuilder& b)
		{
			display->Color = b.Write(display->Color);  // a write returns the new version of the texture
		},
		[color = display->Color, desc](koi::ComputeContext& ctx)
		{
			if (!ctx.SetPipeline(kVignette)) return;
			ctx.SetRootConstants(VignetteRootConstants{ .ColorIndex = ctx.UAV(color) });

			const auto& group = ctx.GetThreadGroup();
			ctx.Dispatch((desc.Width + group[0] - 1) / group[0], (desc.Height + group[1] - 1) / group[1]);
		});
}
```

What you can declare in setup:

| | |
|---|---|
| `b.Read(tex)` | Read in a shader (SRV) |
| `b.Write(tex)` | Write from a compute shader (UAV) |
| `b.ColorTarget(tex, load, clear)` | Render target for a raster pass |
| `b.DepthTarget(tex, load, clearDepth)` | Depth target for a raster pass |

Make new textures with `graph.CreateTexture(desc, name)`. The graph allocates them for the frame and reuses the memory afterwards. To bring in a texture you own, use `graph.ImportTexture`.

In execute, `ctx.SRV(tex)` and `ctx.UAV(tex)` give you bindless descriptor indices to put in your root constants. Raster passes draw with `Draw` / `DrawIndexed`, and compute passes use `Dispatch`.

## Pipelines and shaders

Describe a pass's GPU state as a `constexpr` value. The graph builds and caches it the first time it's used, so there's no setup code:

```cpp
constexpr koi::RasterPipeline kOutline
{
	.VertexShader = "project://Content/Shaders/OutlineVS",
	.PixelShader  = "project://Content/Shaders/OutlinePS",
	.Rasterizer   = koi::RasterizerDesc::CullNone(),        // CullBack (default), CullNone
	.Blend        = koi::BlendTargetDesc::AlphaBlend(),     // Opaque (default), AlphaBlend, Additive
	.DepthStencil = koi::DepthStencilDesc::Read(),          // Default, Read, None
};
```

Render target formats come from the pass that binds the pipeline, so one description works on any target.

Shaders go in your project's `Content/Shaders/Shaders.json` (see [Shaders](Shaders.md)). A few rules:

- A shader's `b0` is `ConstantBuffer<XRootConstants> g_root : register(b0)`. Everything else is reached through `ResourceDescriptorHeap[index]`.
- Put structs shared with C++ in an `Interop/*.hlsli` file written with C++ names (`Matrix`, `Vector4`, `u32`), and include the engine's `Interop.hlsli`. Pad with scalars, never arrays, and add a `KOI_CHECK_SIZE`.
- Root constants are capped at 32 `u32`s. For anything bigger, upload a constant buffer (`GpuFrameBuffer<T>`, see `GridPass`) and pass its index.

F5 in the editor recompiles changed shaders and rebuilds their pipelines, so you don't need to restart.

## Exposing settings to the editor

Give your feature a settings struct with `kKey` and `kReflect = true`. An editor panel can then draw it with `utils::DrawReflected` and save it with `Serializer<T>`. `GridPass::Settings` and `Koi/Editor/Panels/GridPanel.cpp` are a full example. See [Editor extensions](EditorExtensions.md).

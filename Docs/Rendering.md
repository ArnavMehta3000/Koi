# Customizing the renderer

A `SceneRenderer` renders one view with a **pipeline**: an ordered list of **features**. Every frame, each feature records passes into that view's **render graph**. The graph creates temporary textures, inserts barriers, binds render targets and then executes the passes.

The engine's default is `DeferredPipeline::Build`: Shadow → GBuffer → Lighting → AutoExposure → Tonemap → Grid → DebugDraw → Blit. Grid is only in `editor-*` modes, and DebugDraw is left out of `game-release`. The source for each one is in `Koi/Engine/Graphics/RenderPasses/`.

## Swapping the pipeline

A pipeline is just a function, registered with the `Renderer` under a name. A `SceneRenderer` is built from one, and `SetPipeline(name)` rebuilds its features from another:

```cpp
void MyPipeline(koi::Renderer& renderer, koi::FeatureList& features)
{
	koi::DeferredPipeline::Build(renderer, features);  // start from the engine's features
	features.Add<VignettePass>();                      // then add your own
}

// in OnInit
koi::Renderer& renderer = GetEngine()->GetSubsystem<koi::Renderer>();
renderer.RegisterPipeline("MyPipeline", &MyPipeline);
renderer.GetMainSceneRenderer().SetPipeline("MyPipeline");
```

The order you add features in is the order they run. You can also skip `DeferredPipeline::Build` and add only the engine features you want. To reach a feature later, for example to change its settings, use `renderer.GetMainSceneRenderer().GetFeatures().Find<GridPass>()`.

Every view has its own `SceneRenderer`, so its own feature instances: the game world uses the main one, and an editor preview creates another (`std::make_unique<SceneRenderer>(renderer, "Deferred")`). Call `Submit(view, scene)` each frame to record it; the `Renderer` executes every submitted view at the end of the frame, in order. `ViewFlags` on the `RenderView` turn on the grid and the shared debug lines.

## Writing a feature

A feature is any type with a `kName` and a `Record` method. It doesn't need a base class:

```cpp
class VignettePass
{
public:
	static constexpr std::string_view kName = "Vignette";
	void Record(koi::RenderGraph& graph, const koi::RenderView& view, const koi::RenderScene& scene);
};
```

`RenderView` gives you the camera and the output target for this frame. `RenderScene` is what to draw: objects and lights to `Gather`, the scene bounds and the resolved `PostProcess`. Features never see the `World`.

## The blackboard

Features never point at each other. They share data through the graph's blackboard, which is cleared every frame:

| Entry | Published by | What it holds |
|---|---|---|
| `ShadowData` | Shadow | Shadow map and light matrix. Missing when nothing casts a shadow |
| `GBufferData` | GBuffer | Albedo, normals, material |
| `SceneDepth` | GBuffer | D32 depth |
| `SceneColor` | GBuffer, then Lighting | HDR color. GBuffer starts it with emissive, Lighting adds to it |
| `ExposureData` | AutoExposure | One-float buffer the tonemapper scales `SceneColor` by. Missing when auto exposure is off |
| `DisplayColor` | Tonemap | Tonemapped LDR color. Overlays draw on this, and Blit copies it to the output |

Use `bb.TryGet<T>()` for data that might be missing, `bb.Get<T>()` when it must be there, and `bb.Add(T{...})` to publish your own. Any struct works as an entry, so you can define your own types to pass between your features.

## Adding a pass

`graph.AddPass<RasterPass>` or `graph.AddPass<ComputePass>` takes a name and two lambdas. The **setup** lambda declares what the pass reads and writes. The **execute** lambda records the GPU work later, after the graph has placed barriers:

```cpp
namespace
{
	constexpr koi::ComputePipeline kVignette{ .ComputeShader = "project://Content/Shaders/VignetteCS" };
}

void VignettePass::Record(koi::RenderGraph& graph, const koi::RenderView&, const koi::RenderScene&)
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
| `b.Read(tex)` / `b.Read(buf)` | Read in a shader (SRV) |
| `b.Write(tex)` / `b.Write(buf)` | Write from a compute shader (UAV) |
| `b.CopyFrom(buf)` | Copy out of it in execute, e.g. into a readback buffer |
| `b.ColorTarget(tex, load, clear)` | Render target for a raster pass |
| `b.DepthTarget(tex, load, clearDepth)` | Depth target for a raster pass |

Make new textures with `graph.CreateTexture(desc, name)` and buffers with `graph.CreateBuffer(desc, name)`. The graph allocates them for the frame and reuses the memory afterwards. Graph buffers are always structured (set a `Stride`) and start with undefined contents, so clear them in a pass first. To bring in a resource you own, use `graph.ImportTexture` / `graph.ImportBuffer`. Two writes in a row to the same resource get a UAV barrier between them automatically.

In execute, `ctx.SRV(res)` and `ctx.UAV(res)` give you bindless descriptor indices to put in your root constants. Raster passes draw with `Draw` / `DrawIndexed`, and compute passes use `Dispatch`. `ctx.CopyBuffer(dst, offset, buf, size)` copies a `CopyFrom` buffer into one the graph doesn't track; `AutoExposurePass` uses it to read its histogram back for the editor.

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

## Post-process settings

Settings that belong to the scene live in the `PostProcess` component, one nested struct per effect (`AutoExposure`, `Tonemap`). A feature reads them with `scene.GetPostProcess()`: the first `PostProcess` in the world wins, and without one the struct defaults apply. There are no project-level settings; a scene that wants different values adds the component:

```toml
[entity.PostProcess.tonemap]
operator = 'AgX'  # ACES | AgX | PBRNeutral | Hable | Reinhard | None
```

A new effect adds its own struct to `PostProcess` (give it an `operator==` for undo) and a section to `PostProcessInspector`.

## Exposing settings to the editor

For editor-only settings that aren't scene content, give your feature a settings struct with `kKey` and `KOI_REFLECT(...)`. An editor panel can then draw it with `utils::DrawReflected` and save it with `Serializer<T>`. `GridPass::Settings` and `Koi/Editor/Panels/GridPanel.cpp` are a full example. See [Editor extensions](EditorExtensions.md).

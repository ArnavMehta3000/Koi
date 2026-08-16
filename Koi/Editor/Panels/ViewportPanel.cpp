#include "Koi/Editor/Panels/ViewportPanel.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <cmath>
#include <limits>

namespace koi
{
	// Must match the RT formats RendererResources bakes into MeshOpaquePipeline / TonemapPipeline
	static constexpr Format kHDRFormat = Format::R16G16B16A16_FLOAT;
	static constexpr Format kResolvedFormat = Format::R8G8B8A8_UNORM;

	// A click that moves further than this is a camera drag, not a pick
	static constexpr f32 kPickDragSlop = 4.0f;

	ViewportPanel::ViewportPanel(Renderer& renderer, AppBase& game, EditorSelection& selection)
		: m_renderer(&renderer)
		, m_game(&game)
		, m_selection(&selection)
		, m_controller(&m_camera, CameraMode::Orbit)
	{
	}

	ViewportPanel::~ViewportPanel()
	{
		GfxDevice& device = m_renderer->GetDevice();
		if (m_target.HDRColorBuffer)      device.Destroy(m_target.HDRColorBuffer);
		if (m_target.ResolvedColorBuffer) device.Destroy(m_target.ResolvedColorBuffer);
	}

	void ViewportPanel::EnsureSize(u32 width, u32 height)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		if (width == 0 || height == 0)
		{
			m_valid = false;
			return;
		}

		if (m_valid && m_target.Width == width && m_target.Height == height) return;

		GfxDevice& device = m_renderer->GetDevice();
		if (m_target.HDRColorBuffer)      device.Destroy(m_target.HDRColorBuffer);
		if (m_target.ResolvedColorBuffer) device.Destroy(m_target.ResolvedColorBuffer);

		m_target.HDRColorBuffer = device.CreateTexture(TextureDesc{
			.Width  = width,
			.Height = height,
			.Format = kHDRFormat,
			.Usage  = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "Viewport HDR Color",
		});

		m_target.ResolvedColorBuffer = device.CreateTexture(TextureDesc
		{
			.Width     = width,
			.Height    = height,
			.Format    = kResolvedFormat,
			.Usage     = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "Viewport Resolved Color",
		});

		m_target.Width  = width;
		m_target.Height = height;
		m_valid = true;

		m_camera.SetAspect(static_cast<f32>(width) / static_cast<f32>(height));

		KOI_LOG_DEBUG("Viewport render targets resized to {}x{}", width, height);
	}

	const ViewportTarget* ViewportPanel::PrepareFrame(CommandList& cmd)
	{
		ZoneScoped;
		EnsureSize(m_desiredWidth, m_desiredHeight);
		if (!m_valid) return nullptr;

		static constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.TransitionBarrier(m_target.HDRColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd.ClearRenderTarget(m_target.HDRColorBuffer, kClearColor);

		return &m_target;
	}

	void ViewportPanel::DrawToolbar()
	{
		static constexpr const char* kCameraLabels[]     = { "Scene", "Game" };
		static constexpr const char* kResolutionLabels[] = { "Free Aspect", "Window", "1920x1080", "1280x720" };

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

		ImGui::SetNextItemWidth(90.0f);
		int cameraMode = static_cast<int>(m_cameraMode);
		if (ImGui::Combo("##camera", &cameraMode, kCameraLabels, IM_ARRAYSIZE(kCameraLabels)))
			m_cameraMode = static_cast<ViewportCamera>(cameraMode);

		// Resolution only means something for the game camera; the scene camera
		// always fills the panel
		if (m_cameraMode == ViewportCamera::Game)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			int resolution = static_cast<int>(m_resolution);
			if (ImGui::Combo("##resolution", &resolution, kResolutionLabels, IM_ARRAYSIZE(kResolutionLabels)))
				m_resolution = static_cast<ViewportResolution>(resolution);
		}
		else
		{
			static constexpr const char* kGizmoLabels[] = { "Translate", "Rotate", "Scale" };
			static constexpr const char* kSpaceLabels[] = { "World", "Local" };

			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.0f);
			int op = static_cast<int>(m_gizmoOp);
			if (ImGui::Combo("##gizmoop", &op, kGizmoLabels, IM_ARRAYSIZE(kGizmoLabels)))
				m_gizmoOp = static_cast<GizmoOp>(op);

			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.0f);
			int space = static_cast<int>(m_gizmoSpace);
			if (ImGui::Combo("##gizmospace", &space, kSpaceLabels, IM_ARRAYSIZE(kSpaceLabels)))
				m_gizmoSpace = static_cast<GizmoSpace>(space);
		}

		ImGui::PopStyleVar();
	}

	void ViewportPanel::OnDraw()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("Viewport"))
		{
			DrawToolbar();

			// Nothing to size a target from when collapsed, and the aspect maths
			// below would divide by zero
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			if (avail.x >= 1.0f && avail.y >= 1.0f)
			{
				// Scene view always tracks the panel; the fixed modes render at their
				// own size and get scaled to fit below
				switch (m_cameraMode == ViewportCamera::Scene ? ViewportResolution::FreeAspect : m_resolution)
				{
					case ViewportResolution::Window:
						if (const RenderWindow* rw = m_renderer->GetMainRenderWindow())
						{
							m_desiredWidth  = rw->GetWidth();
							m_desiredHeight = rw->GetHeight();
							break;
						}
						[[fallthrough]];  // no window yet, fall back to the panel

					case ViewportResolution::FreeAspect:
						m_desiredWidth  = static_cast<u32>(avail.x);
						m_desiredHeight = static_cast<u32>(avail.y);
						break;

					case ViewportResolution::R1920x1080: m_desiredWidth = 1920; m_desiredHeight = 1080; break;
					case ViewportResolution::R1280x720:  m_desiredWidth = 1280; m_desiredHeight = 720;  break;
				}

				if (m_valid)
				{
					GfxDevice& device = m_renderer->GetDevice();
					if (const Texture* tex = device.Get(m_target.ResolvedColorBuffer);
						tex && tex->SRVIndex != INVALID_DESCRIPTOR)
					{
						// Fit the target inside the panel without distorting it; when the
						// aspects already agree this is the full region
						const f32 targetAspect = static_cast<f32>(m_target.Width) / static_cast<f32>(m_target.Height);
						const f32 availAspect  = avail.x / avail.y;

						const ImVec2 size = (targetAspect > availAspect)
							? ImVec2(avail.x, avail.x / targetAspect)
							: ImVec2(avail.y * targetAspect, avail.y);

						const ImVec2 cursor = ImGui::GetCursorPos();
						ImGui::SetCursorPos(ImVec2(cursor.x + (avail.x - size.x) * 0.5f,
						                           cursor.y + (avail.y - size.y) * 0.5f));

						const ImVec2 imageMin = ImGui::GetCursorScreenPos();
						const auto gpu = device.ResourceHeap().GpuHandle(tex->SRVIndex);
						ImGui::Image(static_cast<ImTextureID>(gpu.ptr), size);

						m_isHovered = ImGui::IsItemHovered() || ImGui::IsItemActive();

						DrawGizmo(imageMin.x, imageMin.y, size.x, size.y);
						HandlePicking(imageMin.x, imageMin.y, size.x, size.y);
					}
				}
			}
			else
			{
				m_isHovered = false;
			}
		}
		else
		{
			m_isHovered = false;
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::DrawGizmo(f32 x, f32 y, f32 width, f32 height)
	{
		m_gizmoActive = false;

		// No gizmo in game view or when no entity is selected
		if (m_cameraMode != ViewportCamera::Scene || !m_selection->IsValid()) return;

		Transform* xform = m_selection->Selected.try_get_mut<Transform>();
		if (!xform) return;

		// Right-mouse is the camera - leave the shortcuts alone while it is held
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoOp = GizmoOp::Translate;
			if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoOp = GizmoOp::Rotate;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOp = GizmoOp::Scale;
		}

		static constexpr ImGuizmo::OPERATION kOps[] =
		{
			ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE
		};

		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(x, y, width, height);

		Matrix matrix = xform->ToMatrix();
		if (ImGuizmo::Manipulate(&m_camera.View._11,
		                         &m_camera.Projection._11,
		                         kOps[static_cast<u32>(m_gizmoOp)],
		                         m_gizmoSpace == GizmoSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
		                         &matrix._11))
		{
			Vector3 scale, position;
			Quaternion rotation;
			if (matrix.Decompose(scale, rotation, position))
			{
				xform->Position = position;
				xform->Rotation = rotation;
				xform->Scale    = scale;
				m_selection->Selected.modified<Transform>();
			}
		}

		m_gizmoActive = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
	}

	void ViewportPanel::HandlePicking(f32 x, f32 y, f32 width, f32 height)
	{
		if (m_cameraMode != ViewportCamera::Scene || m_gizmoActive) return;
		if (!m_isHovered || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;

		// Orbiting also releases the left button over the image, so only a click that stayed put counts as a pick
		const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
		if (std::abs(drag.x) + std::abs(drag.y) > kPickDragSlop) return;

		const ImVec2 mouse = ImGui::GetIO().MousePos;
		PickEntity((mouse.x - x) / width  * 2.0f - 1.0f,
		           1.0f - (mouse.y - y) / height * 2.0f);
	}

	void ViewportPanel::PickEntity(f32 ndcX, f32 ndcY)
	{
		ZoneScoped;
		World& world = m_game->GetWorld();

		const Matrix invViewProj = m_camera.ViewProjection.Invert();

		const Vector4 nearH = Vector4::Transform(Vector4(ndcX, ndcY, 0.0f, 1.0f), invViewProj);
		const Vector4 farH  = Vector4::Transform(Vector4(ndcX, ndcY, 1.0f, 1.0f), invViewProj);

		const Vector3 origin(nearH.x / nearH.w, nearH.y / nearH.w, nearH.z / nearH.w);
		const Vector3 target(farH.x / farH.w, farH.y / farH.w, farH.z / farH.w);

		Vector3 direction = target - origin;
		direction.Normalize();

		Entity hit;
		f32 best = std::numeric_limits<f32>::max();

		world.GetECS()
			.query<const Transform, const MeshRef>()
			.each([&](Entity e, const Transform& xform, const MeshRef& meshRef)
		{
			const GPUMesh* mesh = world.GetMesh(meshRef.Handle);
			if (!mesh) return;

			// Bounds are model-space, so the ray goes to the mesh rather than the box to the world
			const Matrix worldMatrix = xform.ToMatrix();
			const Matrix invert      = worldMatrix.Invert();

			Ray ray(Vector3::Transform(origin, invert), Vector3::TransformNormal(direction, invert));
			ray.direction.Normalize();  // non-uniform scale denormalises it

			const Vector3 center  = mesh->Bounds.Center();
			const Vector3 extents = (mesh->Bounds.Max - mesh->Bounds.Min) * 0.5f;
			const DirectX::BoundingBox box(center, extents);

			f32 modelDist = 0.0f;
			if (!ray.Intersects(box, modelDist)) return;

			// Model-space distances are not comparable across different scales
			const Vector3 point = Vector3::Transform(ray.position + ray.direction * modelDist, worldMatrix);
			const f32 distance  = Vector3::Distance(point, origin);
			if (distance < best)
			{
				best = distance;
				hit  = e;
			}
		});

		m_selection->Selected = hit;
	}

	void ViewportPanel::FrameBounds(const AABB& bounds)
	{
		const Vector3 center = bounds.Center();
		const f32 radius = bounds.Radius();

		m_camera.SetProjection(0.8f, m_camera.Aspect, 0.05f, 1000.0f);
		m_controller.SetOrbitTarget(center, radius * 2.5f, 0.0f, 0.3f);
	}

	void ViewportPanel::Update(f32 dt, const InputManager& input)
	{
		const bool anyMouseDown = input.IsMouseButtonDown(MouseButton::Left)
			|| input.IsMouseButtonDown(MouseButton::Right)
			|| input.IsMouseButtonDown(MouseButton::Middle);

		if (m_isHovered && anyMouseDown && !m_gizmoActive) m_isCapturingInput = true;
		else if (!anyMouseDown) m_isCapturingInput = false;

		if (!m_gizmoActive && (m_isHovered || m_isCapturingInput))
		{
			m_controller.Update(dt, input);
		}
	}
}

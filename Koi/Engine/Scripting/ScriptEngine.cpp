#include "Koi/Engine/Scripting/ScriptEngine.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Scripting/ScriptComponent.h"
#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptstdstring/scriptstdstring.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <new>
#include <system_error>

namespace koi
{
	namespace
	{
		// Injected as an extra section into every compiled module, ahead of the user's own source,
		// since there is no #include support yet to let scripts pull it in themselves
		constexpr std::string_view kScriptBehaviourSource = R"as(
class ScriptBehaviour
{
	Entity@ self;

	void OnCreate() {}
	void OnUpdate(float dt) {}
	void OnDestroy() {}
}
)as";

		// The Entity type scripts see. Wraps a flecs id rather than an Entity handle directly, since
		// the underlying entity can be destroyed out from under a script - every access re-resolves
		// and checks is_alive(). AS-refcounted; never constructed by scripts, only handed out via 'self'
		class ScriptEntityHandle
		{
		public:
			explicit ScriptEntityHandle(EntityID id) : m_id(id) {}

			void AddRef() { ++m_refCount; }
			void Release() { if (--m_refCount == 0) delete this; }

			[[nodiscard]] bool IsValid() const { return static_cast<bool>(Resolve()); }
			void Destroy() const { if (Entity e = Resolve()) e.destruct(); }
			[[nodiscard]] std::string GetName() const { Entity e = Resolve(); return e ? std::string(e.name().c_str()) : std::string(); }

			[[nodiscard]] Vector3 GetPosition() const
			{
				const Transform* t = GetTransform();
				return t ? t->Position : Vector3::Zero;
			}

			void SetPosition(const Vector3& value)
			{
				if (Transform* t = GetTransformMut()) t->Position = value;
			}

			[[nodiscard]] Quaternion GetRotation() const
			{
				const Transform* t = GetTransform();
				return t ? t->Rotation : Quaternion::Identity;
			}

			void SetRotation(const Quaternion& value)
			{
				if (Transform* t = GetTransformMut()) t->Rotation = value;
			}

			[[nodiscard]] Vector3 GetScale() const
			{
				const Transform* t = GetTransform();
				return t ? t->Scale : Vector3::One;
			}

			void SetScale(const Vector3& value)
			{
				if (Transform* t = GetTransformMut()) t->Scale = value;
			}

		private:
			[[nodiscard]] Entity Resolve() const
			{
				Entity e(AppBase::Get().GetWorld().GetECS(), m_id);
				return e.is_alive() ? e : Entity();
			}

			[[nodiscard]] const Transform* GetTransform() const { Entity e = Resolve(); return e ? e.try_get<Transform>() : nullptr; }
			[[nodiscard]] Transform* GetTransformMut() { Entity e = Resolve(); return e ? e.try_get_mut<Transform>() : nullptr; }

			EntityID m_id;
			i32      m_refCount = 1;
		};

		void ConstructVector3(Vector3* self)                          { new (self) Vector3(); }
		void ConstructVector3XYZ(f32 x, f32 y, f32 z, Vector3* self)   { new (self) Vector3(x, y, z); }
		void ConstructVector3Copy(const Vector3& other, Vector3* self) { new (self) Vector3(other); }

		// Vector3's arithmetic operators are free functions in SimpleMath, not members, so opAdd/opSub/
		// opMul need a wrapper with 'self' last (asCALL_CDECL_OBJLAST) rather than asMETHODPR
		Vector3 Vector3Add(const Vector3& rhs, const Vector3* self) { return *self + rhs; }
		Vector3 Vector3Sub(const Vector3& rhs, const Vector3* self) { return *self - rhs; }
		Vector3 Vector3MulScalar(f32 rhs, const Vector3* self)      { return *self * rhs; }

		void ConstructQuaternion(Quaternion* self)                             { new (self) Quaternion(); }
		void ConstructQuaternionXYZW(f32 x, f32 y, f32 z, f32 w, Quaternion* self) { new (self) Quaternion(x, y, z, w); }
		void ConstructQuaternionCopy(const Quaternion& other, Quaternion* self)    { new (self) Quaternion(other); }

		bool Input_IsKeyDown(KeyCode key)
		{
			InputManager& input = Engine::Get().GetSubsystem<InputManager>();
			return input.IsGameFocused() && input.IsKeyDown(key);
		}

		bool Input_IsKeyPressed(KeyCode key)
		{
			InputManager& input = Engine::Get().GetSubsystem<InputManager>();
			return input.IsGameFocused() && input.IsKeyPressed(key);
		}

		bool Input_IsKeyReleased(KeyCode key)
		{
			InputManager& input = Engine::Get().GetSubsystem<InputManager>();
			return input.IsGameFocused() && input.IsKeyReleased(key);
		}

		// name/value table for RegisterEnumValue - mirrors Platform/InputCodes.h
		constexpr std::array kKeyCodes
		{
			std::pair{ "Unknown", KeyCode::Unknown },
			std::pair{ "Space", KeyCode::Space }, std::pair{ "Apostrophe", KeyCode::Apostrophe },
			std::pair{ "Comma", KeyCode::Comma }, std::pair{ "Minus", KeyCode::Minus },
			std::pair{ "Period", KeyCode::Period }, std::pair{ "Slash", KeyCode::Slash },
			std::pair{ "Num0", KeyCode::Num0 }, std::pair{ "Num1", KeyCode::Num1 }, std::pair{ "Num2", KeyCode::Num2 },
			std::pair{ "Num3", KeyCode::Num3 }, std::pair{ "Num4", KeyCode::Num4 }, std::pair{ "Num5", KeyCode::Num5 },
			std::pair{ "Num6", KeyCode::Num6 }, std::pair{ "Num7", KeyCode::Num7 }, std::pair{ "Num8", KeyCode::Num8 },
			std::pair{ "Num9", KeyCode::Num9 },
			std::pair{ "Semicolon", KeyCode::Semicolon }, std::pair{ "Equal", KeyCode::Equal },
			std::pair{ "A", KeyCode::A }, std::pair{ "B", KeyCode::B }, std::pair{ "C", KeyCode::C },
			std::pair{ "D", KeyCode::D }, std::pair{ "E", KeyCode::E }, std::pair{ "F", KeyCode::F },
			std::pair{ "G", KeyCode::G }, std::pair{ "H", KeyCode::H }, std::pair{ "I", KeyCode::I },
			std::pair{ "J", KeyCode::J }, std::pair{ "K", KeyCode::K }, std::pair{ "L", KeyCode::L },
			std::pair{ "M", KeyCode::M }, std::pair{ "N", KeyCode::N }, std::pair{ "O", KeyCode::O },
			std::pair{ "P", KeyCode::P }, std::pair{ "Q", KeyCode::Q }, std::pair{ "R", KeyCode::R },
			std::pair{ "S", KeyCode::S }, std::pair{ "T", KeyCode::T }, std::pair{ "U", KeyCode::U },
			std::pair{ "V", KeyCode::V }, std::pair{ "W", KeyCode::W }, std::pair{ "X", KeyCode::X },
			std::pair{ "Y", KeyCode::Y }, std::pair{ "Z", KeyCode::Z },
			std::pair{ "LeftBracket", KeyCode::LeftBracket }, std::pair{ "Backslash", KeyCode::Backslash },
			std::pair{ "RightBracket", KeyCode::RightBracket }, std::pair{ "GraveAccent", KeyCode::GraveAccent },
			std::pair{ "Escape", KeyCode::Escape }, std::pair{ "Enter", KeyCode::Enter },
			std::pair{ "Tab", KeyCode::Tab }, std::pair{ "Backspace", KeyCode::Backspace },
			std::pair{ "Insert", KeyCode::Insert }, std::pair{ "Delete", KeyCode::Delete },
			std::pair{ "Right", KeyCode::Right }, std::pair{ "Left", KeyCode::Left },
			std::pair{ "Down", KeyCode::Down }, std::pair{ "Up", KeyCode::Up },
			std::pair{ "PageUp", KeyCode::PageUp }, std::pair{ "PageDown", KeyCode::PageDown },
			std::pair{ "Home", KeyCode::Home }, std::pair{ "End", KeyCode::End },
			std::pair{ "CapsLock", KeyCode::CapsLock }, std::pair{ "ScrollLock", KeyCode::ScrollLock },
			std::pair{ "NumLock", KeyCode::NumLock }, std::pair{ "PrintScreen", KeyCode::PrintScreen },
			std::pair{ "Pause", KeyCode::Pause },
			std::pair{ "F1", KeyCode::F1 }, std::pair{ "F2", KeyCode::F2 }, std::pair{ "F3", KeyCode::F3 },
			std::pair{ "F4", KeyCode::F4 }, std::pair{ "F5", KeyCode::F5 }, std::pair{ "F6", KeyCode::F6 },
			std::pair{ "F7", KeyCode::F7 }, std::pair{ "F8", KeyCode::F8 }, std::pair{ "F9", KeyCode::F9 },
			std::pair{ "F10", KeyCode::F10 }, std::pair{ "F11", KeyCode::F11 }, std::pair{ "F12", KeyCode::F12 },
			std::pair{ "LeftShift", KeyCode::LeftShift }, std::pair{ "LeftControl", KeyCode::LeftControl },
			std::pair{ "LeftAlt", KeyCode::LeftAlt }, std::pair{ "LeftSuper", KeyCode::LeftSuper },
			std::pair{ "RightShift", KeyCode::RightShift }, std::pair{ "RightControl", KeyCode::RightControl },
			std::pair{ "RightAlt", KeyCode::RightAlt }, std::pair{ "RightSuper", KeyCode::RightSuper },
			std::pair{ "Menu", KeyCode::Menu },
		};
	}

	void ScriptEngine::MessageCallback(const asSMessageInfo* msg, void*)
	{
		switch (msg->type)
		{
		case asMSGTYPE_ERROR:       KOI_LOG_ERROR("Script [{}:{},{}] {}", msg->section, msg->row, msg->col, msg->message); break;
		case asMSGTYPE_WARNING:     KOI_LOG_WARN("Script [{}:{},{}] {}", msg->section, msg->row, msg->col, msg->message); break;
		case asMSGTYPE_INFORMATION: KOI_LOG_DEBUG("Script [{}:{},{}] {}", msg->section, msg->row, msg->col, msg->message); break;
		}
	}

	void ScriptEngine::Init(const SubsystemInitContext&)
	{
		m_engine = ::asCreateScriptEngine();
		m_engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);

		RegisterAPI();

		KOI_LOG_DEBUG("ScriptEngine initialized");
	}

	void ScriptEngine::Shutdown()
	{
		m_modules.clear();

		if (m_engine)
		{
			m_engine->ShutDownAndRelease();
			m_engine = nullptr;
		}
	}

	void ScriptEngine::RegisterAPI()
	{
		i32 r = 0;

		::RegisterStdString(m_engine);

		// Vector3
		r = m_engine->RegisterObjectType("Vector3", sizeof(Vector3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Vector3>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Vector3", "float x", asOFFSET(Vector3, x)); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Vector3", "float y", asOFFSET(Vector3, y)); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Vector3", "float z", asOFFSET(Vector3, z)); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructVector3), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void f(const Vector3 &in)", asFUNCTION(ConstructVector3Copy), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Vector3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(ConstructVector3XYZ), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Vector3", "Vector3 opAdd(const Vector3 &in) const", asFUNCTION(Vector3Add), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Vector3", "Vector3 opSub(const Vector3 &in) const", asFUNCTION(Vector3Sub), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Vector3", "Vector3 opMul(float) const", asFUNCTION(Vector3MulScalar), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Quaternion
		r = m_engine->RegisterObjectType("Quaternion", sizeof(Quaternion), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<Quaternion>() | asOBJ_APP_CLASS_ALLFLOATS); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Quaternion", "float x", asOFFSET(Quaternion, x)); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Quaternion", "float y", asOFFSET(Quaternion, y)); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Quaternion", "float z", asOFFSET(Quaternion, z)); assert(r >= 0);
		r = m_engine->RegisterObjectProperty("Quaternion", "float w", asOFFSET(Quaternion, w)); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructQuaternion), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT, "void f(const Quaternion &in)", asFUNCTION(ConstructQuaternionCopy), asCALL_CDECL_OBJLAST); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Quaternion", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(ConstructQuaternionXYZW), asCALL_CDECL_OBJLAST); assert(r >= 0);

		// Entity - never constructed by scripts, only handed out via ScriptBehaviour::self
		r = m_engine->RegisterObjectType("Entity", 0, asOBJ_REF); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Entity", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptEntityHandle, AddRef), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectBehaviour("Entity", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptEntityHandle, Release), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "bool IsValid() const", asMETHOD(ScriptEntityHandle, IsValid), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "void Destroy() const", asMETHOD(ScriptEntityHandle, Destroy), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "string GetName() const", asMETHOD(ScriptEntityHandle, GetName), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "Vector3 GetPosition() const", asMETHOD(ScriptEntityHandle, GetPosition), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "void SetPosition(const Vector3 &in)", asMETHOD(ScriptEntityHandle, SetPosition), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "Quaternion GetRotation() const", asMETHOD(ScriptEntityHandle, GetRotation), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "void SetRotation(const Quaternion &in)", asMETHOD(ScriptEntityHandle, SetRotation), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "Vector3 GetScale() const", asMETHOD(ScriptEntityHandle, GetScale), asCALL_THISCALL); assert(r >= 0);
		r = m_engine->RegisterObjectMethod("Entity", "void SetScale(const Vector3 &in)", asMETHOD(ScriptEntityHandle, SetScale), asCALL_THISCALL); assert(r >= 0);

		// Input, under its own namespace so scripts write Input::IsKeyDown(...)
		r = m_engine->RegisterEnum("KeyCode"); assert(r >= 0);
		for (const auto& [name, value] : kKeyCodes)
		{
			r = m_engine->RegisterEnumValue("KeyCode", name, static_cast<i64>(value)); assert(r >= 0);
		}

		r = m_engine->SetDefaultNamespace("Input"); assert(r >= 0);
		r = m_engine->RegisterGlobalFunction("bool IsKeyDown(KeyCode key)", asFUNCTION(Input_IsKeyDown), asCALL_CDECL); assert(r >= 0);
		r = m_engine->RegisterGlobalFunction("bool IsKeyPressed(KeyCode key)", asFUNCTION(Input_IsKeyPressed), asCALL_CDECL); assert(r >= 0);
		r = m_engine->RegisterGlobalFunction("bool IsKeyReleased(KeyCode key)", asFUNCTION(Input_IsKeyReleased), asCALL_CDECL); assert(r >= 0);
		r = m_engine->SetDefaultNamespace(""); assert(r >= 0);
	}

	asIScriptModule* ScriptEngine::GetOrCompileModule(std::string_view scriptPath)
	{
		std::string key(scriptPath);
		if (const auto it = m_modules.find(key); it != m_modules.end())
			return it->second.Module;

		return CompileModule(key);
	}

	asIScriptModule* ScriptEngine::CompileModule(const std::string& vfsPath)
	{
		ZoneScoped;

		const std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(vfsPath);
		if (!bytes)
		{
			KOI_LOG_ERROR("Script: failed to read '{}'", vfsPath);
			return nullptr;
		}

		CScriptBuilder builder;
		if (builder.StartNewModule(m_engine, vfsPath.c_str()) < 0)
		{
			KOI_LOG_ERROR("Script: failed to start module for '{}'", vfsPath);
			return nullptr;
		}

		if (builder.AddSectionFromMemory("ScriptBehaviour", kScriptBehaviourSource.data()) < 0)
		{
			KOI_LOG_ERROR("Script: failed to add ScriptBehaviour bootstrap");
			return nullptr;
		}

		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		if (builder.AddSectionFromMemory(vfsPath.c_str(), text.data(), static_cast<u32>(text.size())) < 0)
		{
			KOI_LOG_ERROR("Script: failed to add section for '{}'", vfsPath);
			return nullptr;
		}

		if (builder.BuildModule() < 0)
		{
			KOI_LOG_ERROR("Script: build failed for '{}'", vfsPath);
			return nullptr;
		}

		ModuleEntry entry{ .Module = builder.GetModule() };
		if (const std::optional<fs::path> disk = VirtualFileSystem::Get().TryResolveToDisk(vfsPath))
		{
			entry.SourceDiskPath = *disk;
			std::error_code ec;
			entry.Stamp = fs::last_write_time(*disk, ec);
		}

		asIScriptModule* compiled = entry.Module;
		m_modules[vfsPath] = std::move(entry);

		KOI_LOG_INFO("Script: compiled '{}'", vfsPath);
		return compiled;
	}

	void ScriptEngine::SetSelfProperty(asIScriptObject& instance, Entity e)
	{
		asITypeInfo* type = instance.GetObjectType();
		const asUINT count = type->GetPropertyCount();
		for (asUINT i = 0; i < count; ++i)
		{
			const char* name = nullptr;
			type->GetProperty(i, &name);
			if (!name || std::string_view(name) != "self") continue;

			void* addr = instance.GetAddressOfProperty(i);
			*reinterpret_cast<ScriptEntityHandle**>(addr) = new ScriptEntityHandle(e.id());  // starts at refcount 1, owned by this slot
			return;
		}

		KOI_LOG_WARN("Script: '{}' has no 'self' property on ScriptBehaviour - was the bootstrap section skipped?", type->GetName());
	}

	void ScriptEngine::InstantiateScript(Entity e, ScriptComponent& sc)
	{
		if (sc.ScriptPath.empty()) return;

		asIScriptModule* compiled = GetOrCompileModule(sc.ScriptPath);
		if (!compiled) return;

		asITypeInfo* baseType = compiled->GetTypeInfoByName("ScriptBehaviour");
		if (!baseType)
		{
			KOI_LOG_ERROR("Script '{}': ScriptBehaviour bootstrap type missing", sc.ScriptPath);
			return;
		}

		asITypeInfo* derivedType = nullptr;
		const asUINT typeCount = compiled->GetObjectTypeCount();
		for (asUINT i = 0; i < typeCount; ++i)
		{
			asITypeInfo* type = compiled->GetObjectTypeByIndex(i);
			if (type != baseType && type->DerivesFrom(baseType))
			{
				derivedType = type;
				break;
			}
		}

		if (!derivedType)
		{
			KOI_LOG_ERROR("Script '{}' has no class deriving ScriptBehaviour", sc.ScriptPath);
			return;
		}

		void* obj = m_engine->CreateScriptObject(derivedType);
		if (!obj) return;

		sc.Instance = static_cast<asIScriptObject*>(obj);
		SetSelfProperty(*sc.Instance, e);

		sc.OnUpdateFn = derivedType->GetMethodByDecl("void OnUpdate(float)");
		sc.Context    = m_engine->CreateContext();

		if (asIScriptFunction* onCreate = derivedType->GetMethodByDecl("void OnCreate()"))
		{
			InvokeMethod(sc, onCreate);
		}
	}

	void ScriptEngine::DestroyInstance(ScriptComponent& sc)
	{
		if (!sc.Instance) return;

		if (asIScriptFunction* onDestroy = sc.Instance->GetObjectType()->GetMethodByDecl("void OnDestroy()"))
			InvokeMethod(sc, onDestroy);

		sc.Instance->Release();
		sc.Instance = nullptr;

		if (sc.Context)
		{
			sc.Context->Release();
			sc.Context = nullptr;
		}

		sc.OnUpdateFn = nullptr;
	}

	void ScriptEngine::InvokeMethod(ScriptComponent& sc, asIScriptFunction* fn)
	{
		if (!fn || !sc.Context) return;

		sc.Context->Prepare(fn);
		sc.Context->SetObject(sc.Instance);
		if (sc.Context->Execute() != asEXECUTION_FINISHED)
			KOI_LOG_ERROR("Script '{}': {}() did not finish", sc.ScriptPath, fn->GetName());
	}

	void ScriptEngine::InvokeMethod(ScriptComponent& sc, asIScriptFunction* fn, f32 arg0)
	{
		if (!fn || !sc.Context) return;

		sc.Context->Prepare(fn);
		sc.Context->SetObject(sc.Instance);
		sc.Context->SetArgFloat(0, arg0);
		if (sc.Context->Execute() != asEXECUTION_FINISHED)
			KOI_LOG_ERROR("Script '{}': {}() did not finish", sc.ScriptPath, fn->GetName());
	}

	void ScriptEngine::BeginPlay(World& world)
	{
		ZoneScoped;
		world.GetECS().each<ScriptComponent>([this](Entity e, ScriptComponent& sc) { InstantiateScript(e, sc); });
	}

	void ScriptEngine::EndPlay(World& world)
	{
		ZoneScoped;
		world.GetECS().each<ScriptComponent>([this](Entity, ScriptComponent& sc) { DestroyInstance(sc); });
	}

	void ScriptEngine::Update(World& world, f32 dt)
	{
		ZoneScoped;
		m_engine->GarbageCollect(asGC_ONE_STEP);

		world.GetECS().each<ScriptComponent>([this, dt](Entity, ScriptComponent& sc)
		{
			if (sc.OnUpdateFn) InvokeMethod(sc, sc.OnUpdateFn, dt);
		});
	}

	void ScriptEngine::Reload(const ReloadRequest& request)
	{
		ZoneScoped;

		std::vector<std::string> changed;
		for (const auto& [path, entry] : m_modules)
		{
			if (request.Scope == ReloadScope::All)
			{
				changed.push_back(path);
				continue;
			}

			std::error_code ec;
			const fs::file_time_type stamp = entry.SourceDiskPath.empty()
				? fs::file_time_type{}
				: fs::last_write_time(entry.SourceDiskPath, ec);

			if (ec || stamp != entry.Stamp)
				changed.push_back(path);
		}

		if (changed.empty()) return;

		// Tear every live instance from an affected module down before discarding it - the old
		// instance is unusable the moment its module is gone, so nothing here half-preserves state
		World& world = AppBase::Get().GetWorld();
		std::vector<std::pair<Entity, ScriptComponent*>> affected;
		world.GetECS().each<ScriptComponent>([&](Entity e, ScriptComponent& sc)
		{
			if (sc.Instance && std::ranges::contains(changed, sc.ScriptPath))
				affected.emplace_back(e, &sc);
		});

		for (auto& [e, sc] : affected)
			DestroyInstance(*sc);

		for (const std::string& path : changed)
		{
			m_modules.erase(path);
			m_engine->DiscardModule(path.c_str());
			KOI_LOG_INFO("Script: reloading '{}'", path);
		}

		for (auto& [e, sc] : affected)
		{
			InstantiateScript(e, *sc);
		}
	}
}

#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include <filesystem>
#include <string>
#include <unordered_map>

class asIScriptEngine;
class asIScriptModule;
class asIScriptFunction;
class asIScriptObject;
struct asSMessageInfo;

namespace koi
{
	namespace fs = std::filesystem;

	class InputManager;
	struct ScriptComponent;

	class ScriptEngine : public NonCopyable, public IReloadable
	{
	public:
		using Dependencies = TypeList<InputManager>;

		void Init(const SubsystemInitContext& ctx);
		void Shutdown();

		void BeginPlay(World& world);
		void EndPlay(World& world);
		void Update(World& world, f32 dt);

		void Reload(const ReloadRequest& request) override;

		// Compiles (or returns the cached) module for a script at a VFS path. Null on compile failure
		[[nodiscard]] asIScriptModule* GetOrCompileModule(std::string_view scriptPath);

		// Whether a script at this VFS path has a compiled module resident
		[[nodiscard]] bool IsModuleLoaded(const std::string& scriptPath) const { return m_modules.contains(scriptPath); }

	private:
		struct ModuleEntry
		{
			asIScriptModule*   Module = nullptr;
			fs::path            SourceDiskPath;
			fs::file_time_type  Stamp{};
		};

		void RegisterAPI();
		asIScriptModule* CompileModule(const std::string& vfsPath);

		void InstantiateScript(Entity e, ScriptComponent& sc);
		void DestroyInstance(ScriptComponent& sc);
		void SetSelfProperty(asIScriptObject& instance, Entity e);

		void InvokeMethod(ScriptComponent& sc, asIScriptFunction* fn);
		void InvokeMethod(ScriptComponent& sc, asIScriptFunction* fn, f32 arg0);

		static void MessageCallback(const asSMessageInfo* msg, void* param);

	private:
		asIScriptEngine* m_engine = nullptr;
		std::unordered_map<std::string, ModuleEntry> m_modules;  // keyed by VFS path
	};
}

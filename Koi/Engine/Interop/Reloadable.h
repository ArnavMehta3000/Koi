#pragma once

namespace koi
{
	enum class ReloadScope
	{
		Changed,  // only what the implementation can tell has changed on disk
		All,      // everything, for when change detection isn't enough
	};

	struct ReloadRequest
	{
		ReloadScope Scope = ReloadScope::Changed;
	};

	class IReloadable
	{
	public:
		virtual ~IReloadable() = default;

		// Implementations decide what "changed" means for their own sources, so the caller
		// never has to know which kinds of file exist
		virtual void Reload(const ReloadRequest& request) = 0;
	};
}

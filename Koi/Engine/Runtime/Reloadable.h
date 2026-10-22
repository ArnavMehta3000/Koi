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

		virtual void Reload(const ReloadRequest& request) = 0;
	};
}

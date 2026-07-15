#pragma once

namespace koi
{
	class IReloadable
	{
	public:
		virtual ~IReloadable() = default;

		virtual void Reload() = 0;
	};
}

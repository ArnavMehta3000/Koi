#pragma once
#include <tracy/tracy/Tracy.hpp>
#include "Koi/Engine/Diagnostics/FrameStats.h"

namespace koi
{
	/// HACK
	// Tracy gives every thread that emits a zone its own timeline row, and there is no way to fold
	// them back together - naming and group hints only affect ordering. So a fan-out across a
	// 25-wide thread pool buries the trace unless the workers emit nothing at all. An inactive
	// ScopedZone returns before it queues anything, so a muted thread stays invisible
	inline thread_local bool t_zonesMuted = false;

	class MuteZonesScope
	{
	public:
		MuteZonesScope()  noexcept : m_previous(t_zonesMuted) { t_zonesMuted = true; }
		~MuteZonesScope() noexcept { t_zonesMuted = m_previous; }
		MuteZonesScope(const MuteZonesScope&) = delete;
		MuteZonesScope& operator=(const MuteZonesScope&) = delete;

	private:
		bool m_previous;
	};
}

// Like ZoneScoped, but silent on threads that opted out
#define KOI_ZONE_UNMUTED ZoneNamed(___tracy_scoped_zone, !::koi::t_zonesMuted)

#define KOI_SCOPE(name) ZoneScopedN(name); ::koi::FrameStatScope ___koi_frame_stat{ name }

#define KOI_SCOPE_TRANSIENT(name) ZoneTransientN(___tracy_scoped_zone, (name).data(), true); ::koi::FrameStatScope ___koi_frame_stat{ name }

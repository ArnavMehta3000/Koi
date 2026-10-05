target("KoiProfiler")
	set_kind("headeronly")
	set_group("External")
	add_deps("ImGui", { public = true })
	add_includedirs("$(projectdir)/External/KoiProfiler", { public = true })
	add_headerfiles("$(projectdir)/External/KoiProfiler/*.h")
	add_defines(
		"KOI_PROFILER_NO_SOURCE_LOCATION",
		"KOI_PROFILER_NO_MARKERS",
		"KOI_PROFILER_NO_OVERLAY",
		{ public = true })
target_end()

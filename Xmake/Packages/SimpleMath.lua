target("SimpleMath")
    set_kind("static")
    set_group("External")
    add_files("$(projectdir)/External/SimpleMath/SimpleMath.cpp")
    add_headerfiles("$(projectdir)/External/SimpleMath/SimpleMath.h",
    				"$(projectdir)/External/SimpleMath/SimpleMath.inl")
    add_includedirs("$(projectdir)/External/SimpleMath", { public = true })
target_end()


target("koi-runtime")
    set_kind("binary")
    add_files("Runtime/Main.cpp")
    add_deps("Koi.Engine", "Sandbox")
target_end()

target("koi-editor")
    set_kind("binary")
    add_files("Editor/Main.cpp")
    add_deps("Koi.Engine", "Koi.Editor", "Sandbox")
target_end()

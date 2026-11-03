-- Project management: `xmake koi new <Name>`, `xmake koi list`, `xmake koi tracy` to open the profiler, `xmake koi check` to build + test both configs
task("koi")
	set_category("plugin")
	set_menu(
	{
		usage = "xmake koi [new|list|tracy|check] [name]",
		description = "Create and list Koi game projects, open the Tracy profiler, or build and test both configs",
		options =
		{
			{ nil, "command", "v", nil, "new   Create Projects/<name> from the template",
			                            "list  Show every discovered project",
			                            "tracy Open the Tracy viewer matching the client (profile modes)",
			                            "check Build every project + Koi.Tests in editor-debug and game-release, run the tests" },
			{ nil, "name",    "v", nil, "Project name (identifier: letters, digits, underscore)" },
		}
	})

	on_run("main")
task_end()

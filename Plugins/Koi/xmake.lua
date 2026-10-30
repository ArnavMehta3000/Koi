-- Project management: `xmake koi new <Name>`, `xmake koi list`, plus `xmake koi tracy` to open the profiler
task("koi")
	set_category("plugin")
	set_menu(
	{
		usage = "xmake koi [new|list|tracy] [name]",
		description = "Create and list Koi game projects, or open the Tracy profiler",
		options =
		{
			{ nil, "command", "v", nil, "new   Create Projects/<name> from the template",
			                            "list  Show every discovered project",
			                            "tracy Open the Tracy viewer matching the client (profile modes)" },
			{ nil, "name",    "v", nil, "Project name (identifier: letters, digits, underscore)" },
		}
	})

	on_run("main")
task_end()

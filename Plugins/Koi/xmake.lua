-- Project management: `xmake koi new <Name>`, `xmake koi list`
task("koi")
	set_category("plugin")
	set_menu(
	{
		usage = "xmake koi [new|list] [name]",
		description = "Create and list Koi game projects",
		options =
		{
			{ nil, "command", "v", nil, "new   Create Projects/<name> from the template",
			                            "list  Show every discovered project" },
			{ nil, "name",    "v", nil, "Project name (identifier: letters, digits, underscore)" },
		}
	})

	on_run("main")
task_end()

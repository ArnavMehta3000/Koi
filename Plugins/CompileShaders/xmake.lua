-- Shader compilation task
task("compile_shaders")
	set_category("plugin")
	set_menu(
	{
		usage = "xmake CompileShaders [options]",
		description = "Compile HLSL shaders using DXC",
		options =
		{
			{'o', "out_dir",     "kv", nil, "Output directory for compiled shaders"                        },
			{'s', "src_path",    "kv", nil, "Path to shader source directory"                              },
			{'c', "cfg_file",    "kv", nil, "Path to ShaderBuild.json config file"                         },
			{'t', "target_name", "kv", nil, "xmake target name (uses target's dependir for .d files)"      },
			{'f', "force",       "k",  nil, "Force recompilation of all shaders"                           },
		}
	})

	on_run("main")
task_end()

--[[
// This is a sample input JSON
{
	"common_args": [
		"-WX",
		"-Zi",
		"-Qembed_debug"
	],
	"include_dirs": [],
	"defines": [
		"DEBUG=1"
	],
	"entries": [
		{
			"source_file": "GeometryPassVS.hlsl",
			"output_file": "GeometryPassVS.cso",
			"entry": "VSMain",
			"target": "vs_6_0",
			"defines": [],
			"extra_args": []
		},
		{
			"source_file": "GeometryPassPS.hlsl",
			"output_file": "GeometryPassPS.cso",
			"entry": "PSMain",
			"target": "ps_6_0",
			"defines": [],
			"extra_args": []
		},
	]
}
]]

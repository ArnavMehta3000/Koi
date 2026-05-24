-- Generate RDI file using raddbg/radbin
rule("GenerateRDI")
	after_build(function(target)
		if target:kind() == "binary"  and  (target:get("mode") ~= "release")then

	    	import("lib.detect.find_program")

		    local program = find_program("radbin")
			if program then
	    		local cmd = program .. " --rdi " .. target:targetfile()
				os.exec(cmd)
			end
		end
    end)
rule_end()

-- Add natvis files from packages
rule("Natvis")
    on_config(function (target)
        for name, pkg in pairs(target:pkgs()) do
            local includedir = path.join(pkg:installdir(), "include")
            local natvis = os.files(path.join(includedir, "**.natvis"))
            if #natvis > 0 then
                local groups = table.wrap(target:get("filegroups"))
                if not table.find(groups, name) then
                    target:add("extrafiles", natvis, { install = false })
                    target:add("filegroups", path.join("nativs", name), { rootdir = includedir })
                end
            end
        end
    end)
rule_end()

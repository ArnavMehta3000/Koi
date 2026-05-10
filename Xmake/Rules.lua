-- Generate RDI file using raddbg/radbin
rule("GenerateRDI")
	after_build(function(target)
		if target:kind() == "binary" then

	    	import("lib.detect.find_program")

		    local program = find_program("radbin")
			if program then
	    		local cmd = program .. " --rdi " .. target:targetfile()
				os.exec(cmd)
			end
		end
    end)
rule_end()

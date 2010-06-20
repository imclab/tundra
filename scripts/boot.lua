
-- Set up the package path based on the script path first thing so we can require() stuff.
local cmdline_args = ...
TundraRootDir = assert(cmdline_args[1])
do
	local package = require("package")
	package.path = string.format("%s/scripts/?.lua;%s/lua/etc/?.lua", TundraRootDir, TundraRootDir)
end

function printf(msg, ...)
	local str = string.format(msg, ...)
	print(str)
end

-- Use "strict" when developing to flag accesses to nil global variables
require("strict")

local util = require("tundra.util")

-- Parse the command line options.
do
	local message = nil
	local option_blueprints = {
		{ Name="Help", Short="h", Long="help", Doc="This message" },
		{ Name="Verbose", Short="v", Long="verbose", Doc="Be verbose" },
		{ Name="DryRun", Short="n", Long="dry-run", Doc="Don't execute any actions" },
		{ Name="WriteGraph", Long="dep-graph", Doc="Generate dependency graph" },
		{ Name="GraphEnv", Long="dep-graph-env", Doc="Include environments in dependency graph" },
		{ Name="GraphFilename", Long="dep-graph-filename", Doc="Dependency graph filename", HasValue=true },
		{ Name="SelfTest", Long="self-test", Doc="Perform self-tests" },
	}
	Options, Targets, message = util.ParseCommandline(cmdline_args, option_blueprints)
	if message then
		io.write(message)
		return 1
	end

	if #Targets == 0 then
		table.insert(Targets, "All")
	end

	if Options.Help then
		io.write("Tundra Build Processor, v0.0.5\n\nCommand-line options:\n")
		for _, bp in ipairs(option_blueprints) do
			local l = string.format("  %- 3s %- 25s %s\n",
			bp.Short and "-"..bp.Short or "",
			bp.Long and "--"..bp.Long or "",
			bp.Doc or "")
			io.write(l)
		end
		return 0
	end
end

local environment = require("tundra.environment")

if Options.Verbose then
	print("Options:")
	for k, v in pairs(Options) do
		print(k, v)
	end
	print("Targets:", table.concat(Targets, ", "))
end

DefaultEnvironment = environment.Create()
DefaultEnvironment:SetMany {
	["RM"] = "rm -f",
	["RMCOM"] = "$(RM) $(<)",
	["LIBPATH"] = "",
	["LIBCOM"] = "$(LIB) $(LIBFLAGS) $(@) $(<)",
	["LIB"] = "ar",
	["LIBFLAGS"] = "-ru",
	["LIBSUFFIX"] = ".a",
	["OBJECTDIR"] = "tundra-output",
	["OBJECTSUFFIX"] = ".o",
	["CC"] = "gcc",
	["CFLAGS"] = "",
	["C++FLAGS"] = "",
	["C++"] = "g++",
	["CPPPATH"] = "",
	["CCCOM"] = "$(CC) $(CFLAGS) $(CPPPATH:p-I) -c -o $(@) $(<)",
	["CPPCOM"] = "$(C++) $(C++FLAGS) -c -o $(@) $(<)",
	["PROGSUFFIX"] = ".exe",
	["PROGFLAGS"] = "",
	["PROGCOM"] = "$(CC) -o $(@) $(PROGFLAGS) $(<)",
	["CSC"] = "csc.exe",
	["CSCFLAGS"] = "/nologo /optimize+ /debug- /warn:4",
	["CSCEXECOM"] = "$(CSC) /out:$(@) /target:exe $(CSCFLAGS) $(<)",
}

-- Initialize tools
do
	local chunk = loadfile(TundraRootDir .. "/scripts/tools.lua")
	chunk()
end

function RunBuildScript(fn)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = _G
	setmetatable(script_globals, script_globals_mt)

	local chunk = assert(loadfile(fn))
	setfenv(chunk, script_globals)

	local function stack_dumper(err_obj)
		local debug = require("debug")
		return debug.traceback(err_obj, 2)
	end

	local function args_stub()
		return chunk()
	end

	local success, result = xpcall(args_stub, stack_dumper)

	if not success then
		io.stderr:write(result)
		error("failure")
	else
		return result
	end
end

-- RunBuildScript("tundra.lua")

local native = require("tundra.native")

native_engine = native:make_engine()

function Glob(directory, pattern)
	local result = {}
	for dir, dirs, files in native.walk_path(directory) do
		util.FilterInPlace(files, function (val) return string.match(val, pattern) end)
		for _, fn in ipairs(files) do
			table.insert(result, dir .. '/' .. fn)
		end
	end
	return result
end

local function PrintTree(n, level)
	if not level then level = 0 end
	local indent = string.rep("    ", level)
	printf("%s=> %s [pass: %s]", indent, n:GetAnnotation(), n.pass.Name)
	printf("%scmd: %s", indent, n:GetAction())
	for _, fn in util.NilIPairs(n:GetInputFiles()) do printf("%s   [ input: %s ]", indent, fn) end
	for _, fn in util.NilIPairs(n:GetOutputFiles()) do printf("%s   [ output: %s ]", indent, fn) end
	for _, dep in util.NilIPairs(n:GetDependencies()) do
		PrintTree(dep, level + 1)
	end
end

function Build(node)
	--if Options.Verbose then
	--	PrintTree(node)
	--end
	native_engine:build(node)
end

RunBuildScript("tundra.lua")

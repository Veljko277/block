target_family = os.getenv("TARGET_FAMILY")
if target_family then
	family = target_family
end
target_platform = os.getenv("TARGET_PLATFORM")
if target_platform then
	platform = target_platform
end
target_arch = os.getenv("TARGET_ARCH")
if target_arch then
	arch = target_arch
end

Import("configure.lua")
Import("other/mysql/mysql.lua")

--- Setup Config -------
config = NewConfig()
config:Add(OptCCompiler("compiler"))
config:Add(OptTestCompileC("stackprotector", "int main(){return 0;}", "-fstack-protector -fstack-protector-all"))
config:Add(OptTestCompileC("minmacosxsdk", "int main(){return 0;}", "-mmacosx-version-min=10.7 -isysroot /Developer/SDKs/MacOSX10.7.sdk"))
config:Add(OptTestCompileC("macosxppc", "int main(){return 0;}", "-arch ppc"))
config:Add(OptLibrary("zlib", "zlib.h", false))
config:Add(Mysql.OptFind("mysql", false))
config:Add(OptString("websockets", false))
config:Finalize("config.lua")

-- data compiler
function Script(name)
	return "python " .. name
end

function CHash(output, ...)
	local inputs = TableFlatten({...})

	output = Path(output)

	-- compile all the files
	local cmd = Script("scripts/cmd5.py") .. " "
	for index, inname in ipairs(inputs) do
		cmd = cmd .. Path(inname) .. " "
	end

	cmd = cmd .. " > " .. output

	AddJob(output, "cmd5 " .. output, cmd)
	for index, inname in ipairs(inputs) do
		AddDependency(output, inname)
	end
	AddDependency(output, "scripts/cmd5.py")
	return output
end

function ResCompile(scriptfile)
	windres = os.getenv("WINDRES")
	if not windres then
		windres = "windres"
	end

	scriptfile = Path(scriptfile)
	if config.compiler.driver == "cl" then
		output = PathBase(scriptfile) .. ".res"
		AddJob(output, "rc " .. scriptfile, "rc /fo " .. output .. " " .. scriptfile)
	elseif config.compiler.driver == "gcc" then
		output = PathBase(scriptfile) .. ".coff"
		AddJob(output, windres .. " " .. scriptfile, windres .. " -i " .. scriptfile .. " -o " .. output)
	end

	AddDependency(output, scriptfile)
	return output
end

function Dat2c(datafile, sourcefile, arrayname)
	datafile = Path(datafile)
	sourcefile = Path(sourcefile)

	AddJob(
		sourcefile,
		"dat2c " .. PathFilename(sourcefile) .. " = " .. PathFilename(datafile),
		Script("scripts/dat2c.py").. "\" " .. sourcefile .. " " .. datafile .. " " .. arrayname
	)
	AddDependency(sourcefile, datafile)
	return sourcefile
end

function ContentCompile(action, output)
	output = Path(output)
	AddJob(
		output,
		action .. " > " .. output,
		Script("datasrc/compile.py") .. " " .. action .. " > " .. Path(output)
	)
	AddDependency(output, Path("datasrc/content.py")) -- do this more proper
	AddDependency(output, Path("datasrc/network.py"))
	AddDependency(output, Path("datasrc/compile.py"))
	AddDependency(output, Path("datasrc/datatypes.py"))
	return output
end

-- Content Compile
network_source = ContentCompile("network_source", "src/game/generated/protocol.cpp")
network_header = ContentCompile("network_header", "src/game/generated/protocol.h")
server_content_source = ContentCompile("server_content_source", "src/game/generated/server_data.cpp")
server_content_header = ContentCompile("server_content_header", "src/game/generated/server_data.h")

AddDependency(network_source, network_header)
AddDependency(server_content_source, server_content_header)

nethash = CHash("src/game/generated/nethash.cpp", "src/engine/shared/protocol.h", "src/game/generated/protocol.h", "src/game/tuning.h", "src/game/gamecore.cpp", network_header)

server_link_other = {}
server_sql_depends = {}


function Intermediate_Output(settings, input)
	return "objs/" .. string.sub(PathBase(input), string.len("src/")+1) .. settings.config_ext
end

function build(settings)
	-- apply compiler settings
	config.compiler:Apply(settings)

	--settings.objdir = Path("objs")
	settings.cc.Output = Intermediate_Output

	cc = os.getenv("CC")
	if cc then
		settings.cc.exe_c = cc
	end
	cxx = os.getenv("CXX")
	if cxx then
		settings.cc.exe_cxx = cxx
		settings.link.exe = cxx
		settings.dll.exe = cxx
	end
	cflags = os.getenv("CFLAGS")
	if cflags then
		settings.cc.flags:Add(cflags)
	end
	ldflags = os.getenv("LDFLAGS")
	if ldflags then
		settings.link.flags:Add(ldflags)
	end

	if config.websockets.value then
		settings.cc.defines:Add("WEBSOCKETS")
	end

	if config.compiler.driver == "cl" then
		settings.cc.flags:Add("/wd4244")
		settings.cc.flags:Add("/EHsc")
	else
		settings.cc.flags:Add("-Wall")
		settings.cc.flags_cxx:Add("-std=c++11")
		if config.stackprotector.value == 1 then
			settings.cc.flags:Add("-fstack-protector", "-fstack-protector-all")
			settings.link.flags:Add("-fstack-protector", "-fstack-protector-all")
		end
	end

	settings.cc.includes:Add("src")
	settings.cc.includes:Add("src/engine/external")

	-- set some platform specific settings
	if family == "unix" then
		settings.link.libs:Add("pthread")

		if platform == "solaris" then
			settings.link.flags:Add("-lsocket")
			settings.link.flags:Add("-lnsl")
		end

		if platform == "linux" then
			settings.link.libs:Add("rt") -- clock_gettime for glibc < 2.17
		end
	end

	-- compile zlib if needed
	if config.zlib.value == 1 then
		settings.link.libs:Add("z")
		if config.zlib.include_path then
			settings.cc.includes:Add(config.zlib.include_path)
		end
		zlib = {}
	else
		zlib = Compile(settings, Collect("src/engine/external/zlib/*.c"))
		settings.cc.includes:Add("src/engine/external/zlib")
	end

	-- build the small libraries
	-- wavpack = Compile(settings, Collect("src/engine/external/wavpack/*.c"))
	-- pnglite = Compile(settings, Collect("src/engine/external/pnglite/*.c"))
	jsonparser = Compile(settings, Collect("src/engine/external/json-parser/*.c"))
	md5 = Compile(settings, "src/engine/external/md5/md5.c")
	if config.websockets.value then
		libwebsockets = Compile(settings, Collect("src/engine/external/libwebsockets/*.c"))
	end

	-- build game components
	engine_settings = settings:Copy()
	server_settings = engine_settings:Copy()
	launcher_settings = engine_settings:Copy()


	if family == "unix" and platform == "linux" then
		engine_settings.link.libs:Add("dl")
		server_settings.link.libs:Add("dl")
		launcher_settings.link.libs:Add("dl")
	end

	engine = Compile(engine_settings, Collect("src/engine/shared/*.cpp", "src/base/*.c"))
	server = Compile(server_settings, Collect("src/engine/server/*.cpp"))

	game_shared = Compile(settings, Collect("src/game/*.cpp"), nethash, network_source)
	game_server = Compile(settings, CollectRecursive("src/game/server/*.cpp"), server_content_source)

	server_exe = Link(server_settings, "teeworlds_srv", engine, server,
		game_shared, game_server, zlib, server_link_other, libwebsockets, md5)

	serverlaunch = {}

	-- make targets
	if string.find(settings.config_name, "sql") then
		s = PseudoTarget("server".."_"..settings.config_name, server_exe, serverlaunch, server_sql_depends)
	else
		s = PseudoTarget("server".."_"..settings.config_name, server_exe, serverlaunch)
	end
	g = PseudoTarget("game".."_"..settings.config_name, server_exe)

	v = PseudoTarget("versionserver".."_"..settings.config_name, versionserver_exe)
	m = PseudoTarget("masterserver".."_"..settings.config_name, masterserver_exe)
	t = PseudoTarget("tools".."_"..settings.config_name, tools)
	p = PseudoTarget("twping".."_"..settings.config_name, twping_exe)

	all = PseudoTarget(settings.config_name, s, v, m, t, p)
	return all
end


debug_settings = NewSettings()
debug_settings.config_name = "debug"
debug_settings.config_ext = "_d"
debug_settings.debug = 1
debug_settings.optimize = 0
debug_settings.cc.defines:Add("CONF_DEBUG")

debug_sql_settings = NewSettings()
debug_sql_settings.config_name = "sql_debug"
debug_sql_settings.config_ext = "_sql_d"
debug_sql_settings.debug = 1
debug_sql_settings.optimize = 0
debug_sql_settings.cc.defines:Add("CONF_DEBUG", "CONF_SQL")

release_settings = NewSettings()
release_settings.config_name = "release"
release_settings.config_ext = ""
release_settings.debug = 0
release_settings.optimize = 1
release_settings.cc.defines:Add("CONF_RELEASE")

release_sql_settings = NewSettings()
release_sql_settings.config_name = "sql_release"
release_sql_settings.config_ext = "_sql"
release_sql_settings.debug = 0
release_sql_settings.optimize = 1
release_sql_settings.cc.defines:Add("CONF_RELEASE", "CONF_SQL")

config.mysql:Apply(debug_sql_settings)
config.mysql:Apply(release_sql_settings)

build(debug_settings)
build(debug_sql_settings)
build(release_settings)
build(release_sql_settings)
DefaultTarget("game_debug")

--[[--
Power Collage installer for DaVinci Resolve.

Run it inside Resolve: Workspace > Console, switch to Lua and enter
    dofile("/path/to/install.lua")
or drag this file onto the Nodes view of the Fusion page.

Installs the Fusion effect (no admin rights needed) and, on macOS, the
OpenFX plugin (asks for the admin password). Files are taken from the
repository folder next to this script; if they are missing, they are
downloaded from GitHub, so the script also works on its own.
--]]--

local REPO_URL = "https://raw.githubusercontent.com/schmodderr/DaVinci-Resolve-Power-Collage/main/"
local OFX_TARGET = "/Library/OFX/Plugins/PowerCollage.ofx.bundle"

local FUSION_FILES = {
	{ src = "Fuses/PowerCollage.fuse", dst = "Fuses/PowerCollage.fuse" },
	{ src = "Templates/Edit/Effects/Power Collage.setting", dst = "Templates/Edit/Effects/Power Collage.setting" },
}
local OFX_FILES = {
	"Plugin/PowerCollage.ofx.bundle/Contents/MacOS/PowerCollage.ofx",
	"Plugin/PowerCollage.ofx.bundle/Contents/_CodeSignature/CodeResources",
}

local fusion = fu or app or fusion
local isWindows = package.config:sub(1, 1) == "\\"
local isMac = (jit and jit.os == "OSX") or (not isWindows and io.popen("uname -s"):read("*l") == "Darwin")

local function scriptDir()
	local source = debug.getinfo(1, "S").source
	if source:sub(1, 1) == "@" then
		return source:sub(2):match("^(.*)[/\\]")
	end
	return nil
end

local function fileExists(path)
	local f = io.open(path, "rb")
	if f then f:close() end
	return f ~= nil
end

local function parentDir(path)
	return path:match("^(.*)[/\\]")
end

local function makeDirs(path)
	if isWindows then
		os.execute('mkdir "' .. path .. '" 2>nul')
	else
		os.execute("mkdir -p '" .. path .. "'")
	end
end

local function copyFile(src, dst)
	local input = io.open(src, "rb")
	if not input then return false, "nicht lesbar: " .. src end
	local data = input:read("*a")
	input:close()
	makeDirs(parentDir(dst))
	local output = io.open(dst, "wb")
	if not output then return false, "nicht schreibbar: " .. dst end
	output:write(data)
	output:close()
	return true
end

-- os.execute returns 0 in Lua 5.1 / LuaJIT and true in Lua 5.2+.
local function run(cmd)
	local result = os.execute(cmd)
	return result == 0 or result == true
end

local function shellQuote(s)
	return "'" .. s:gsub("'", "'\\''") .. "'"
end

local function tempDir()
	local path
	if isWindows then
		path = os.getenv("TEMP") .. "\\PowerCollage" .. os.time()
	else
		path = os.tmpname()
		os.remove(path)
	end
	makeDirs(path)
	return path
end

local function download(relPath, dst)
	makeDirs(parentDir(dst))
	local url = REPO_URL .. relPath:gsub(" ", "%%20")
	local cmd
	if isWindows then
		cmd = 'curl -fsSL -o "' .. dst .. '" "' .. url .. '"'
	else
		cmd = "curl -fsSL -o " .. shellQuote(dst) .. " " .. shellQuote(url)
	end
	if not run(cmd) or not fileExists(dst) then
		return false, "Download fehlgeschlagen: " .. url
	end
	return true
end

-- Local repository folder if it has all files, otherwise a temp folder
-- filled from GitHub.
local function resolveSource(relPaths, forceDownload)
	local dir = scriptDir()
	if dir and not forceDownload then
		local complete = true
		for _, rel in ipairs(relPaths) do
			if not fileExists(dir .. "/" .. rel) then complete = false end
		end
		if complete then return dir end
	end
	local tmp = tempDir()
	for _, rel in ipairs(relPaths) do
		local ok, err = download(rel, tmp .. "/" .. rel)
		if not ok then return nil, err end
	end
	return tmp
end

local function installFusionEffect(forceDownload)
	local userData = fusion and fusion:MapPath("UserData:")
	if not userData or userData == "" or userData == "UserData:" then
		return false, "Fusion-Ordner nicht gefunden"
	end
	local rels = {}
	for _, f in ipairs(FUSION_FILES) do rels[#rels + 1] = f.src end
	local source, err = resolveSource(rels, forceDownload)
	if not source then return false, err end
	for _, f in ipairs(FUSION_FILES) do
		local ok, copyErr = copyFile(source .. "/" .. f.src, userData .. f.dst)
		if not ok then return false, copyErr end
	end
	return true, "Fusion-Effekt installiert in " .. userData
end

local function installOpenFX(forceDownload)
	if not isMac then
		return false, "Das OpenFX-Plugin gibt es bisher nur für macOS"
	end
	local source, err = resolveSource(OFX_FILES, forceDownload)
	if not source then return false, err end

	-- /Library/OFX/Plugins belongs to root, so the copy runs through the
	-- macOS admin password dialog.
	local bundle = source .. "/Plugin/PowerCollage.ofx.bundle"
	local lines = {
		"set -e",
		"mkdir -p /Library/OFX/Plugins",
		"rm -rf " .. shellQuote(OFX_TARGET),
		"mkdir -p " .. shellQuote(OFX_TARGET .. "/Contents/MacOS") .. " " .. shellQuote(OFX_TARGET .. "/Contents/_CodeSignature"),
		"cp " .. shellQuote(bundle .. "/Contents/MacOS/PowerCollage.ofx") .. " " .. shellQuote(OFX_TARGET .. "/Contents/MacOS/PowerCollage.ofx"),
		"cp " .. shellQuote(bundle .. "/Contents/_CodeSignature/CodeResources") .. " " .. shellQuote(OFX_TARGET .. "/Contents/_CodeSignature/CodeResources"),
		"chmod 755 " .. shellQuote(OFX_TARGET .. "/Contents/MacOS/PowerCollage.ofx"),
		-- Downloaded files are quarantined and macOS would refuse to load the plugin.
		"xattr -dr com.apple.quarantine " .. shellQuote(OFX_TARGET) .. " 2>/dev/null || true",
	}
	local script = os.tmpname()
	local f = io.open(script, "w")
	f:write(table.concat(lines, "\n") .. "\n")
	f:close()
	local ok = run("osascript -e 'do shell script \"/bin/sh " .. script .. "\" with administrator privileges' >/dev/null 2>&1")
	os.remove(script)
	if not ok or not fileExists(OFX_TARGET .. "/Contents/MacOS/PowerCollage.ofx") then
		return false, "OpenFX-Plugin nicht installiert (Passwortabfrage abgebrochen?)"
	end
	return true, "OpenFX-Plugin installiert in " .. OFX_TARGET
end

local function install(options)
	local messages = {}
	local allOk = true
	if options.fusion then
		local ok, msg = installFusionEffect(options.forceDownload)
		allOk = allOk and ok
		messages[#messages + 1] = msg
	end
	if options.ofx then
		local ok, msg = installOpenFX(options.forceDownload)
		allOk = allOk and ok
		messages[#messages + 1] = msg
	end
	if allOk and #messages > 0 then
		messages[#messages + 1] = "Bitte DaVinci Resolve neu starten."
	end
	return allOk, table.concat(messages, "\n")
end

-- Without a UI, e.g. for automated tests: set POWER_COLLAGE_AUTO = { fusion = true, ofx = false } before dofile.
if type(POWER_COLLAGE_AUTO) == "table" then
	local ok, msg = install(POWER_COLLAGE_AUTO)
	print(msg)
	POWER_COLLAGE_RESULT = { ok = ok, message = msg }
	return
end

local ui = fusion.UIManager
local disp = bmd.UIDispatcher(ui)
local win = disp:AddWindow({
	ID = "PowerCollageInstaller",
	WindowTitle = "Power Collage installieren",
	Geometry = { 300, 300, 460, 230 },
	ui:VGroup({
		ui:Label({ Text = "Was soll installiert werden?", Weight = 0 }),
		ui:CheckBox({ ID = "Fusion", Text = "Fusion-Effekt (Effects > Fusion Effects)", Checked = true, Weight = 0 }),
		ui:CheckBox({
			ID = "OFX",
			Text = isMac and "OpenFX-Plugin (Effects > Open FX), braucht das Admin-Passwort" or "OpenFX-Plugin (nur macOS)",
			Checked = isMac,
			Enabled = isMac,
			Weight = 0,
		}),
		ui:Label({ ID = "Status", Text = "", WordWrap = true, Weight = 1 }),
		ui:HGroup({
			Weight = 0,
			ui:HGap(0, 1),
			ui:Button({ ID = "Close", Text = "Schließen" }),
			ui:Button({ ID = "Install", Text = "Installieren", Default = true }),
		}),
	}),
})
local items = win:GetItems()

function win.On.Install.Clicked(ev)
	items.Install.Enabled = false
	items.Status.Text = "Installiere ..."
	local ok, msg = install({ fusion = items.Fusion.Checked, ofx = items.OFX.Checked })
	items.Status.Text = (ok and "" or "Fehler: ") .. msg
	items.Install.Enabled = true
end

function win.On.Close.Clicked(ev)
	disp:ExitLoop()
end

function win.On.PowerCollageInstaller.Close(ev)
	disp:ExitLoop()
end

win:Show()
disp:RunLoop()
win:Hide()

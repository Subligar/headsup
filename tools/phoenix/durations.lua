-- Write data/phoenix_spell_durations.tsv: how long each spell's status lasts on Phoenix, from the map server's own
-- duration functions with Phoenix's module overrides applied, for a caster at the skill cap with no gear, merits or
-- job abilities. A random duration is taken at its longest. Only spells whose script calls the shared enfeebling, enhancing,
-- song or ninjutsu routine are written; the rest have their own code.
--
-- Usage: luajit durations.lua SERVER_DIR LIVE_SETTINGS OUT_DIR (LuaJIT, as the server: the overrides need setfenv)
local server, liveSettings, out = arg[1], arg[2], arg[3]
if not out then
    io.stderr:write('usage: luajit durations.lua SERVER_DIR LIVE_SETTINGS OUT_DIR\n')
    os.exit(2)
end
package.path = server .. '/?.lua;' .. package.path

local function files(dir)
    local list = {}
    local pipe = assert(io.popen('ls "' .. dir .. '"'))
    for name in pipe:lines() do
        list[#list + 1] = dir .. '/' .. name
    end
    pipe:close()
    return list
end

local function read(path)
    local file = io.open(path)
    if not file then return nil end
    local text = file:read('*a')
    file:close()
    return text
end

xi = {}
for _, path in ipairs(files(server .. '/scripts/enum')) do
    if path:match('%.lua$') then dofile(path) end
end
dofile(server .. '/settings/default/main.lua')
dofile(liveSettings)
require('modules/module_utils')

math.randomInt = function(low, high) return high end
math.randomFloat = function(low, high) return high end

-- Each skill's cap: the most any job has at the level cap, from the server's skill ranks and caps.
local skillCaps = {}
do
    local section, ranks, caps = nil, {}, {}
    for line in io.lines(server .. '/data/skills.yaml') do
        section = line:match('^  (%a+):%s*$') or section
        local key, body = line:match('^    ([%w_]+):%s*{(.*)}')
        if key and section == 'ranks' then ranks[key] = body end
        if key and section == 'caps' then caps[tonumber(key)] = body end
    end
    local cap = caps[xi.settings.main.MAX_LEVEL] or error('skills.yaml has no caps at the level cap')
    for name, skill in pairs(xi.skill) do
        local most = 0
        for rank in (ranks[name:lower()] or ''):gmatch('%a+:%s*([%w_]+)') do
            most = math.max(most, tonumber(cap:match('%f[%w_]' .. rank .. ':%s*(%d+)')))
        end
        skillCaps[skill] = most
    end
end

-- Anyone at all: every number 0 but skills at their cap, every question no, and a level no spell is above.
local kLevel = 99
local function nobody(id)
    return setmetatable({}, { __index = function(_, method)
        if method == 'getID' then return function() return id end end
        if method == 'getMainLvl' then return function() return kLevel end end
        if method == 'getSkillLevel' then return function(_, skill) return skillCaps[skill] or 0 end end
        if method:match('^has') or method:match('^is') then return function() return false end end
        if method == 'getStatusEffect' then return function() return nil end end
        return function() return 0 end
    end })
end
local caster, target = nobody(1), nobody(2)

local families =
{
    { file = 'enfeebling_spell', use = 'useEnfeeblingSpell' },
    { file = 'enhancing_spell', use = 'useEnhancingSpell' },
    { file = 'enfeebling_song', use = 'useEnfeeblingSong' },
    { file = 'enhancing_song', use = 'useEnhancingSong' },
    { file = 'enhancing_ninjutsu', use = 'useEnhancingNinjutsu' },
}
for _, family in ipairs(families) do
    dofile(server .. '/scripts/globals/spells/' .. family.file .. '.lua')
end

-- What the server's module loader does with the enabled modules (init.txt, in order) that change these routines.
for line in io.lines(server .. '/modules/init.txt') do
    local dir = line:match('^%s*([^#%s][^%s]-)/?%s*$')
    for _, family in ipairs(families) do
        local path = dir and server .. '/modules/' .. dir .. '/globals/spells/' .. family.file .. '.lua'
        if path and read(path) then dofile(path) end
    end
end
for _, module in ipairs(xi.module.registry) do
    if module.enabled then
        for _, override in ipairs(module.overrides) do
            if override.enabled then
                local owner, name = override.name:match('^(.*)%.([^.]+)$')
                local base = _G
                for part in owner:gmatch('[^.]+') do base = base[part] end
                applyOverride(base, name, override.func, override.name, module.name)
            end
        end
    end
end

-- A table the server keeps local, read off the routine that uses it.
local function upvalue(func, wanted)
    for i = 1, math.huge do
        local name, value = debug.getupvalue(func, i)
        if name == nil then error('no ' .. wanted .. ' beside the routine') end
        if name == wanted then return value end
    end
end
local function baseOf(func)
    while true do
        local env = getfenv(func)
        if rawget(env, 'super') == nil then return func end
        func = rawget(env, 'super')
    end
end

local spells = xi.spells
local enfeebling = upvalue(baseOf(spells.enfeebling.calculateDuration), 'pTable')
local songs = upvalue(baseOf(spells.enfeebling.useEnfeeblingSong), 'pTable')
local spellStub = setmetatable({}, { __index = function() return function() return 0 end end })
local durations =
{
    enfeebling_spell = function(id)
        return spells.enfeebling.calculateDuration(caster, target, id, enfeebling[id][1], xi.skill.ENFEEBLING_MAGIC)
    end,
    enhancing_spell = function(id)
        local row = spells.enhancing.spellPTable[id]
        return spells.enhancing.calculateEnhancingDuration(caster, target, spellStub, id, xi.magic.spellGroup.WHITE, row[2])
    end,
    enfeebling_song = function(id)
        local row = songs[id]
        return spells.enfeebling.calculateSongDuration(caster, row[1], row[5], 0)
    end,
    enhancing_song = function(id)
        return spells.enhancing.calculateSongDuration(caster, target, spellStub, 0, nil)
    end,
    enhancing_ninjutsu = function(id)
        return spells.enhancing.ninjutsuPTable[id][4]
    end,
}
local tables =
{
    enfeebling_spell = enfeebling,
    enhancing_spell = spells.enhancing.spellPTable,
    enfeebling_song = songs,
    enhancing_song = spells.enhancing.songPTable,
    enhancing_ninjutsu = spells.enhancing.ninjutsuPTable,
}

local scripts = {}
for _, group in ipairs(files(server .. '/scripts/actions/spells')) do
    for _, path in ipairs(files(group)) do
        scripts[path:match('([^/]+)%.lua$') or ''] = path
    end
end
local names = {}
for name, id in pairs(xi.magic.spell) do names[id] = name:lower() end

local rows = {}
for _, family in ipairs(families) do
    for id in pairs(tables[family.file]) do
        local script = scripts[names[id] or '']
        local text = script and read(script)
        if text and text:find(family.use, 1, true) then
            local seconds = math.floor(durations[family.file](id))
            if seconds > 0 then rows[#rows + 1] = { id, seconds } end
        end
    end
end
table.sort(rows, function(a, b) return a[1] < b[1] end)
if #rows == 0 then
    io.stderr:write('durations: no spell gave a duration\n')
    os.exit(1)
end

local file = assert(io.open(out .. '/phoenix_spell_durations.tsv', 'w'))
file:write('spell\tseconds\n')
for _, row in ipairs(rows) do file:write(row[1], '\t', row[2], '\n') end
file:close()
print(string.format('durations: %d spells -> %s', #rows, out))

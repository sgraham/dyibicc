-- Cache library functions.
local type, pairs, ipairs = type, pairs, ipairs
local pcall, error, assert = pcall, error, assert
local _s = string
local sub, match, gmatch, gsub = _s.sub, _s.match, _s.gmatch, _s.gsub
local format, rep, upper = _s.format, _s.rep, _s.upper
local _t = table
local insert, remove, concat, sort = _t.insert, _t.remove, _t.concat, _t.sort
local execute = os.execute
local tmpnam = os.tmpnam
local io = io
local stdin, stdout, stderr = io.stdin, io.stdout, io.stderr

------------------------------------------------------------------------------

local g_fname
local g_command_line_torun
local g_expected_rv
local g_expected_text = ''
local g_disabled = false

------------------------------------------------------------------------------

local function doline(line)
  local prefix, run = match(line, "^(// RUN: )(.*)$")
  if run then
    g_command_line_torun = run:gsub("{self}", g_fname)
  end
  local prefix, ret = match(line, "^(// RET: )(.*)$")
  if ret then
    g_expected_rv = tonumber(ret)
  end
  local prefix, txt = match(line, "^(// TXT: )(.*)$")
  if txt then
    g_expected_text = g_expected_text .. txt:gsub("{self}", g_fname) .. '\n'
  end
  local disabled = match(line, "^(// DISABLED)$")
  if disabled then
    g_disabled = true
  end
end

-- Read input file.
readtestdirectives = function(fin)
  for line in fin:lines() do
    local ok, err = pcall(doline, line)
    if not ok and wprinterr(err, "\n") then return true end
  end

  -- Close input file.
  assert(fin == stdin or fin:close())
end

local function runtest(platform, infile)
  stdout:write("\n", infile, "\n")

  g_fname = infile
  local fin = assert(io.open(infile, "r"))
  readtestdirectives(fin)

  if g_disabled then
    stdout:write(" => skipped\n")
    return 0
  end

  if g_command_line_torun == nil then
    stdout:write("no // RUN: found\n")
    return 2
  end
  if g_expected_rv == nil then
    stdout:write("no // RET: found")
    return 3
  end
  if platform == 'linux' then
    g_expected_rv = g_expected_rv * 256
  end

  local cc
  if platform == 'win' then
    cc = 'dyibicc.exe'
  elseif platform == 'linux' then
    cc = './dyibicc'
  end
  local rv
  if g_expected_text ~= '' then
    local redir_name = tmpnam()
    rv = execute(cc .. ' ' .. g_command_line_torun .. '>' .. redir_name)
    ftestout = assert(io.open(redir_name))
    local testgot = ftestout:read("*all")
    os.remove(redir_name)
    if testgot ~= g_expected_text then
      stdout:write('output was:\n', testgot, '\nbut expected:\n', g_expected_text, '\n')
      return 1
    end
    for line in ftestout:lines() do
    end
  else
    rv = execute(cc .. ' ' .. g_command_line_torun)
  end

  if rv ~= g_expected_rv then
    stdout:write("got rc=", rv, " but expected rc=", g_expected_rv)
    return 1
  end

  return 0
end

local function handleargs(args)
  local infile = nil
  local platform = nil
  if #args + 1 >= 2 then
    platform = args[1]
    if platform ~= 'win' and platform ~= 'linux' then
      stdout:write('first argument expected to be win or linux')
      return 1
    end

    if #args + 1 >= 3 then
      infile = args[2]
    end
  end

  if infile then
    return runtest(platform, infile)
  else
    local redirtmp = tmpnam()
    if platform == 'win' then
      execute('dir test\\*.c /b > ' .. redirtmp)
    elseif platform == 'linux' then
      execute('ls -pa test/*.c > ' .. redirtmp)
    end
    local ftmp = assert(io.open(redirtmp))
    for line in ftmp:lines() do
      g_command_line_torun = nil
      g_expected_rv = nil
      g_expected_text = ''
      g_disabled = false
      local res
      if platform == 'win' then
        res = runtest(platform, 'test/' .. line)
      elseif platform == 'linux' then
        res = runtest(platform, line)
      end
      if res ~= 0 then
        os.remove(redirtmp)
        return res
      end
    end
    os.remove(redirtmp)
    return 0
  end
end

handleargs{...}

--[[
  **************************
  * interactive console    *
  * written by: ni-richard *
  **************************
]]

require "essential"

_PROMPT = "\n> "
_PROMPT2 = "+ "

function print(x)
  local name, value = type(x)
  local max = 18446744073709551616
  ans = x
  
  if name == "number" then
    if x > max then
      value = string.format("%.10e", x)
    else
      value = string.format("%.10f", x):unpad("0")
      if value[-1] == "." then
        value = value:sub(1, #value - 1)
      end
    end
  elseif name == "string" then
    value = "\"" .. x .. "\""
  else
    value = tostring(x)
    list1, list2 = {value:find("file %((.*)%)")}, {value:find("(.+): (.+)")}
    if #list1 == 3 then
      name, value = "file", list1[3]
    elseif #list2 == 4 then
      name, value = list2[3], list2[4]
    end
  end
  
  printf("  ans = %s%s%s (%s%s%s)\n",
         color.boldwhite, value, color.reset,
         color.boldgreen, name, color.reset)
end

local serve = function() end
debug.sethook(function(event, line)
  local name, scope, value
  
  scope = debug.getinfo(2).short_src
  if event == "call" and scope == "stdin" then
    printf("\x1b[1A%s>%s\n", color.grey, color.reset)
  elseif event == "line" and scope == "stdin" and line == 1 then
    name, value = debug.getlocal(2, 1)
    if value == "return ;" then
      printf("\x1b[1A%s>%s serve()\n", color.grey, color.reset)
      print(serve())
    else serve = debug.getinfo(2).func end
  end
end, "cl")

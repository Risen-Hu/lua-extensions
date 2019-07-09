--[[
  **************************
  * stdlib extensions      *
  * written by: ni-richard *
  **************************
]]

function printf(fmt, ...)
  io.write(fmt:format(...))
end

local strind = getmetatable("").__index
getmetatable("").__index = function(str, i)
  if type(i) == "number" then
    return string.sub(str, i, i)
  else
    return strind[i]
  end
end

function string:unpad(fill)
  local pos = #self
  
  while self[pos] == fill do
    pos = pos - 1
  end
  return self:sub(1, pos)
end

function string.fromhex(text)
  local prod = ""
  
  for i = 1, #text, 2 do
    printf("%s\n",text:sub(i,i+1))
    prod = prod .. string.char(tonumber(text:sub(i, i + 1), 16))
  end
  return prod
end

function string.tohex(data)
  local prod = ""
  
  for i = 1, #data do
    prod = prod .. string.format("%02x", string.byte(data[i]))
  end
  return prod
end

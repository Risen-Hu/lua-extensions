--[[
  **************************
  * enter into a container *
  * written by: ni-richard *
  **************************
]]

name = arg[1] or ""
proc = io.popen("docker ps", "r")

for line in proc:lines() do
  id = line:match("^(%x+)%s+" .. name)
  if id then
    os.execute("docker exec -it " .. id .. " bash")
    break
  end
end

proc:close()

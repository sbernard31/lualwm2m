local lwm2m = require 'lwm2m'
local socket = require 'socket'

local udp = socket.udp();
udp:setsockname('*', 5682)

local deviceObj = {
  id = 3,
  [0]  = "Open Mobile Alliance",                   -- manufacturer
  [1]  = "Lightweight M2M Client",                 -- model number
  [2]  = "345000123",                              -- serial number
  [3]  = "1.0",                                    -- firmware version
  [13] = {read = function() return os.time() end}, -- current time
}

local ll = lwm2m.init("testlualwm2mclient", {deviceObj},
  function(data,host,port) udp:sendto(data,host,port) end)

ll:addserver(123, "127.0.0.1", 5683)
ll:register()

repeat
  ll:step()
  local data, ip, port, msg = udp:receivefrom()
  if data then
    ll:handle(data,ip,port)
  end
until false

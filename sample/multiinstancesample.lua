local lwm2m = require 'lwm2m'
local socket = require 'socket'
local obj = require 'lwm2mobject'

-- Get script arguments.
local args = {...}
local serverip = args[1] or "127.0.0.1"
local serverport = args[2] or 5683
local deviceport = args[3] or 5682

-- Create UDP socket.
local udp = socket.udp();
udp:setsockname('*', deviceport)

-- Define mandatory objects (used for connection)
local securityObj = obj.new(0, {
  [0]  = "coap://"..serverport..":"..serverport,   -- serverURI
  [1]  = false,                                    -- true if it's a bootstrap server
  [10] = 123,                                      -- short server ID
  [11] = 0,                                        -- client hold off time (revelant only for bootstrap server)
})
local serverObj = obj.new(1, {
  [0]  = 123,                                      -- short server ID
  [1]  = 3600,                                     -- lifetime
  [7]  = "U",                                      -- binding
})
local deviceObj = obj.new(3, {
  [0]  = "Open Mobile Alliance",                   -- manufacturer
  [1]  = "Lightweight M2M Client",                 -- model number
  [2]  = "345000123",                              -- serial number
  [3]  = "1.0",                                    -- firmware version
  [13] = {                                         -- current time
    read  = function() return os.time() end,
    write = function (i,v) print(v) end,
    type  = "date"},
})
-- Define an custom object to manage temperature in rooms.
local rooms = obj.new(21, {
  [0]  = {read = function () return tostring(math.random(16,30)) end}, -- Current Temperature
  [1]  = {read = true, write=true, type="string"},                     -- Temperature set-point
},true)

-- Create instances.
local bedroom = rooms:newinstance(0);
bedroom[1] = "16";   -- bedroom temperature set point

local livingroom = rooms:newinstance(1);
livingroom[1] = "18";   -- livingroom temperature set point


-- Initialize lwm2m client.
local ll = lwm2m.init("lua-multi-instance-client", {securityObj, serverObj,deviceObj, rooms},
  function(serverid) return serverip,serverport end,
  function(data,host,port) udp:sendto(data,host,port) end)

-- Communicate ...
ll:start()
repeat
  ll:step()
  local data, ip, port, msg = udp:receivefrom()
  if data then
    ll:handle(data,ip,port)
  end
until false

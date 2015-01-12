local lwm2m = require 'lwm2m'
local obj = require "lwm2mobject"
local socket = require 'socket'

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
  [1]  = 3600,                                        -- lifetime
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

-- Initialize lwm2m client.
local ll = lwm2m.init("lua-observed-client", {securityObj, serverObj, deviceObj},
  function(serverid) return serverip,serverport end,
  function(data,host,port) udp:sendto(data,host,port) end)

-- set timeout for a non-blocking receivefrom call.
udp:settimeout(5)

-- Communicate ...
ll:start()
repeat
  ll:step()
  local data, ip, port, msg = udp:receivefrom()
  if data then
    ll:handle(data,ip,port)
  end

  -- notify the resource /3/0/13 change
  -- (every 5seconds because of the timeout configuration)
  ll:resourcechanged("/3/0/13")
until false

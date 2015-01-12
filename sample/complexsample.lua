local lwm2m = require 'lwm2m'
local obj = require 'lwm2mobject'
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
-- Define a sample object.
local sampleObj = obj.new(3, {
  -- READ
  [0]  = "Res0 : Read only",
  [1]  = {read = function (instance) return "Res2: Read only" end},

  -- EXECUTE
  [4]  = {execute = function (instance) print ("Res4: execute !") end},

  -- READ/WRITE/EXECUTE
  [13] = {
    read  = function (instance)
      local val = instance[15] or 123456
      print (val); return val;
    end,
    write = function (instance, value)
      print("Res15 modification")
      print("before :", instance[15]," after:",value)
      instance[15] = value
    end,
    type = "date" -- could be string, number, boolean or date
  },

  -- READ/WRITE
  [14] = {read = "default value", write = true},
  [15] = function (instance, mode, value)
    if mode == "read" then
      return "Res13: Read/Write/Execute, mode=" .. mode
    elseif mode == "write" then
      print ("Res13: Read/Write/Execute, value=",value, ", mode=",mode)
    elseif mode == "execute" then
      print ("Res13: Read/Write/Execute, mode=",mode)
    end
  end
})

-- Initialize lwm2m client.
local ll = lwm2m.init("lua-complex-client", {securityObj,serverObj,sampleObj},
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

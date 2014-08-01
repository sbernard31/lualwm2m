local lwm2m = require 'lwm2m'
local obj = require 'lwm2mobject'
local socket = require 'socket'
-- you need a luadtls available at https://github.com/sbernard31/luadtls
local dtls = require "dtls"

-- Get script arguments.
local args = {...}
local serverip = args[1] or "127.0.0.1"
local serverport = args[2] or 5684
local deviceport = args[3] or 5683

-- Create UDP socket.
local udp = socket.udp();
udp:setsockname('*', deviceport)
-- Secure the socket.
dtls.wrap(udp, {security = "PSK", identity = "Client_identity", key = dtls.hex2bin("73656372657450534b")})

-- Define a device object.
local deviceObj = obj.new(3, {
  [0]  = "Open Mobile Alliance",                   -- manufacturer
  [1]  = "Lightweight M2M Client",                 -- model number
  [2]  = "345000123",                              -- serial number
  [3]  = "1.0",                                    -- firmware version
  [13] = {read = function() return os.time() end}, -- current time
})

-- Initialize lwm2m client.
local ll = lwm2m.init("dtlslualwm2mclient", {deviceObj},
  function(data,host,port) udp:sendto(data,host,port) end)

-- Add server and register to it.
ll:addserver(123, serverip, serverport)
ll:register()

-- Communicate ...
repeat
  ll:step()
  local data, ip, port, msg = udp:receivefrom()
  if data then
    ll:handle(data,ip,port)
  end
until false

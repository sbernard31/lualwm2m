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

-- Define an custom object to manage temperature in rooms.
local rooms = obj.new(21, {
  [0]  = {read = function () return math.random(16,30) end},  -- Current Temperature
  [1]  = {read = true, write=true},                           -- Temperature set-point
},true)

-- Create instances.
local bedroom = rooms:newinstance(0);
bedroom[1] = 16;   -- bedroom temperature set point

local livingroom = rooms:newinstance(1);
livingroom[1] = 18;   -- livingroom temperature set point


-- Initialize lwm2m client.
local ll = lwm2m.init("lua-multi-instance-client", {rooms},
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

local lwm2m = require 'lwm2m'
local obj = require 'lwm2mobject'
local socket = require 'socket'
-- you need a luadtls available at https://github.com/sbernard31/luadtls
local dtls = require "dtls"

-- Get script arguments.
local args = {...}
local serverip = args[1] or "127.0.0.1"
local serverport = args[2] or 5684
local server_xkey=dtls.hex2bin("fcc28728c123b155be410fc1c0651da374fc6ebe7f96606e90d927d188894a73")
local server_ykey=dtls.hex2bin("d2ffaa73957d76984633fc1cc54d0b763ca0559a9dff9706e9f4557dacc3f52a")

local deviceport = args[3] or 5683

-- Create UDP socket.
local udp = socket.udp();
udp:setsockname('*', deviceport)
-- Secure the socket.
dtls.wrap(udp,{
  security = "ECC",
  privatekey = dtls.hex2bin("e67b68d2aaeb6550f19d98cade3ad62b39532e02e6b422e1f7ea189dabaea5d2"),
  xpublickey = dtls.hex2bin("89c048261979208666f2bfb188be1968fc9021c416ce12828c06f4e314c167b5"),
  ypublickey = dtls.hex2bin("cbf1eb7587f08e01688d9ada4be859137ca49f79394bad9179326b3090967b68"),
  verify = function (ip,port,xkey,ykey)
   --print (ip, port, dtls.bin2hex(xkey), dtls.bin2hex(ykey))
   return ip == serverip and port == serverport and xkey == server_xkey and ykey == server_ykey
  end
})

-- Define a device object.
local deviceObj = obj.new(3, {
  [0]  = "Open Mobile Alliance",                   -- manufacturer
  [1]  = "Lightweight M2M Client",                 -- model number
  [2]  = "345000123",                              -- serial number
  [3]  = "1.0",                                    -- firmware version
  [13] = {read = function() return os.time() end}, -- current time
})

-- Initialize lwm2m client.
local ll = lwm2m.init("lua-dtlsrpk-client", {deviceObj},
  function(data,host,port) udp:sendto(data,host,port) end)

-- Add server and register to it.
ll:addserver(123, serverip, serverport)
ll:register()

udp:settimeout(5)

-- Communicate ...
repeat
  ll:step()
  local data, ip, port, msg = udp:receivefrom()
  if data then
    ll:handle(data,ip,port)
  else
    -- if there is no more data for us,
    -- we close the DTLS session for this peer.
    udp:close(serverip,serverport)
  end
until false

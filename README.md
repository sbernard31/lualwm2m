lualwm2m
=======

**lualwm2m** is a Lua binding for client side [**liblwm2m/wakaama**](https://github.com/eclipse/wakaama) library .


A **lualwm2m** \ **luasocket** sample :
``` lua
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
  [0]  = "coap://"..serverport..":"..serverport,-- serverURI
  [1]  = false,                                 -- true if it's a bootstrap server
  [10] = 123,                                   -- short server ID
  [11] = 0,                                     -- client hold off time (revelant only for bootstrap server)
})
local serverObj = obj.new(1, {
  [0] = 123,                                    -- short server ID
  [1] = 3600,                                   -- lifetime
  [7] = "U",                                    -- binding
})
local deviceObj = obj.new(3, {
  [0]  = "Open Mobile Alliance",                -- manufacturer
  [1]  = "Lightweight M2M Client",              -- model number
  [2]  = "345000123",                           -- serial number
  [3]  = "1.0",                                 -- firmware version
  [13] = {                                      -- current time
    read = function() return os.time() end,
    write = function (i,v) print(v) end,
    type = "date"},
})

-- Initialize lwm2m client.
local ll = lwm2m.init("lua-client", {securityObj,serverObj,deviceObj},
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
```
More samples available in [sample folder](https://github.com/sbernard31/lualwm2m/tree/master/sample).
You could use [luadtls](https://github.com/sbernard31/luadtls) to secure your lwm2m communication with DTLS.


Compile & Test
--------------
Get the code : (*`--recursive` is need because of use of git submodule.*)
```
git clone --recursive https://github.com/sbernard31/lualwm2m.git lualwm2m
```


Compile it : (You need lua header files from package `liblua5.1-0-dev`)
```
mkdir [builddir]
cd [builddir]
cmake [lualwm2m source directory]
make
```

Test it : (*Lua 5.1 and luasocket is needed.*)
```
lua simplesample.lua [serverip] [serverport] [deviceport]
```
You can test it with [leshan](https://github.com/jvermillard/leshan).
Go to http://54.228.25.31 and run `lua simplesample.lua 54.228.25.31`

Limitation
----------
**lualwm2m** binding is still in development.
For now, it is only compatible with Lua 5.1 and linux.

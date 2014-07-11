lualwm2m is a binding lua for the liblwm2m library (LWM2M protocol from the Open Mobile Alliance).
lualwm2m is a Lua binding for Lua 5.1 of the client implementation for the LWM2M protocol from the Open Mobile Alliance base on C liblwm2m library.

Testing
-------

Compile the C module :
In the any directory [build directory], run the following commands:
    cmake [liblwm2m directory]/lua/
    make

To Test add the .so to your LUA_CPATH env var and use the sample.lua (you need to install luasocket)

For Linux :
export LUA_CPATH=';;[build directory]/?.so'
lua simplesample.lua


Syntax
-------

idea.lua contains syntax idea.

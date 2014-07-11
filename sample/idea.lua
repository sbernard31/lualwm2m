---
-- This file contains idea for futur object description implementation.
--
-- Please, do not hesitate to propose more syntax.
--
local lwm2m = require 'lwm2m'

-- Proposition : single instance
-- IMPLEMENTED
local sobj = {
  id = 3,
  [0] = {
    read = function (instance) return "value" end,
    write = function (instance, value) end,
    execute = function (instance) end,
  },
  [1] = "string/integer value",                 --> ressource in R
  [2] = 1024,                                   --> ressource in R
  [3] = function (obj,mode,value) end,          --> ressource in RWE
}

-- Proposition : full structure with instances management
-- NOT IMPLEMENTED
local obj = {
  id = 3,
  instances = {
    [0] = {
      [1] = "Instance 0",
    },
    [1] = {
      [1] = "Instance 1",
    }
  },
  operations ={
    [0] = {
      read = function (instance) return "value" end,
      write = function (instance,value) end,
      execute = function (instance) end,
    },
    [1] = {read = true}
  }
}

-- Proposition : object style
-- NOT IMPLEMENTED
local obj = lwm2m.obj(3)
obj[0] = "read only value"                       --> ressource in R
obj[3] = function (instance,mode,value) end      --> ressource in RWE

--obj[i] = {unix style right, value}          
obj[1] = {2, "read only value"}                  --> ressource in W
obj[2] = {6, "writable value"}                   --> ressource in RW
obj[4] = {1, function (instance,mode,value) end} --> ressource in E
obj[5] = {7, function (instance,mode,value) end} --> ressource in RWE

obj[6].write = function (instance,value) end     --> ressource in W
obj[7].execute = function (instance) end         --> ressource in E
obj[8].read = function (instance) end            --> ressource in R
obj[8] = {                                       --> ressource in RW
  write = function (instance,value) end,
  read = function (instance) end
}

-- use execute/read/write at lua side : not sure this was really usefull...
function callback(instance,value)
  instance[0]:execute()         -- execute()
  instance[0] = value           -- write()
  local r = instance[0]         -- read()
  local r = rawget(instance,0)  -- rawget
  rawset(instance,0,value)      -- rawset
end
 
-- Please, do not hesitate to propose more syntax.

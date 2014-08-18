local M = {}

-- LWM2M constant
M.CREATED = 0x41
M.DELETED = 0x42
M.CHANGED   = 0x44
M.CONTENT   = 0x45
M.BAD_REQUEST = 0x80
M.UNAUTHORIZED = 0x81
M.NOT_FOUND    = 0x84
M.METHOD_NOT_ALLOWED = 0x85
M.NOT_ACCEPTABLE = 0x86
M.INTERNAL_SERVER_ERROR  = 0xA0
M.NOT_IMPLEMENTED = 0xA1
M.SERVICE_UNAVAILABLE = 0xA3

-- LWM2M Read operation
local function read (instance, resourceid)
  local _mt = getmetatable(instance)
  local operations = _mt.object.operations

  local op = operations[resourceid]
  local optype = type(op)

  if optype == "nil" then
    return M.NOT_FOUND
  elseif optype == "string" then
    return M.CONTENT, op
  elseif optype == "number" then
    return M.CONTENT, op
  elseif optype == "function" then
    return M.CONTENT, op(instance,"read")
  elseif optype == "table" then
    if type(op.read) == "function" then
      return M.CONTENT, op.read(instance)
    elseif type(op.read) == "boolean" and op.read then
      return M.CONTENT, instance[resourceid]
    elseif type(op.read) == "string" then
      return M.CONTENT, instance[resourceid] or op.read
    end
  end

  return M.METHOD_NOT_ALLOWED
end

-- LWM2M Write operation
local function write (instance, resourceid, value)
  local _mt = getmetatable(instance)
  local operations = _mt.object.operations

  local op = operations[resourceid]
  local optype = type(op)

  if optype == "nil" then
    return M.NOT_FOUND
  elseif optype == "function" then
    return M.CHANGED, op(instance, "write", value)
  elseif optype == "table" then
    if type(op.write) == "function" then
      return M.CHANGED, op.write(instance,value)
    elseif type(op.write) == "boolean" and op.write then
      instance[resourceid]  = value
      return M.CHANGED
    end
  end

  return M.METHOD_NOT_ALLOWED
end

-- LWM2M Execute operation
local function execute (instance, resourceid)
  local _mt = getmetatable(instance)
  local operations = _mt.object.operations

  local op = operations[resourceid]
  local optype = type(op)

  if optype == "nil" then
    return M.NOT_FOUND
  elseif optype == "function" then
    return M.CHANGED, op(instance,"execute")
  elseif optype == "table" then
    if type(op.execute) == "function" then
      return M.CHANGED, op.execute(instance)
    end
  end

  return M.METHOD_NOT_ALLOWED
end

-- list all available resources
local function list (instance)
  local _mt = getmetatable(instance)
  local operations = _mt.object.operations

  local res = {}

  for resourceid, operation in pairs(operations) do
    if type(resourceid)=="number" then
      table.insert(res,resourceid)
    end
  end

  return res
end

function M.new(id, operations, multi)
  local object = {
    id = id,
    operations = operations,
    newinstance = function (obj,id)
      local instance = {id = id}
      obj[id] = instance
      setmetatable(instance,{
        object     = obj,
        __index    = {read = read, write = write, execute = execute, list = list},
      })
      return instance
    end,
    multi = multi,
    create = function (obj,id)
      if multi and not obj[id] then
        local instance = obj:newinstance(id)
        return M.CREATED, instance 
      else
        return M.METHOD_NOT_ALLOWED
      end
    end
  }

  if not multi then
    object:newinstance(0)
  end

  return object
end

return M
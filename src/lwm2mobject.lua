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

-- LWM2M TYPE
M.LWM2M_STRING = 0x01
M.LWM2M_NUMBER = 0x02
M.LWM2M_BOOLEAN = 0x03

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
    elseif type(op.read) == "string" or type(op.read) == "number" then
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

-- get the type of resource with the given resourceid
local function _type (instance, resourceid)
  local _mt = getmetatable(instance)
  local operations = _mt.object.operations

  local op = operations[resourceid]
  local optype = type(op)

  if optype == "table" then
    if type(op.type) == "string" then
      -- use the field type to know the type
      if op.type == "number" or op.type == "date" then
        return M.LWM2M_NUMBER
      elseif op.type == "boolean" then
        return M.LWM2M_BOOLEAN
      elseif op.type == "string" then
        return M.LWM2M_STRING
      end
    elseif type(op.write) == "string" then
      -- use the field type to know the type
      if op.write == "number" or op.type == "date" then
        return M.LWM2M_NUMBER
      elseif op.write == "boolean" then
        return M.LWM2M_BOOLEAN
      elseif op.write == "string" then
        return M.LWM2M_STRING
      end
    elseif op.read then
      -- use the type of read field
      if type(op.read) == "number" then
        return M.LWM2M_NUMBER
      elseif type(op.read) == "boolean" then
        return M.LWM2M_BOOLEAN
      elseif type(op.read) == "string" then
        return M.LWM2M_STRING
      end
    end
  elseif optype == "number" then
    return M.LWM2M_NUMBER
  elseif optype == "boolean" then
    return M.LWM2M_BOOLEAN
  elseif optype == "string" then
    return M.LWM2M_STRING
  end
  
  -- default type is "string"
  return M.LWM2M_STRING
end

-- LWM2M delete operation
local function delete (instance)
  local _mt = getmetatable(instance)
  local obj = _mt.object

  if type(instance.delete) == "boolean" and instance.delete then
    obj[instance.id]  = nil
    return M.DELETED
  end

  -- if no delete information for this object
  -- default behavior is to :
  -- delete is llowed for multiinstance and forbidden for single one.
  if obj.multi then
    obj[instance.id] = nil
    return M.DELETED
  else
    return M.METHOD_NOT_ALLOWED
  end
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
        __index    = {read = read, write = write, execute = execute, list = list, delete = delete, type = _type},
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

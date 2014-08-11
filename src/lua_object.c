/*
 MIT License (MIT)

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#include "liblwm2m.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua5.1/lua.h"
#include "lua5.1/lauxlib.h"
#include "lua5.1/lualib.h"

typedef struct luaobject_userdata {
	lua_State * L;
	int tableref;
} luaobject_userdata;

// Push the instance with the given instanceId on the lua stack
static int prv_get_instance(lua_State * L, luaobject_userdata * userdata,
		uint16_t instanceId) {
	// Get table of this object on the stack.
	lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->tableref); // stack: ..., object
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 0;
	}

	// Get instance
	lua_pushinteger(L, instanceId); // stack: ..., object, instanceId
	lua_gettable(L, -2); // stack: ..., object, instance
	if (!lua_istable(L, -1)) {
		lua_pop(L, 2);
		return 0;
	}

	// Remove object of the stack
	lua_remove(L, -2);  // stack: ..., instance

	return 1;
}

// Push a lua list on the stack of all resourceId available for the instance on the stack
static int prv_get_resourceId_list(lua_State * L) {
	// Call the list function
	lua_getfield(L, -1, "list"); // stack: ..., instance, listFunc

	// list field should be a function
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // clean the stack
		return 0;
	}

	// Push instance and resource id on the stack and call the writeFunc
	lua_pushvalue(L, -2);  // stack: ..., instance, listFunc, instance
	lua_call(L, 1, 1); // stack: ..., instance, list

	if (!lua_istable(L, -1)) {
		lua_pop(L, 1); // clean the stack
		return 0;
	}

	return 1;
}

// Read the resource of the instance on the top of the stack.
static uint8_t prv_read_resource(lua_State * L, uint16_t resourceid,
		lwm2m_tlv_t * tlvP) {

	// Get the read function
	lua_getfield(L, -1, "read"); // stack: ..., instance, readFunc
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // clean the stack
		return COAP_500_INTERNAL_SERVER_ERROR ;
	}

	// Push instance and resource id on the stack and call the readFunc
	lua_pushvalue(L, -2);  // stack: ..., instance, readFunc, instance
	lua_pushinteger(L, resourceid); // stack: ..., instance, readFunc, instance, resourceId
	lua_call(L, 2, 2); // stack: ..., instance, return_code, value

	// Get return code
	int ret = lua_tointeger(L, -2);
	if (ret == COAP_205_CONTENT) {
		tlvP->id = resourceid;
		tlvP->value = strdup(lua_tolstring(L, -1, &tlvP->length));
	}
	// clean the stack
	lua_pop(L, 2);
	return ret;
}

static uint8_t prv_read(uint16_t instanceId, int * numDataP,
		lwm2m_tlv_t ** dataArrayP, lwm2m_object_t * objectP) {

	// Get user data.
	luaobject_userdata * userdata = (luaobject_userdata*) objectP->userData;
	lua_State * L = userdata->L;

	// Push instance on the stack
	int res = prv_get_instance(L, userdata, instanceId); // stack: ..., instance
	if (!res)
		return COAP_500_INTERNAL_SERVER_ERROR ;

	if ((*numDataP) == 0) {
		// Push resourceId list on the stack
		int res = prv_get_resourceId_list(L); // stack : ..., instance, resourceList
		if (!res) {
			lua_pop(L, 1);
			return COAP_500_INTERNAL_SERVER_ERROR ;
		}

		// Get number of resource
		size_t nbRes = lua_objlen(L, -1);

		// Create temporary structure
		lwm2m_tlv_t tmpDataArray[nbRes];
		memset(tmpDataArray, 0, nbRes * sizeof(lwm2m_tlv_t));

		// Iterate through all items of the resourceId list
		int i = 0;
		lua_pushnil(L); // stack: ..., instance, resourceList, key(nil)
		while (lua_next(L, -2) != 0) { // stack: ...,instance , resourceList, key, value
			if (lua_isnumber(L, -1)) {
				int resourceid = lua_tonumber(L, -1);
				lua_pushvalue(L, -4); // stack: ...,instance , resourceList, key, value, instance
				int res = prv_read_resource(L, resourceid, &tmpDataArray[i]);
				if (res <= COAP_205_CONTENT)
					i++;
				lua_pop(L, 1); // stack: ...,instance , resourceList, key, value
			}
			// Removes 'value'; keeps 'key' for next iteration
			lua_pop(L, 1); // stack: ...,instance , resourceList, key
		}
		// Clean the stack
		lua_pop(L, 2);

		// Allocate memory for this resource
		*dataArrayP = lwm2m_tlv_new(i);
		if (*dataArrayP == NULL)
			return COAP_500_INTERNAL_SERVER_ERROR ;

		// Copy data in output parameter
		(*numDataP) = i;
		memcpy(*dataArrayP, tmpDataArray, sizeof(lwm2m_tlv_t) * i);

		return COAP_205_CONTENT ;
	} else {
		// Get resource.
		(*numDataP) = 1;
		int ret = prv_read_resource(L, (*dataArrayP)->id, (*dataArrayP));
		lua_pop(L, 1);
		return ret;
	}

	lua_pop(L, 1);
	return COAP_501_NOT_IMPLEMENTED ;
}

// Read the resource of the instance on the top of the stack.
static uint8_t prv_write_resource(lua_State * L, uint16_t resourceid,
		lwm2m_tlv_t tlv) {

	// Get the write function
	lua_getfield(L, -1, "write"); // stack: ..., instance, writeFunc
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // clean the stack
		return COAP_500_INTERNAL_SERVER_ERROR ;
	}

	// Push instance and resource id on the stack and call the writeFunc
	lua_pushvalue(L, -2);  // stack: ..., instance, writeFunc, instance
	lua_pushinteger(L, resourceid); // stack: ..., instance, writeFunc, instance, resourceId
	lua_pushlstring(L, tlv.value, tlv.length); // stack: ..., instance, writeFunc, instance, resourceId, value
	lua_call(L, 3, 1); // stack: ..., instance, return_code

	// Get return code
	int ret = lua_tointeger(L, -1);

	// Clean the stack
	lua_pop(L, 1);
	return ret;
}

static uint8_t prv_write(uint16_t instanceId, int numData,
		lwm2m_tlv_t * dataArray, lwm2m_object_t * objectP) {
	// Get user data.
	luaobject_userdata * userdata = (luaobject_userdata*) objectP->userData;
	lua_State * L = userdata->L;

	// Push instance on the stack
	int res = prv_get_instance(L, userdata, instanceId);
	if (!res)
		return COAP_500_INTERNAL_SERVER_ERROR ;

	// write resource
	int i = 0;
	int result;
	do {
		result = prv_write_resource(userdata->L, dataArray[i].id, dataArray[i]);
		i++;
	} while (i < numData && result == COAP_204_CHANGED );
	lua_pop(L, 1);
	return result;
}

static uint8_t prv_execute_resource(lua_State * L, uint16_t resourceid) {
	// Get the execute_function
	lua_getfield(L, -1, "execute"); // stack: ..., instance, executeFunc
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1); // clean the stack
		return COAP_500_INTERNAL_SERVER_ERROR ;
	}

	// Push instance and resource id on the stack and call the executeFunc
	lua_pushvalue(L, -2);  // stack: ..., instance, executeFunc, instance
	lua_pushinteger(L, resourceid); // stack: ..., instance, executeFunc, instance, resourceId
	lua_call(L, 2, 1); // stack: ..., instance, return_code

	// get return code
	int ret = lua_tointeger(L, -1);

	// clean the stack
	lua_pop(L, 1);
	return ret;
}

static uint8_t prv_execute(uint16_t instanceId, uint16_t resourceId,
		char * buffer, int length, lwm2m_object_t * objectP) {
	// Get user data.
	luaobject_userdata * userdata = (luaobject_userdata*) objectP->userData;
	lua_State * L = userdata->L;

	//push instance on the stack
	int res = prv_get_instance(L, userdata, instanceId);
	if (!res)
		return COAP_500_INTERNAL_SERVER_ERROR ;

	// execute the given resource for the given id
	if (instanceId == 0) {
		int ret = prv_execute_resource(userdata->L, resourceId);
		lua_pop(L, 1);
		return ret;
	} else {
		// TODO : manage multi-instance.
		lua_pop(L, 1);
		return COAP_501_NOT_IMPLEMENTED ;
	}
	lua_pop(L, 1);
	return COAP_501_NOT_IMPLEMENTED ;
}

static uint8_t prv_create(uint16_t instanceId, int numData,
		lwm2m_tlv_t * dataArray, lwm2m_object_t * objectP) {
	return COAP_501_NOT_IMPLEMENTED ;
}

static uint8_t prv_delete(uint16_t id, lwm2m_object_t * objectP) {
	return COAP_501_NOT_IMPLEMENTED ;
}

static void prv_close(lwm2m_object_t * objectP) {

	luaobject_userdata * userdata = (luaobject_userdata *) objectP->userData;
	if (userdata != NULL) {
		// Release table reference in lua registry.
		if (userdata->tableref != LUA_NOREF) {
			luaL_unref(userdata->L, LUA_REGISTRYINDEX, userdata->tableref);
			userdata->tableref = LUA_NOREF;
		}

		// Release memory.
		free(userdata);
		objectP->userData = NULL;
	}
}

lwm2m_object_t * get_lua_object(lua_State *L, int tableindex, int objId) {

	// Allocate memory for lwm2m object.
	lwm2m_object_t * objectP = (lwm2m_object_t *) malloc(
			sizeof(lwm2m_object_t));

	if (NULL != objectP) {
		memset(objectP, 0, sizeof(lwm2m_object_t));

		// Allocate memory for userdata.
		luaobject_userdata * userdata = (luaobject_userdata *) malloc(
				sizeof(luaobject_userdata));
		if (userdata == NULL) {
			free(objectP);
			return NULL;
		}

		// store table represent this object
		lua_pushvalue(L, tableindex); // stack: ..., objectTable
		userdata->tableref = luaL_ref(L, LUA_REGISTRYINDEX); //stack: ...

		// set fields
		userdata->L = L;
		objectP->objID = objId;
		objectP->readFunc = prv_read;
		objectP->writeFunc = prv_write;
		objectP->executeFunc = prv_execute;
		objectP->createFunc = prv_create;
		objectP->deleteFunc = prv_delete;
		objectP->closeFunc = prv_close;
		objectP->userData = userdata;

		// Update instance List
		// ---------------------
		// Get table of this object on the stack.
		lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->tableref); // stack: ..., objectTable
		int i = 0;
		lua_pushnil(L); // stack: ..., objectTable, key(nil)
		while (lua_next(L, -2) != 0) { // stack: ..., objectTable, key, value
			if (lua_isnumber(L, -2)) {
				int instanceid = lua_tonumber(L, -2);
				lwm2m_list_t * instance = malloc(sizeof(lwm2m_list_t));
				if (NULL == instance)
					return NULL;
				memset(instance, 0, sizeof(lwm2m_list_t));
				instance->id = instanceid;
				objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList,
						instance);
			}
			// Removes 'value'; keeps 'key' for next iteration
			lua_pop(L, 1); // stack: ..., objectTable, key
		}
		// Clean the stack
		lua_pop(L, 1);

	}

	return objectP;
}

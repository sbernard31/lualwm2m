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

#include "lua5.1/lua.h"
#include "lua5.1/lauxlib.h"
#include "lua5.1/lualib.h"

#include "liblwm2m.h"
#include <string.h>
#include <stdlib.h>


extern lwm2m_object_t * get_lua_object(lua_State *L, int tableindex, int objId);

void stackdump_g(lua_State* l) {
	int i;
	int top = lua_gettop(l);

	printf(" ============== >total in stack %d\n", top);

	for (i = 1; i <= top; i++) { /* repeat for each level */
		int t = lua_type(l, i);
		switch (t) {
		case LUA_TSTRING: /* strings */
			printf("string: '%s'\n", lua_tostring(l, i));
			break;
		case LUA_TBOOLEAN: /* booleans */
			printf("boolean %s\n", lua_toboolean(l, i) ? "true" : "false");
			break;
		case LUA_TNUMBER: /* numbers */
			printf("number: %g\n", lua_tonumber(l, i));
			break;
		default: /* other values */
			printf("%s\n", lua_typename(l, t));
			break;
		}
		printf("  "); /* put a separator */
	}
	printf("\n"); /* end the listing */
}

typedef struct llwm_addr_t {
	char * host;
	int port;
} llwm_addr_t;

typedef struct llwm_userdata {
	lua_State * L;
	lwm2m_context_t * ctx;
	int sendCallbackRef;
} llwm_userdata;

static llwm_userdata * checkllwm(lua_State * L, const char * functionname) {
	llwm_userdata* lwu = (llwm_userdata*) luaL_checkudata(L, 1,
			"lualwm2m.llwm");

	if (lwu->ctx == NULL)
		luaL_error(L, "bad argument #1 to '%s' (llwm object is closed)",
				functionname);

	return lwu;
}

static uint8_t prv_buffer_send_callback(void * sessionH, uint8_t * buffer, size_t length, void * userData) {

	llwm_userdata * ud = userData;
	lua_State * L = ud->L;
	llwm_addr_t * la = (llwm_addr_t *) sessionH;

	lua_rawgeti(L, LUA_REGISTRYINDEX, ud->sendCallbackRef);
	lua_pushlstring(L, buffer, length);
	lua_pushstring(L, la->host);
	lua_pushnumber(L, la->port);
	lua_call(L, 3, 0);

	return COAP_NO_ERROR ;
}

static int llwm_init(lua_State *L) {
	// 1st parameter : should be end point name.
	char * endpointName = luaL_checkstring(L, 1);

	// 2nd parameter : should be a list of "lwm2m objects".
	luaL_checktype(L, 2, LUA_TTABLE);
	size_t objListLen = lua_objlen(L, 2);
	if (objListLen <= 0)
		return luaL_error(L,
				"bad argument #2 to 'init' (should be a non empty list : #table > 0)");

	// 3rd parameter : should be a callback.
	luaL_checktype(L, 3, LUA_TFUNCTION);

	// Create llwm userdata object and set its metatable.
	llwm_userdata * lwu = lua_newuserdata(L, sizeof(llwm_userdata)); // stack: endpoint, tableobj, sendcallback, lwu
	lwu->L = L;
	lwu->sendCallbackRef = LUA_NOREF;
	lwu->ctx = NULL;
	luaL_getmetatable(L, "lualwm2m.llwm"); // stack: endpoint, tableobj, sendcallback, lwu, metatable
	lua_setmetatable(L, -2); // stack: endpoint, tableobj, sendcallback, lwu
	lua_replace(L, 1); // stack: lwu, tableobj, sendcallback

	// Store the callback in Lua registry to keep a reference on it.
	int callbackref = luaL_ref(L, LUA_REGISTRYINDEX); // stack: lwu, tableobj

	// Manage "lwm2m objects" list :
	// For each object in "lwm2m objects" list, create a "C lwm2m object".
	lwm2m_object_t * objArray[objListLen];
	int i;
	for (i = 1; i <= objListLen; i++) {
		// Get object table.
		lua_rawgeti(L, -1, i); // stack: lwu, tableobj, tableobj[i]
		if (lua_type(L, -1) != LUA_TTABLE)
			return luaL_error(L,
					"bad argument #2 to 'init' (all element of the list should be a table with a 'id' field which is a number )");

		// Check the id field is here.
		lua_getfield(L, -1, "id"); // stack: lwu, tableobj, tableobj[i], tableobj[i].id
		if (!lua_isnumber(L, -1))
			return luaL_error(L,
					"bad argument #2 to 'init' (all element of the list should be a table with a 'id' field which is a number)");

		int id = (int) lua_tonumber(L, -1);
		lua_pop(L, 1); // stack: lwu, tableobj, tableobj[i]

		// Create Lua Object.
		lwm2m_object_t * obj = get_lua_object(L, -1, id); //stack should not be modify by "get_lua_object".
		if (obj == NULL) {
			// object can not be create, release previous one.
			for (i--; i >= 1; i--) {
				free(objArray[i - 1]);
			}
			return luaL_error(L,
					"unable to create objects (Bad object structure or memory allocation problem ?)");
		}
		objArray[i - 1] = obj;
		lua_pop(L, 1); // stack: lwu, tableobj
	}
	lua_pop(L, 1); // stack: lwu

	// Context Initialization.
	lwm2m_context_t * contextP = lwm2m_init(endpointName, objListLen,
			objArray, prv_buffer_send_callback, lwu);
	lwu->ctx = contextP;
	lwu->sendCallbackRef = callbackref;

	return 1;
}

static int llwm_add_server(lua_State *L) {
	// Get llwm userdata.
	llwm_userdata * lwu = checkllwm(L, "addserver");

	// Get server short ID.
	uint16_t shortID = luaL_checknumber(L, 2);

	// Get server address.
	char* host = luaL_checkstring(L, 3);
	int port = luaL_checkint(L, 4);

	// Get lifetime for registration
	int lifetime = luaL_checkint(L, 5);

	// SMS number of NULL
	char * sms = luaL_checkstring(L, 6);

	// on empty string the SMS number is null
	if (strlen(sms) <= 0) {
		sms = NULL;
	}
	// binding mode
	char * strBinding = luaL_checkstring(L, 7);
	lwm2m_binding_t binding = BINDING_UNKNOWN;

	if (strcmp(strBinding,"U") == 0) {
		binding = BINDING_U;
	} else if (strcmp(strBinding,"UQ") == 0) {
		binding = BINDING_UQ;
	} else if (strcmp(strBinding,"S") == 0) {
		binding = BINDING_S;
	} else if (strcmp(strBinding,"SQ") == 0) {
		binding = BINDING_SQ;
	} else if (strcmp(strBinding,"US") == 0) {
		binding = BINDING_US;
	} else if (strcmp(strBinding,"UQS") == 0) {
		binding = BINDING_UQS;
	} else {
		return luaL_error(L,
			"unknown binding mode");
	}

	// Create struct to store it.
	size_t lal = sizeof(struct llwm_addr_t);
	struct llwm_addr_t * la = malloc(lal);
	if (la == NULL)
		return luaL_error(L, "Memory allocation problem when 'addserver'");
	la->host = strdup(host);
	la->port = port;

	// We do not manage security for now.
	lwm2m_security_t security;
	memset(&security, 0, sizeof(lwm2m_security_t));

	// Add server to context.
	int error = lwm2m_add_server(lwu->ctx, shortID, lifetime, sms, binding, la, &security);

	// Free adress struct memory.
	//free(la->host); // TODO we cannot free it because it is already used, problem this will never be free...
	//free(la);

	// Manage error.
	if (error)
		return luaL_error(L, "unable to add server (uid=%d,url=%s,port=%d)",
				shortID, host, port);

	return 0;
}

static int llwm_register(lua_State *L) {
	// Get llwm userdata.
	llwm_userdata * lwu = checkllwm(L, "register");

	// Register it to servers.
	lwm2m_register(lwu->ctx);

	return 0;
}

static int llwm_handle(lua_State *L) {
	// Get llwm userdata.
	llwm_userdata * lwu = checkllwm(L, "handle");

	// Get data buffer.
	size_t length;
	uint8_t * buffer = (uint8_t*) luaL_checklstring(L, 2, &length);

	// Get server address.
	char* host = luaL_checkstring(L, 3);
	int port = luaL_checkint(L, 4);

	// HACK : https://github.com/01org/liblwm2m/pull/18#issuecomment-45501037
	// find session object in the server list.
	lwm2m_server_t * targetP;
	llwm_addr_t * la = NULL;
	bool found = false;
	targetP = lwu->ctx->serverList;
	while (targetP != NULL && !found) {
		// get host and port of the target
		llwm_addr_t * session = (llwm_addr_t *) targetP->sessionH;
		if (session != NULL && session->port == port
				&& strcmp(session->host, host) == 0) {
			la = session;
			found = true;
		} else {
			targetP = targetP->next;
		}
	}

	// Handle packet
	if (found)
		lwm2m_handle_packet(lwu->ctx, buffer, length, la);

	return 0;
}

static int llwm_step(lua_State *L) {
	// Get llwm userdata.
	llwm_userdata * lwu = checkllwm(L, "step");

	// TODO make this arguments available in lua.
	struct timeval tv;
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	lwm2m_step(lwu->ctx, &tv);

	return 0;
}

static int llwm_resource_changed(lua_State *L) {
	// Get llwm userdata.
	llwm_userdata *lwu = checkllwm(L, "resource_changed");

	// Get parameters.
	size_t length;
	uint8_t * uriPath = (uint8_t*) luaL_checklstring(L, 2, &length);

	// Create URI of resource which changed.
	lwm2m_uri_t uri;
	int result = lwm2m_stringToUri(uriPath, length, &uri);
	if (result == 0){
		lua_pushnil(L);
		lua_pushstring(L,"resource uri syntax error");
		return 2;
	}

	//notify the change.
	lwm2m_resource_value_changed(lwu->ctx, &uri);
	return 0;
}

static int llwm_close(lua_State *L) {
	// Get llwm userdata
	llwm_userdata* lwu = (llwm_userdata*) luaL_checkudata(L, 1,
				"lualwm2m.llwm");

	// Close lwm2m context.
	if (lwu->ctx) {
		lwm2m_close(lwu->ctx);
		lwu->ctx->bufferSendUserData = NULL;
	}

	// Release "send" callback.
	luaL_unref(L, LUA_REGISTRYINDEX, lwu->sendCallbackRef);
	lwu->sendCallbackRef = LUA_NOREF;

	lwu->ctx = NULL;

	return 0;
}

static const struct luaL_Reg llwm_objmeths[] = { { "handle", llwm_handle }, {
		"addserver", llwm_add_server }, { "register", llwm_register }, {
		"close", llwm_close }, { "step", llwm_step }, { "resourcechanged",
		llwm_resource_changed }, { "__gc", llwm_close }, {
NULL, NULL } };

static const struct luaL_Reg llwm_modulefuncs[] = { { "init", llwm_init }, {
NULL, NULL } };

int luaopen_lwm2m(lua_State *L) {
	// Define llwm object metatable.
	luaL_newmetatable(L, "lualwm2m.llwm"); // stack: metatable

	// Do : metatable.__index = metatable.
	lua_pushvalue(L, -1); // stack: metatable, metatable
	lua_setfield(L, -2, "__index"); // stack: metatable

	// Register llwm object methods : set methods to table on top of the stack
	luaL_register(L, NULL, llwm_objmeths); // stack: metatable

	// Register module functions.
	luaL_register(L, "lwm2m", llwm_modulefuncs); // stack: metatable, functable
	return 1;
}

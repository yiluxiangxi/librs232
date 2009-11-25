/*
 * Copyright (c) 2009 Petr Stetiar <ynezz@true.cz>, Gaben Ltd.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <lauxlib.h>
#include <lua.h>

#include <string.h>
#include <stdlib.h>

#include "librs232/rs232.h"

#ifdef WIN32
#include "librs232/rs232_windows.h"
#else
#include "librs232/rs232_linux.h"
#endif

#define MODULE_TIMESTAMP __DATE__ " " __TIME__
#define MODULE_NAMESPACE "luars232"
#define MODULE_VERSION "1.0.1"
#define MODULE_BUILD "$Id$"
#define MODULE_COPYRIGHT "Copyright (c) 2009 Petr Stetiar <ynezz@true.cz>, Gaben Ltd."

static struct {
	const char *name;
	unsigned long value;
} luars232_ulong_consts[] = {
	/* baudrates */
	{ "RS232_BAUD_9600", RS232_BAUD_9600 },
	{ "RS232_BAUD_19200", RS232_BAUD_19200 },
	{ "RS232_BAUD_38400", RS232_BAUD_38400 },
	{ "RS232_BAUD_57600", RS232_BAUD_57600 },
	{ "RS232_BAUD_115200", RS232_BAUD_115200 },
	/* databits */
	{ "RS232_DATA_5", RS232_DATA_5 },
	{ "RS232_DATA_6", RS232_DATA_6 },
	{ "RS232_DATA_7", RS232_DATA_7 },
	{ "RS232_DATA_8", RS232_DATA_8 },
	/* stop bits */
	{ "RS232_STOP_1", RS232_STOP_1 },
	{ "RS232_STOP_2", RS232_STOP_2 },
	/* parity */
	{ "RS232_PARITY_NONE", RS232_PARITY_NONE },
	{ "RS232_PARITY_ODD", RS232_PARITY_ODD },
	{ "RS232_PARITY_EVEN", RS232_PARITY_EVEN },
	/* flow */
	{ "RS232_FLOW_OFF", RS232_FLOW_OFF },
	{ "RS232_FLOW_HW", RS232_FLOW_HW },
	{ "RS232_FLOW_XON_XOFF", RS232_FLOW_XON_XOFF },
	/* errors */
	{ "RS232_ERR_NOERROR", RS232_ERR_NOERROR },
	{ "RS232_ERR_UNKNOWN", RS232_ERR_UNKNOWN },
	{ "RS232_ERR_OPEN", RS232_ERR_OPEN },
	{ "RS232_ERR_CLOSE", RS232_ERR_CLOSE },
	{ "RS232_ERR_FLUSH", RS232_ERR_FLUSH },
	{ "RS232_ERR_CONFIG", RS232_ERR_CONFIG },
	{ "RS232_ERR_READ", RS232_ERR_READ },
	{ "RS232_ERR_WRITE", RS232_ERR_WRITE },
	{ "RS232_ERR_SELECT", RS232_ERR_SELECT },
	{ "RS232_ERR_TIMEOUT", RS232_ERR_TIMEOUT },
	{ "RS232_ERR_IOCTL", RS232_ERR_IOCTL },
	{ "RS232_ERR_PORT_CLOSED", RS232_ERR_PORT_CLOSED },
	{ NULL, 0 }
};

#ifdef WIN32
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
	return TRUE;
}
#endif

/*
 * rs232 = require("luars232")
 * error, port = rs232.open(device)
 * error, port = rs232.open("/dev/ttyUSB0")
 */
static int lua_port_open(lua_State *L)
{
	int ret = 0;
	struct rs232_port_t *p = NULL;

	p = rs232_init();
	if (p == NULL) {
		lua_pushinteger(L, RS232_ERR_CONFIG);
		lua_pushnil(L);
		return 2;
	}

	DBG("p=%p \n", (void *)p);

	rs232_set_device(p, (char *) luaL_checkstring(L, 1));
	ret = rs232_open(p);
	if (ret > RS232_ERR_NOERROR) {
		free(p->pt);
		free(p);
		lua_pushinteger(L, ret);
		lua_pushnil(L);
		return 2;
	}

	lua_pushinteger(L, RS232_ERR_NOERROR);
	lua_pushlightuserdata(L, p);

	luaL_getmetatable(L, MODULE_NAMESPACE);
	lua_setmetatable(L, -2);

	return 2;
}

/* 
 * error, data, read_len = port:read(max_read_len [[, timeout_ms], forced])
 *
 * if forced > 0 then read() blocks until 'timeout_ms' or there's 'max_read_len'
 * bytes available
 */
static int lua_port_read(lua_State *L)
{
	int ret = 0;
	int argc = 0;
	int forced = 0;
	unsigned int timeout = 0;
	unsigned int len = 0;
	unsigned int bytes_read = 0;
	unsigned char *data = NULL;
	struct rs232_port_t *p = NULL;

	p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);
	lua_remove(L, 1);

	if (p == NULL || !rs232_port_open(p)) {
		lua_pushinteger(L, RS232_ERR_PORT_CLOSED);
		lua_pushnil(L);
		lua_pushinteger(L, 0);
		return 3;
	}

	argc = lua_gettop(L);
	switch (argc) {
	case 1:
		len = (unsigned int) luaL_checkinteger(L, 1);
		data = (unsigned char*) malloc(len * sizeof(unsigned char *));
		memset(data, 0, len);
		ret = rs232_read(p, data, len, &bytes_read);
		break;
	case 2:
	case 3:
		len = (unsigned int) luaL_checknumber(L, 1);
		data = (unsigned char*) malloc(len * sizeof(unsigned char *));
		memset(data, 0, len);
		timeout = (unsigned int) luaL_checknumber(L, 2);
		forced = luaL_optint(L, 3, 0);
		if (forced > 0)
			ret = rs232_read_timeout_forced(p, data, len, &bytes_read, timeout);
		else
			ret = rs232_read_timeout(p, data, len, &bytes_read, timeout);
		break;
	default:
		lua_pushinteger(L, RS232_ERR_UNKNOWN);
		lua_pushnil(L);
		lua_pushinteger(L, 0);
		return 3;
	}

	DBG("ret=%d hex='%s' bytes_read=%d\n",
	    ret, rs232_hex_dump(data, bytes_read), bytes_read);

	lua_pushinteger(L, RS232_ERR_NOERROR);
	if (bytes_read > 0)
		lua_pushlstring(L, (char *) data, bytes_read);
	else
		lua_pushnil(L);

	if (data)
		free(data);

	lua_pushinteger(L, bytes_read);
	return 3;
}

/*
 * error, written_len = port:write(data [, timeout_ms])
 */
static int lua_port_write(lua_State *L)
{
	int ret = 0;
	int argc = 0;
	unsigned int timeout = 0;
	unsigned int wlen = 0;
	size_t len = 0;
	const char *data;
	struct rs232_port_t *p = NULL;

	p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);
	lua_remove(L, 1);

	if (p == NULL || !rs232_port_open(p)) {
		lua_pushinteger(L, RS232_ERR_PORT_CLOSED);
		lua_pushinteger(L, 0);
		return 2;
	}

	argc = lua_gettop(L);
	switch (argc) {
	case 1: {
		data = luaL_checklstring(L, 1, &len);
		ret = rs232_write(p, (unsigned char*) data, (unsigned int) len, &wlen);
		break;
	}
	case 2:
		data = luaL_checklstring(L, 1, &len);
		timeout = (unsigned int) luaL_checknumber(L, 2);
		ret = rs232_write_timeout(p, (unsigned char*) data, (unsigned int) len, &wlen, timeout);
		break;
	default:
		lua_pushinteger(L, RS232_ERR_CONFIG);
		lua_pushinteger(L, 0);
		return 2;
	}

	lua_pushinteger(L, ret);
	lua_pushinteger(L, wlen);
	return 2;
}

/* error = port:close() */
static int lua_port_close(lua_State *L)
{
	struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);

	if (p == NULL || !rs232_port_open(p)) {
		lua_pushinteger(L, RS232_ERR_PORT_CLOSED);
		return 1;
	}

	lua_pushinteger(L, rs232_close(p));
	return 1;
}

/* error = port:flush() */
static int lua_port_flush(lua_State *L)
{
	struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);

	if (p == NULL || !rs232_port_open(p)) {
		lua_pushinteger(L, RS232_ERR_PORT_CLOSED);
		return 1;
	}

	lua_pushinteger(L, rs232_flush(p));
	return 1;
}

/* __tostring metamethod */
static int lua_port_tostring(lua_State *L)
{
	const char *ret;
	struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);

	if (p == NULL) {
		lua_pushnil(L);
		return 1;
	}

	ret = rs232_to_string(p);
	if (ret == NULL)
		lua_pushnil(L);
	else
		lua_pushstring(L, ret);

	return 1;
}

static int lua_port_device(lua_State *L)
{
	struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE);
	const char *ret = rs232_get_device(p);
	if (ret == NULL)
		lua_pushnil(L);
	else
		lua_pushstring(L, ret);

	return 1;
}

/*
 * print(port:error_tostring(error))
 */
static int lua_port_strerror(lua_State *L)
{
	const char *ret = rs232_strerror((unsigned int) luaL_checkinteger(L, 1));
	if (ret == NULL)
		lua_pushnil(L);
	else
		lua_pushstring(L, ret);

	return 1;
}

#define FN_SET_PORT(type) \
	static int lua_port_set_##type(lua_State *L) \
	{ \
		struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE); \
		lua_pushnumber(L, rs232_set_##type(p, (unsigned int) luaL_checknumber(L, 2))); \
		return 1; \
	} \

#define FN_GET_PORT(type) \
	static int lua_port_get_##type(lua_State *L) \
	{ \
		struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE); \
		lua_pushnumber(L, rs232_get_##type(p)); \
		return 1; \
	}

#define FN_GET_PORT_STRING(type) \
	static int lua_port_get_str##type(lua_State *L) \
	{ \
		struct rs232_port_t *p = (struct rs232_port_t*) luaL_checkudata(L, 1, MODULE_NAMESPACE); \
		int param = (int) luaL_optinteger(L, 2, -1); \
		const char *ret = rs232_str##type(param == -1 ? rs232_get_##type(p) : (unsigned int) param); \
		if (ret == NULL) { \
			lua_pushnil(L); \
		} else { \
			lua_pushstring(L, ret); \
		} \
		return 1; \
	}

FN_SET_PORT(baud)
FN_SET_PORT(data)
FN_SET_PORT(stop)
FN_SET_PORT(parity)
FN_SET_PORT(flow)

FN_GET_PORT(baud)
FN_GET_PORT(data)
FN_GET_PORT(stop)
FN_GET_PORT(parity)
FN_GET_PORT(flow)

FN_GET_PORT_STRING(baud)
FN_GET_PORT_STRING(data)
FN_GET_PORT_STRING(stop)
FN_GET_PORT_STRING(parity)
FN_GET_PORT_STRING(flow)

static luaL_reg port_methods[] = {
	{ "__tostring", lua_port_tostring },
	{ "read", lua_port_read },
	{ "write", lua_port_write },
	{ "close", lua_port_close },
	{ "flush", lua_port_flush },
	{ "device", lua_port_device },
	/* baud */
	{ "baud_rate", lua_port_get_baud },
	{ "baud_rate_tostring", lua_port_get_strbaud },
	{ "set_baud_rate", lua_port_set_baud },
	/* data */
	{ "data_bits", lua_port_get_data },
	{ "data_bits_tostring", lua_port_get_strdata },
	{ "set_data_bits", lua_port_set_data },
	/* stop */
	{ "stop_bits", lua_port_get_stop },
	{ "stop_bits_tostring", lua_port_get_strstop },
	{ "set_stop_bits", lua_port_set_stop },
	/* parity */
	{ "parity", lua_port_get_parity },
	{ "parity_tostring", lua_port_get_strparity },
	{ "set_parity", lua_port_set_parity },
	/* flow */
	{ "flow_control", lua_port_get_flow },
	{ "flow_control_tostring", lua_port_get_strflow },
	{ "set_flow_control", lua_port_set_flow },
	{ NULL, NULL }
};

static luaL_reg port_functions[] = {
	{ "open", lua_port_open },
	{ "error_tostring", lua_port_strerror },
	{ NULL, NULL }
};

static int create_metatables(lua_State *L, const char *name, const luaL_reg *methods)
{
	if (!luaL_newmetatable (L, name))
		return 0;

	luaL_openlib (L, NULL, methods, 0);

	lua_pushliteral (L, "__gc");
	lua_pushcfunction (L, methods->func);
	lua_settable (L, -3);

	lua_pushliteral (L, "__index");
	lua_pushvalue (L, -2);
	lua_settable (L, -3);

	return 1;
}

RS232_LIB int luaopen_luars232(lua_State *L);
RS232_LIB int luaopen_luars232(lua_State *L)
{
	int i;
	luaL_openlib(L, MODULE_NAMESPACE, port_functions, 0);

	for (i = 0; luars232_ulong_consts[i].name != NULL; i++) {
		lua_pushstring(L, luars232_ulong_consts[i].name);
		lua_pushnumber(L, luars232_ulong_consts[i].value);
		lua_settable(L, -3);
	}

	create_metatables(L, MODULE_NAMESPACE, port_methods);
	lua_pop (L, 1);

	lua_pushstring(L, MODULE_VERSION);
	lua_setfield(L, -2, "_VERSION");

	lua_pushstring(L, MODULE_BUILD);
	lua_setfield(L, -2, "_BUILD");

	lua_pushstring(L, MODULE_TIMESTAMP);
	lua_setfield(L, -2, "_TIMESTAMP");

	lua_pushstring(L, MODULE_COPYRIGHT);
	lua_setfield(L, -2, "_COPYRIGHT");

	DBG("[*] luaopen_luars232(Version: '%s' Build: '%s' TimeStamp: '%s')\n",
	    MODULE_VERSION, MODULE_BUILD, MODULE_TIMESTAMP);

	return 0;
}

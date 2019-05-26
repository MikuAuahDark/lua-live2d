/**
 * Copyright (c) 2040 Dark Energy Processor Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

/* std */
#include <stdlib.h>
#include <string.h>

/* Config */
#ifdef HAVE_LUALIVE2D_CONFIG_H
#include "lualive2d_config.h"
#endif

#ifndef LUALIVE2D_METATABLE_NAME
#define LUALIVE2D_METATABLE_NAME "Live2DModel*"
#endif

/* Lua */
#include "lua.h"
#include "lauxlib.h"

/* Live2D */
#include "Live2DCubismCore.h"

/* It is always win32 that forces dllexport duh */
#if defined(_WIN32) && !defined(LUALIVE2D_EMBEDDED)
#define EXPORT_SIGNATURE __declspec(dllexport)
#else
#define EXPORT_SIGNATURE
#endif

/* This define align memory */
#define ALIGN_TO_N(ptr, n) ((size_t) (ptr) + (n - 1) & (~(size_t) (n - 1)))

/* Struct for the metadata */
typedef struct ModelDefinition
{
	void *mocMemory, *mocMemoryAligned;
	csmMoc *moc;
	void *modelMemory, *modelMemoryAligned;
	csmModel *model;
} ModelDefinition;

typedef union FunctionString
{
	const void *ptr;
	const char str[sizeof(void*)];
} FunctionString;

static int l2dh_istrue(lua_State *L, int idx)
{
	int type = lua_type(L, idx);

	switch(type)
	{
		case LUA_TNIL:
			return 0;
		case LUA_TBOOLEAN:
			return lua_toboolean(L, idx);
		case LUA_TNUMBER:
			return lua_tonumber(L, idx) != 0.0;
		case LUA_TSTRING:
			return lua_objlen(L, idx) > 0;
		default:
			return 1;
	}
}

static int l2d_loadModel(lua_State *L)
{
	size_t mocSize;
	unsigned int modelSize;
	const char *mocData;
	ModelDefinition tempModel, *modelObject;

	/* Need to get string contents of the moc */
	mocData = luaL_checklstring(L, 1, &mocSize);

	/* Allocate moc memory */
	tempModel.mocMemory = malloc(mocSize + csmAlignofMoc - 1);
	if (tempModel.mocMemory == NULL)
		luaL_error(L, "cannot allocate moc memory");

	tempModel.mocMemoryAligned = (void *) ALIGN_TO_N(tempModel.mocMemory, csmAlignofMoc);

	/* Load moc */
	memcpy(tempModel.mocMemoryAligned, mocData, mocSize);
	tempModel.moc = csmReviveMocInPlace(tempModel.mocMemoryAligned, (unsigned int) mocSize);
	if (tempModel.moc == NULL)
	{
		free(tempModel.mocMemory);
		luaL_error(L, "cannot load moc file");
	}

	/* Get model size */
	modelSize = csmGetSizeofModel(tempModel.moc);
	if (modelSize == 0)
	{
		free(tempModel.mocMemory);
		luaL_error(L, "cannot get model size");
	}

	/* Allocate model memory */
	tempModel.modelMemory = malloc(modelSize + csmAlignofModel - 1);
	if (tempModel.modelMemory == NULL)
	{
		free(tempModel.mocMemory);
		luaL_error(L, "cannot allocate model memory");
	}

	tempModel.modelMemoryAligned = (void *) ALIGN_TO_N(tempModel.modelMemory, csmAlignofModel);

	/* Load model */
	tempModel.model = csmInitializeModelInPlace(tempModel.moc, tempModel.modelMemoryAligned, modelSize);
	if (tempModel.model == NULL)
	{
		free(tempModel.modelMemory);
		free(tempModel.mocMemory);
		luaL_error(L, "cannot load model");
	}

	/* Create new Lua userdata */
	modelObject = (ModelDefinition *) lua_newuserdata(L, sizeof(ModelDefinition));
	memcpy(modelObject, &tempModel, sizeof(ModelDefinition));
	luaL_getmetatable(L, LUALIVE2D_METATABLE_NAME);
	lua_setmetatable(L, -2);

	return 1;
}

static int l2dw___tostring(lua_State *L)
{
	ModelDefinition *model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	lua_pushfstring(L, LUALIVE2D_METATABLE_NAME": %p", model);

	return 1;
}

static int l2dw___gc(lua_State *L)
{
	ModelDefinition *model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	free(model->modelMemory);
	free(model->mocMemory);

	return 0;
}

static int l2dw_update(lua_State *L)
{
	ModelDefinition *model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	csmUpdateModel(model->model);

	return 0;
}

static int l2dw_readCanvasInfo(lua_State *L)
{
	ModelDefinition *model;
	csmVector2 size, offset;
	float units;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	csmReadCanvasInfo(model->model, &size, &offset, &units);

	lua_pushnumber(L, size.X);
	lua_pushnumber(L, size.Y);
	lua_pushnumber(L, offset.X);
	lua_pushnumber(L, offset.Y);
	lua_pushnumber(L, units);

	return 5;
}

static int l2dw_getParameterDefault(lua_State *L)
{
	ModelDefinition *model;
	int paramCount, namedRet, tableIndex;
	const char **paramNames;
	const float *paramMin, *paramMax, *paramDef;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	paramCount = csmGetParameterCount(model->model);
	paramNames = csmGetParameterIds(model->model);
	paramMin = csmGetParameterMinimumValues(model->model);
	paramMax = csmGetParameterMaximumValues(model->model);
	paramDef = csmGetParameterDefaultValues(model->model);

	/* Check if user supply a table. */
	if (lua_istable(L, 2))
	{
		/* Use existing, user-supplied table */
		tableIndex = 2;
		namedRet = l2dh_istrue(L, 3);
	}
	else
	{
		/* Create new table */
		namedRet = l2dh_istrue(L, 2);
		lua_createtable(L, namedRet ? 0 : paramCount, namedRet ? paramCount : 0);
		tableIndex = lua_gettop(L);
	}

	if (namedRet)
	{
		/* The key is the parameter name instead */
		for (int i = 0; i < paramCount; i++)
		{
			lua_pushstring(L, paramNames[i]);
			lua_createtable(L, 0, 3);

			/* Index */
			lua_pushlstring(L, "index", 5);
			lua_pushinteger(L, i + 1);
			lua_rawset(L, -3);
			/* Min value */
			lua_pushlstring(L, "min", 3);
			lua_pushnumber(L, paramMin[i]);
			lua_rawset(L, -3);
			/* Max value */
			lua_pushlstring(L, "max", 3);
			lua_pushnumber(L, paramMax[i]);
			lua_rawset(L, -3);
			/* Default value */
			lua_pushlstring(L, "default", 7);
			lua_pushnumber(L, paramDef[i]);
			lua_rawset(L, -3);
			/* Set */
			lua_rawset(L, tableIndex);
		}
	}
	else
	{
		/* Ordered by parameter count */
		for (int i = 0; i < paramCount; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_createtable(L, 0, 4);

			/* Parameter name */
			lua_pushlstring(L, "name", 4);
			lua_pushstring(L, paramNames[i]);
			lua_rawset(L, -3);
			/* Min value */
			lua_pushlstring(L, "min", 3);
			lua_pushnumber(L, paramMin[i]);
			lua_rawset(L, -3);
			/* Max value */
			lua_pushlstring(L, "max", 3);
			lua_pushnumber(L, paramMax[i]);
			lua_rawset(L, -3);
			/* Default value */
			lua_pushlstring(L, "default", 7);
			lua_pushnumber(L, paramDef[i]);
			lua_rawset(L, -3);
			/* Set */
			lua_rawset(L, tableIndex);
		}
	}

	/* Push table index into stack and return that */
	lua_pushvalue(L, tableIndex);
	return 1;
}

static int l2dw_getParameterValues(lua_State *L)
{
	ModelDefinition *model;
	int paramCount, tableIndex, namedRet;
	const char **paramNames;
	float *paramValues;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	paramCount = csmGetParameterCount(model->model);
	paramValues = csmGetParameterValues(model->model);

	/* Check if user supply a table. */
	if (lua_istable(L, 2))
	{
		/* Use existing, user-supplied table */
		tableIndex = 2;
		namedRet = l2dh_istrue(L, 3);
	}
	else
	{
		/* Create new table */
		namedRet = l2dh_istrue(L, 2);
		lua_createtable(L, namedRet ? 0 : paramCount, namedRet ? paramCount : 0);
		tableIndex = lua_gettop(L);
	}

	if (namedRet)
	{
		paramNames = csmGetParameterIds(model->model);

		for (int i = 0; i < paramCount; i++)
		{
			lua_pushstring(L, paramNames[i]);
			lua_pushnumber(L, paramValues[i]);
			lua_rawset(L, tableIndex);
		}
	}
	else
	{
		for (int i = 0; i < paramCount; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_pushnumber(L, paramValues[i]);
			lua_rawset(L, tableIndex);
		}
	}

	lua_pushvalue(L, tableIndex);
	return 1;
}

static int l2dw_setParameterValues(lua_State *L)
{
	ModelDefinition *model;
	int paramCount, namedRet;
	const char **paramNames;
	float *paramValues;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	paramCount = csmGetParameterCount(model->model);
	paramValues = csmGetParameterValues(model->model);
	luaL_checktype(L, 2, LUA_TTABLE);
	namedRet = l2dh_istrue(L, 3);

	if (namedRet)
	{
		paramNames = csmGetParameterIds(model->model);

		for (int i = 0; i < paramCount; i++)
		{
			lua_pushstring(L, paramNames[i]);
			lua_rawget(L, 2);
			if (lua_isnumber(L, -1))
				paramValues[i] = (float) lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}
	else
	{
		for (int i = 0; i < paramCount; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_rawget(L, 2);
			if (lua_isnumber(L, -1))
				paramValues[i] = (float) lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}

	return 0;
}

static int l2dw_getPartsData(lua_State *L)
{
	ModelDefinition *model;
	int partCount, namedRet, tableIndex;
	const int *partParent;
	const char **partNames;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	partCount = csmGetPartCount(model->model);
	partNames = csmGetPartIds(model->model);
	partParent = csmGetPartParentPartIndices(model->model);
	
	/* Check if user supply a table. */
	if (lua_istable(L, 2))
	{
		/* Use existing, user-supplied table */
		tableIndex = 2;
		namedRet = l2dh_istrue(L, 3);
	}
	else
	{
		/* Create new table */
		namedRet = l2dh_istrue(L, 2);
		lua_createtable(L, namedRet ? 0 : partCount, namedRet ? partCount : 0);
		tableIndex = lua_gettop(L);
	}

	if (namedRet)
	{
		for(int i = 0; i < partCount; i++)
		{
			lua_pushstring(L, partNames[i]);
			lua_createtable(L, 0, 2);

			/* Index */
			lua_pushlstring(L, "index", 5);
			lua_pushinteger(L, i + 1);
			lua_rawset(L, -3);
			/* Parent */
			if (partParent[i] >= 0 && partParent[i] < partCount)
			{
				lua_pushlstring(L, "parent", 6);
				lua_pushstring(L, partNames[partParent[i]]);
				lua_rawset(L, -3);
			}
		}
	}
	else
	{
		for(int i = 0; i < partCount; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_createtable(L, 0, 2);

			/* Index */
			lua_pushlstring(L, "name", 4);
			lua_pushstring(L, partNames[i]);
			lua_rawset(L, -3);
			/* Parent */
			if (partParent[i] >= 0 && partParent[i] < partCount)
			{
				lua_pushlstring(L, "parent", 6);
				lua_pushinteger(L, partParent[i]);
				lua_rawset(L, -3);
			}
		}
	}

	lua_pushvalue(L, tableIndex);
	return 2;
}

/* Libraries to export */
const luaL_Reg l2d_export[] = {
	{"loadModel", &l2d_loadModel},
	{NULL, NULL}
};

/* Methods to export */
const luaL_Reg l2dw_export[] = {
	{"__tostring", &l2dw___tostring},
	{"__gc", &l2dw___gc},
	{"update", &l2dw_update},
	{"getParameterDefault", &l2dw_getParameterDefault},
	{"readCanvasInfo", &l2dw_readCanvasInfo},
	{"getParameterValues", &l2dw_getParameterValues},
	{"setParameterValues", &l2dw_setParameterValues},
	{"getPartsData", &l2dw_getPartsData},
	{NULL, NULL}
};

int EXPORT_SIGNATURE luaopen_lualive2d(lua_State *L)
{
	/* This was meant to be used in conjunction with */
	/* https://www.github.com/MikuAuahDark/lua-live2d-framework */
	luaL_error(L, "are you missing lua-live2d-framework? this shouldn't be invoked");
	lua_pushnil(L);
	return 1;
}

int EXPORT_SIGNATURE luaopen_lualive2d_core(lua_State *L)
{
	const luaL_Reg *i;
	csmVersion csmVer;
	FunctionString fstr;

	/* New table for our beloved module */
	lua_createtable(L, 0, sizeof(l2d_export) + 2);

	lua_pushlstring(L, "_mt", 3);
	/* Create new metatable */
	luaL_newmetatable(L, LUALIVE2D_METATABLE_NAME);
	/* Export methods */
	for (i = l2dw_export; i->name != NULL; i++)
	{
		lua_pushstring(L, i->name);
		lua_pushcfunction(L, i->func);
		lua_rawset(L, -3);
	}
	/* Set metatable to global module table */
	lua_rawset(L, -3);

	/* Export methods */
	for (i = l2d_export; i->name != NULL; i++)
	{
		lua_pushstring(L, i->name);
		lua_pushcfunction(L, i->func);
		lua_rawset(L, -3);
	}

	/* Live2D core function pointer, for LuaJIT FFI */
	/* Pointer is stored as sizeof(void*)-sized string */
	lua_pushlstring(L, "ptr", 3);
	lua_newtable(L);

#define SET_FUNCTION_POINTER(name) \
	fstr.ptr = &name; \
	lua_pushstring(L, #name); \
	lua_pushlstring(L, fstr.str, sizeof(void*)); \
	lua_rawset(L, -3);

	SET_FUNCTION_POINTER(csmGetVersion);
	SET_FUNCTION_POINTER(csmGetLatestMocVersion);
	SET_FUNCTION_POINTER(csmGetMocVersion);
	SET_FUNCTION_POINTER(csmGetLogFunction);
	SET_FUNCTION_POINTER(csmSetLogFunction);
	SET_FUNCTION_POINTER(csmReviveMocInPlace);
	SET_FUNCTION_POINTER(csmGetSizeofModel);
	SET_FUNCTION_POINTER(csmInitializeModelInPlace);
	SET_FUNCTION_POINTER(csmUpdateModel);
	SET_FUNCTION_POINTER(csmReadCanvasInfo);
	SET_FUNCTION_POINTER(csmGetParameterCount);
	SET_FUNCTION_POINTER(csmGetParameterIds);
	SET_FUNCTION_POINTER(csmGetParameterMinimumValues);
	SET_FUNCTION_POINTER(csmGetParameterMaximumValues);
	SET_FUNCTION_POINTER(csmGetParameterDefaultValues);
	SET_FUNCTION_POINTER(csmGetParameterValues);
	SET_FUNCTION_POINTER(csmGetPartCount);
	SET_FUNCTION_POINTER(csmGetPartIds);
	SET_FUNCTION_POINTER(csmGetPartOpacities);
	SET_FUNCTION_POINTER(csmGetPartParentPartIndices);
	SET_FUNCTION_POINTER(csmGetDrawableCount);
	SET_FUNCTION_POINTER(csmGetDrawableIds);
	SET_FUNCTION_POINTER(csmGetDrawableConstantFlags);
	SET_FUNCTION_POINTER(csmGetDrawableDynamicFlags);
	SET_FUNCTION_POINTER(csmGetDrawableTextureIndices);
	SET_FUNCTION_POINTER(csmGetDrawableDrawOrders);
	SET_FUNCTION_POINTER(csmGetDrawableRenderOrders);
	SET_FUNCTION_POINTER(csmGetDrawableOpacities);
	SET_FUNCTION_POINTER(csmGetDrawableMaskCounts);
	SET_FUNCTION_POINTER(csmGetDrawableMasks);
	SET_FUNCTION_POINTER(csmGetDrawableVertexCounts);
	SET_FUNCTION_POINTER(csmGetDrawableVertexPositions);
	SET_FUNCTION_POINTER(csmGetDrawableVertexUvs);
	SET_FUNCTION_POINTER(csmGetDrawableIndexCounts);
	SET_FUNCTION_POINTER(csmGetDrawableIndices);
	SET_FUNCTION_POINTER(csmResetDrawableDynamicFlags);
#undef SET_FUNCTION_POINTER

	lua_rawset(L, -3);

	/* Live2D version */
	csmVer = csmGetVersion();
	lua_pushlstring(L, "Live2DVersion", 13);
	lua_pushfstring(L, "%u.%u.%u", (csmVer & 0xFF000000U) >> 24, (csmVer & 0xFF0000) >> 16, csmVer & 0xFFFF);
	lua_rawset(L, -3);

	return 1;
}

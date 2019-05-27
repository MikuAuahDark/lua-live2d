/**
 * Copyright (C) 2019 Miku AuahDark
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **/

/* std */
#include <stdlib.h>
#include <string.h>

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

#ifndef LUALIVE2D_METATABLE_NAME
#define LUALIVE2D_METATABLE_NAME "Live2DModel*"
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
	csmVector2 modelDimensions, modelCenter;
	float modelDPI;
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
		case LUA_TNONE:
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

	/* Read canvas info */
	csmReadCanvasInfo(tempModel.model, &tempModel.modelDimensions, &tempModel.modelCenter, &tempModel.modelDPI);

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
	ModelDefinition *model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);

	lua_pushnumber(L, model->modelDimensions.X);
	lua_pushnumber(L, model->modelDimensions.Y);
	lua_pushnumber(L, model->modelCenter.X);
	lua_pushnumber(L, model->modelCenter.Y);
	lua_pushnumber(L, model->modelDPI);

	return 5;
}

/* Returns namedRet */
static int l2dh_usertablenamed(lua_State *L, int idx, int *tableIndex, int allocsize)
{
	/* Check if user supply a table. */
	if (lua_istable(L, idx))
	{
		/* Use existing, user-supplied table */
		*tableIndex = idx;
		return l2dh_istrue(L, idx + 1);
	}
	else
	{
		/* Create new table */
		int namedRet = l2dh_istrue(L, idx);
		lua_createtable(L, namedRet ? 0 : allocsize, namedRet ? allocsize : 0);
		*tableIndex = lua_gettop(L);
		return namedRet;
	}
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
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, paramCount);

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
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, paramCount);

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
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, partCount);

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

			lua_rawset(L, tableIndex);
		}
	}

	lua_pushvalue(L, tableIndex);
	return 2;
}

static int l2dw_getPartsOpacity(lua_State *L)
{
	ModelDefinition *model;
	int partCount, namedRet, tableIndex, i;
	const float *partOpacity;
	const char **partNames;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	partCount = csmGetPartCount(model->model);
	partOpacity = csmGetPartOpacities(model->model);
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, partCount);

	if (namedRet)
	{
		partNames = csmGetPartIds(model->model);

		for(i = 0; i < partCount; i++)
		{
			lua_pushstring(L, partNames[i]);
			lua_pushnumber(L, partOpacity[i]);
			lua_rawset(L, -3);
		}
	}
	else
	{
		for(i = 0; i < partCount; i++)
		{
			lua_pushinteger(L, i + 1);
			lua_pushnumber(L, partOpacity[i]);
			lua_rawset(L, -3);
		}
	}

	lua_pushvalue(L, tableIndex);
	return 1;
}

static int l2dw_getDrawableData(lua_State *L)
{
	ModelDefinition *model;
	int drawCount, namedRet, tableIndex, i, j;
	const char **drawNames;
	const unsigned short **drawIndex;
	const int *drawIndexCount, *drawMaskCount, *drawTex, *drawVertCount, **drawMask;
	const csmFlags *drawConstFlags;
	const csmVector2 **drawUVs;
	static csmFlags blendMulAndAdd = csmBlendAdditive | csmBlendAdditive;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	drawCount = csmGetDrawableCount(model->model);
	drawNames = csmGetDrawableIds(model->model);
	drawIndex = csmGetDrawableIndices(model->model);
	drawIndexCount = csmGetDrawableIndexCounts(model->model);
	drawMaskCount = csmGetDrawableMaskCounts(model->model);
	drawTex = csmGetDrawableTextureIndices(model->model);
	drawVertCount = csmGetDrawableVertexCounts(model->model);
	drawMask = csmGetDrawableMasks(model->model);
	drawConstFlags = csmGetDrawableConstantFlags(model->model);
	drawUVs = csmGetDrawableVertexUvs(model->model);
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, drawCount);

	for (i = 0; i < drawCount; i++)
	{
		if (namedRet)
		{
			lua_pushstring(L, drawNames[i]);
			lua_createtable(L, 0, 5 + (drawMaskCount[i] > 0 ? 1 : 0));

			/* Index */
			lua_pushlstring(L, "index", 5);
			lua_pushinteger(L, i + 1);
			lua_rawset(L, -3);
		}
		else
		{
			lua_pushinteger(L, i + 1);
			lua_createtable(L, 0, 5 + (drawMaskCount[i] > 0));

			/* Index */
			lua_pushlstring(L, "name", 4);
			lua_pushstring(L, drawNames[i]);
			lua_rawset(L, -3);
		}

		/* Flags */
		lua_pushlstring(L, "flags", 5);
		lua_createtable(L, 0, 2);
		lua_pushlstring(L, "blending", 8);
		if ((drawConstFlags[i] & blendMulAndAdd) == blendMulAndAdd || (drawConstFlags[i] & blendMulAndAdd) == 0)
			lua_pushlstring(L, "normal", 6);
		else if (drawConstFlags[i] & csmBlendAdditive)
			lua_pushlstring(L, "add", 3);
		else if (drawConstFlags[i] & csmBlendMultiplicative)
			lua_pushlstring(L, "multiply", 8);
		lua_rawset(L, -3); /* blending */
		lua_pushlstring(L, "doublesided", 11);
		lua_pushboolean(L, drawConstFlags[i] & csmIsDoubleSided);
		lua_rawset(L, -3); /* doublesided */
		lua_rawset(L, -3); /* flags */
		/* Texture */
		lua_pushlstring(L, "texture", 7);
		lua_pushinteger(L, drawTex[i] + 1);
		lua_rawset(L, -3);
		/* Mask */
		if (drawMaskCount[i] > 0)
		{
			lua_pushlstring(L, "mask", 4);
			lua_createtable(L, drawMaskCount[i], 0);

			if (namedRet)
			{
				for (j = 0; j < drawMaskCount[i]; j++)
				{
					lua_pushinteger(L, j + 1);
					lua_pushstring(L, drawNames[drawMask[i][j]]);
					lua_rawset(L, -3);
				}
			}
			else
			{
				for (j = 0; j < drawMaskCount[i]; j++)
				{
					lua_pushinteger(L, j + 1);
					lua_pushinteger(L, drawMask[i][j] + 1);
					lua_rawset(L, -3);
				}
			}

			lua_rawset(L, -3); /* mask */
		}
		/* Vertex Count */
		lua_pushlstring(L, "vertexCount", 11);
		lua_pushinteger(L, drawVertCount[i]);
		lua_rawset(L, -3);
		/* UVs */
		lua_pushlstring(L, "uv", 2);
		lua_createtable(L, drawVertCount[i] * 2, 0);
		for (j = 0; j < drawVertCount[i]; j++)
		{
			lua_pushinteger(L, j * 2 + 1);
			lua_pushnumber(L, drawUVs[i][j].X);
			lua_rawset(L, -3);
			lua_pushinteger(L, j * 2 + 2);
			lua_pushnumber(L, drawUVs[i][j].X);
			lua_rawset(L, -3);
		}
		lua_rawset(L, -3);
		/* Index Map */
		lua_pushlstring(L, "indexMap", 8);
		lua_createtable(L, drawIndexCount[i], 0);
		for (j = 0; j < drawIndexCount[i]; j++)
		{
			lua_pushinteger(L, j + 1);
			lua_pushinteger(L, drawIndex[i][j] + 1);
			lua_rawset(L, -3);
		}
		lua_rawset(L, -3);

		lua_rawset(L, tableIndex); /* end */
	}

	lua_pushvalue(L, tableIndex);
	return 1;
}

static int l2dw_getDynamicDrawableData(lua_State *L)
{
	ModelDefinition *model;
	int drawCount, namedRet, tableIndex, i, j, alwaysSetVertex;
	const char **drawNames;
	const int *drawOrder, *drawRenderOrder, *drawVertexCount;
	const csmFlags *drawDynFlags;
	const float *drawOpacity;
	const csmVector2 **drawVertex;

	model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	drawCount = csmGetDrawableCount(model->model);
	drawNames = csmGetDrawableIds(model->model);
	drawOrder = csmGetDrawableDrawOrders(model->model);
	drawRenderOrder = csmGetDrawableRenderOrders(model->model);
	drawVertexCount = csmGetDrawableVertexCounts(model->model);
	drawDynFlags = csmGetDrawableDynamicFlags(model->model);
	drawOpacity = csmGetDrawableOpacities(model->model);
	drawVertex = csmGetDrawableVertexPositions(model->model);
	namedRet = l2dh_usertablenamed(L, 2, &tableIndex, drawCount);

	for (i = 0; i < drawCount; i++)
	{
		if (namedRet)
			lua_pushstring(L, drawNames[i]);
		else
			lua_pushinteger(L, i + 1);
		lua_rawget(L, tableIndex);

		/* If it's not a table, create new table */
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			lua_createtable(L, 0, 5);
			if (namedRet)
				lua_pushstring(L, drawNames[i]);
			else
				lua_pushinteger(L, i + 1);
			lua_pushvalue(L, -2);
			lua_rawset(L, tableIndex);
			/* now we have the mutable table at -1 */
		}

		/* drawOrder */
		lua_pushlstring(L, "drawOrder", 9);
		lua_pushinteger(L, drawOrder[i]);
		lua_rawset(L, -3);
		/* renderOrder */
		lua_pushlstring(L, "renderOrder", 11);
		lua_pushinteger(L, drawRenderOrder[i]);
		lua_rawset(L, -3);
		/* opacity */
		lua_pushlstring(L, "opacity", 7);
		lua_pushnumber(L, drawOpacity[i]);
		lua_rawset(L, -3);
		/* Dynamic flags */
		lua_pushlstring(L, "dynamicFlags", 12);
		lua_rawget(L, -2);
		/* If it's not a table, create new table, leaving it at -1 */
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			lua_createtable(L, 0, 6);
			/* Set the new table, leaving the new table at -1 */
			lua_pushlstring(L, "dynamicFlags", 12);
			lua_pushvalue(L, -2);
			lua_rawset(L, -4);
		}
		/* visible - Dynamic flags */
		lua_pushlstring(L, "visible", 7);
		lua_pushboolean(L, drawDynFlags[i] & csmIsVisible);
		lua_rawset(L, -3);
		/* visibilityChanged - Dynamic flags */
		lua_pushlstring(L, "visibilityChanged", 17);
		lua_pushboolean(L, drawDynFlags[i] & csmVisibilityDidChange);
		lua_rawset(L, -3);
		/* opacityChanged - Dynamic flags */
		lua_pushlstring(L, "opacityChanged", 14);
		lua_pushboolean(L, drawDynFlags[i] & csmOpacityDidChange);
		lua_rawset(L, -3);
		/* drawOrderChanged - Dynamic flags */
		lua_pushlstring(L, "drawOrderChanged", 16);
		lua_pushboolean(L, drawDynFlags[i] & csmDrawOrderDidChange);
		lua_rawset(L, -3);
		/* renderOrderChanged - Dynamic flags */
		lua_pushlstring(L, "renderOrderChanged", 18);
		lua_pushboolean(L, drawDynFlags[i] & csmRenderOrderDidChange);
		lua_rawset(L, -3);
		/* vertexChanged - Dynamic flags */
		lua_pushlstring(L, "vertexChanged", 13);
		lua_pushboolean(L, drawDynFlags[i] & csmVertexPositionsDidChange);
		lua_rawset(L, -3);
		/* remove the flags table */
		lua_pop(L, 1);
		/* Vertex positions */
		lua_pushlstring(L, "vertexPosition", 14);
		lua_rawget(L, -2);
		if ((alwaysSetVertex = !lua_istable(L, -1)))
		{
			/* Always re-new vertex positions */
			lua_pop(L, 1);
			lua_createtable(L, drawVertexCount[i] * 2, 0);
			/* Set the new table, leaving the new table at -1 */
			lua_pushlstring(L, "vertexPosition", 14);
			lua_pushvalue(L, -2);
			lua_rawset(L, -4);
		}

		if (alwaysSetVertex || (drawDynFlags[i] & csmVertexPositionsDidChange))
		{
			for (j = 0; j < drawVertexCount[i]; j++)
			{
				lua_pushinteger(L, j * 2 + 1);
				lua_pushnumber(L, drawVertex[i][j].X);
				lua_rawset(L, -3);
				lua_pushinteger(L, j * 2 + 2);
				lua_pushnumber(L, drawVertex[i][j].Y);
				lua_rawset(L, -3);
			}
		}
		/* pop vertex table and the current data table */
		lua_pop(L, 2);
	}

	lua_pushvalue(L, tableIndex);
	return 1;
}

static int l2dw_resetDynamicDrawableFlags(lua_State *L)
{
	ModelDefinition *model = (ModelDefinition *) luaL_checkudata(L, 1, LUALIVE2D_METATABLE_NAME);
	csmResetDrawableDynamicFlags(model->model);
	return 0;
}

/* Libraries to export */
const luaL_Reg l2d_export[] = {
	{"loadModelFromString", &l2d_loadModel},
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
	{"getPartsOpacity", &l2dw_getPartsOpacity},
	{"getDrawableData", &l2dw_getDrawableData},
	{"getDynamicDrawableData", &l2dw_getDynamicDrawableData},
	{"resetDynamicDrawableFlags", &l2dw_resetDynamicDrawableFlags},
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
	lua_pushlstring(L, "__index", 7);
	lua_newtable(L);
	/* Export methods */
	for (i = l2dw_export; i->name != NULL; i++)
	{
		lua_pushstring(L, i->name);
		lua_pushcfunction(L, i->func);
		lua_rawset(L, -3);
	}
	/* Set __index to metatable */
	lua_rawset(L, -3);
	/* set metatable to global module table */
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
	lua_pushfstring(L, "%d.%d.%d", (csmVer & 0xFF000000U) >> 24, (csmVer & 0xFF0000) >> 16, csmVer & 0xFFFF);
	lua_rawset(L, -3);
	/* Module version */
	lua_pushlstring(L, "_VERSION", 8);
	lua_pushlstring(L, "0.1", 3);
	lua_rawset(L, -3);

	return 1;
}

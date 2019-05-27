lua-live2d
==========

Barebone Live2D Cubism 3 SDK for Native binding for Lua 5.1 (with plans to support Lua 5.2 and 5.3 coming
soon, patches welcome).

This Lua C module is meant to be used in conjunction with
[lua-live2d-framework](https://www.github.com/MikuAuahDark/lua-live2d-framework).

Note that only this Lua binding is MIT licensed. Live2D Cubism 3 SDK for Native is proprietary, non-free.

Example Code
------------

```lua
local lualive2dcore = require("lualive2d.core")
print("Live2D Version: "..lualive2dcore.Live2DVersion)
print("LuaJIT FFI table pointer: "..tostring(lualive2dcore.ptr))

-- modelData is the whole model file as string.
local model = lualive2dcore.loadModelFromString(modelData)
-- Model canvas information
local width, height, centerX, centerY, pixelPerUnit = model:readCanvasInfo()
-- Get model parameter defaults
local parameterDefaults = model:getParameterDefault()
for i = 1, #parameterDefaults do
	local parameter = parameterDefaults[i]
	print("Parameter #"..i)
	print("\tName: "..parameter.name)
	print(string.format("\tMinValue: %.4g  MaxValue: %.4g  DefaultValue: %.4g", parameter.min, parameter.max, parameter.default))
end
-- Get parameter current value
local parameterValue = model:getParameterValues()
-- Set parameter values
parameterValue[index] = math.random()
model:setParameterValues(parameterValue)
-- Update model to account for the new parameter values
model:update()
-- Get drawable data
local drawableData = model:getDrawableData()
-- drawableData[index] = {
--     name = drawable data name
--     flags = {
--         blending = normal|add|multiply
--         doublesided = true|false
--     }
--     texture = texture index number (start from 1)
--     mask = {list of draw mask index, start from 1}
--            (maybe nil if there's no mask)
--     vertexCount = amount of vertices for this draw data part
--     uv = texture mapping coordinates list, interleaved as {x, y, x, y, x, y, ...}
--          where #uv == vertexCount * 2
--     indexMap = {list of vertex mapping, 1-based index}
-- }
local dynamicDrawableData = model:getDynamicDrawableData()
-- dynamicDrawableData[index] = {
--     drawOrder = current drawable data draw order
--     renderOrder = current drawable data render order
--     opacity = current drawable data opacity
--     dynamicFlags = {
--         visible = true|false
--         visibilityChanged = true|false
--         opacityChanged = true|false
--         drawOrderChanged = true|false
--         renderOrderChanged = true|false
--         vertexChanged = true|false
--     }
--     vertexPosition = list of vertex position in "units" units interleaved as
--                      {x, y, x, y, x, y, ...}. Multiply by "pixelPerUnits" to get
--                      pixel position of the vertex then add by modelCenterX/Y to
--                      make sure drawing start at (0, 0). This table has size of
--                      #vertexPosition == vertexCount * 2.
-- }
-- Reset dynamic drawable data
model:resetDynamicDrawableFlags()
```

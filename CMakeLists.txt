# Well shit don't blame me, blame Live2D Cubism Native Core uses CMake 3.6
cmake_minimum_required (VERSION 3.6)

###############
# Some checks #
###############

# Prevent in-tree build.
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	message(FATAL_ERROR "Prevented in-tree build!")
endif()

# Check Live2D source/header files
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/include/Live2DCubismCore.h")
	message(FATAL_ERROR "Live2D Cubism 3 SDK for Native is missing!")
endif()

# If it's referenced by other project, that means it's embedded.
get_directory_property(LUALIVE2D_EMBEDDED PARENT_DIRECTORY)

#################
# Project stuff #
#################

project(lua-live2d LANGUAGES C)

if(MSVC)
	option(LUALIVE2D_MT "Build multi-thread (/MT) version of library" OFF)
endif()

# Add Live2D Cubism 3 SDK for Native library
add_subdirectory("live2d/Core")
# Require Lua 5.1
find_package(Lua 5.1 EXACT REQUIRED)

if(BUILD_SHARED_LIBS)
	add_library(lualive2d SHARED src/main.c)
else()
	add_library(lualive2d STATIC src/main.c)
endif()

# fPIC is mandatory!
set_target_properties(lualive2d PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(lualive2d PROPERTIES PREFIX "")

# According to Core CMakeLists.txt, this shouldn't be "OFF" if there are other deps
if(NOT ${CSM_CORE_DEPS} STREQUAL "OFF")
	add_dependencies(lualive2d ${CSM_CORE_DEPS})
endif()

# MSVC-specific.
# MSVC is somewhat messy because we must account for multiple types
if(MSVC)
	target_compile_definitions(lualive2d PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE LUA_BUILD_AS_DLL LUA_LIB)

	# Select correct MSVC version
	if((${MSVC_VERSION} EQUAL 1900) OR (${MSVC_VERSION} GREATER 1900))
		set(_LIVE2LOVE_MSVC_LINK 140)
	else()
		set(_LIVE2LOVE_MSVC_LINK 120)
	endif()

	# Is it 64-bit build?
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_LIVE2LOVE_WINARCH x86_64)
	else()
		set(_LIVE2LOVE_WINARCH x86)
	endif()

	# Are we building with MT switch?
	if(LUALIVE2D_MT)
		set(_LIVE2LOVE_CRT_TYPE MT)
	else()
		set(_LIVE2LOVE_CRT_TYPE MD)
	endif()

	set(_LIVE2LOVE_RELEASE_OPTION "-${_LIVE2LOVE_CRT_TYPE}")
	set(_LIVE2LOVE_DEBUG_OPTION "-${_LIVE2LOVE_CRT_TYPE}d")

	target_compile_options(lualive2d PUBLIC "$<$<CONFIG:DEBUG>:${_LIVE2LOVE_DEBUG_OPTION}>")
	target_compile_options(lualive2d PUBLIC "$<$<CONFIG:RELEASE>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_compile_options(lualive2d PUBLIC "$<$<CONFIG:RELWITHDEBINFO>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_compile_options(lualive2d PUBLIC "$<$<CONFIG:MINSIZEREL>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_link_libraries(lualive2d
		debug ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/lib/windows/${_LIVE2LOVE_WINARCH}/${_LIVE2LOVE_MSVC_LINK}/Live2DCubismCore_${_LIVE2LOVE_CRT_TYPE}d.lib
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/lib/windows/${_LIVE2LOVE_WINARCH}/${_LIVE2LOVE_MSVC_LINK}/Live2DCubismCore_${_LIVE2LOVE_CRT_TYPE}.lib
	)
	message(STATUS "Selected Live2D Core MSVC ver: ${_LIVE2LOVE_MSVC_LINK} (${_LIVE2LOVE_WINARCH})")
else()
	if(UNIX AND NOT RPI AND NOT ANDROID AND NOT APPLE)
		# Unfortunately libLive2DCubismCore.a (provided by ${CSM_CORE_LIBS}) is not compiled
		# with -fPIC, but we need -fPIC, so we can't link with it.
		target_link_libraries(lualive2d ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/dll/linux/x86_64/libLive2DCubismCore.so)
	else()
		target_link_libraries(lualive2d ${CSM_CORE_LIBS})
	endif()
endif()

target_link_libraries(lualive2d ${LUA_LIBRARIES})
target_include_directories(lualive2d PRIVATE ${CSM_CORE_INCLUDE_DIR} ${LUA_INCLUDE_DIR})

#################
# Configuration #
#################

if(LUALIVE2D_EMBEDDED)
	target_compile_definitions(lualive2d PRIVATE LUALIVE2D_EMBEDDED)
endif()

###########
# Install #
###########
install(TARGETS lualive2d DESTINATION lib)

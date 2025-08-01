# Minimum SDK version required to build
set(VS_MINIMUM_BUILD_SDK_VERSION 10.0.22621.0)

if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION VERSION_LESS VS_MINIMUM_BUILD_SDK_VERSION)
  message(FATAL_ERROR "Detected Windows SDK version is ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}.\n"
    "Windows SDK ${VS_MINIMUM_BUILD_SDK_VERSION} or higher is required.\n"
    "INFO: Windows SDKs can be installed from the Visual Studio installer.")
endif()

# -------- Host Settings ---------

set(_gentoolset ${CMAKE_GENERATOR_TOOLSET})
string(REPLACE "host=" "" HOSTTOOLSET "${_gentoolset}")
unset(_gentoolset)

# -------- Architecture settings ---------

if(CMAKE_GENERATOR_PLATFORM STREQUAL arm64)
  set(ARCH arm64)
  set(SDK_TARGET_ARCH arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ARCH win32)
  set(SDK_TARGET_ARCH x86)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH x64)
  set(SDK_TARGET_ARCH x64)
endif()

# -------- Paths (mainly for find_package) ---------

set(PLATFORM_DIR platform/win32)
set(APP_RENDER_SYSTEM dx11)

set(CORE_MAIN_SOURCE ${CMAKE_SOURCE_DIR}/xbmc/platform/win32/WinMain.cpp)

# Precompiled headers fail with per target output directory. (needs CMake 3.1)
set(PRECOMPILEDHEADER_DIR ${PROJECT_BINARY_DIR}/${CORE_BUILD_CONFIG}/objs)
set(CMAKE_SYSTEM_NAME Windows)
set(DEPS_FOLDER_RELATIVE project/BuildDependencies)
# ToDo: currently host build tools are hardcoded to win32
# If we ever allow package.native other than 0_package.native-win32.list we will want to
# adapt this based on host
set(NATIVEPREFIX ${CMAKE_SOURCE_DIR}/${DEPS_FOLDER_RELATIVE}/tools)
set(DEPENDS_PATH ${CMAKE_SOURCE_DIR}/${DEPS_FOLDER_RELATIVE}/${ARCH})
set(MINGW_LIBS_DIR ${CMAKE_SOURCE_DIR}/${DEPS_FOLDER_RELATIVE}/mingwlibs/${ARCH})

# mingw libs
list(APPEND CMAKE_PREFIX_PATH ${MINGW_LIBS_DIR})
list(APPEND CMAKE_LIBRARY_PATH ${MINGW_LIBS_DIR}/bin)

if(NOT TARBALL_DIR)
  set(TARBALL_DIR "${CMAKE_SOURCE_DIR}/project/BuildDependencies/downloads")
endif()

# -------- Compiler options ---------

# Allow to use UTF-8 strings in the source code, disable MSVC charset conversion
add_options(CXX ALL_BUILDS "/utf-8")
add_options(CXX ALL_BUILDS "/wd\"4996\"")
set(ARCH_DEFINES -D_WINDOWS -DTARGET_WINDOWS -DTARGET_WINDOWS_DESKTOP)

# Do not add SSE flags for ARM64
if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL arm64)
  list(APPEND ARCH_DEFINES -D__SSE__ -D__SSE2__)
endif()

set(SYSTEM_DEFINES -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DHAS_DX -D__STDC_CONSTANT_MACROS
                   -DTAGLIB_STATIC -DNPT_CONFIG_ENABLE_LOGGING
                   -DPLT_HTTP_DEFAULT_USER_AGENT="UPnP/1.0 DLNADOC/1.50 Kodi"
                   -DPLT_HTTP_DEFAULT_SERVER="UPnP/1.0 DLNADOC/1.50 Kodi"
                   -DUNICODE -D_UNICODE
                   -DFRIBIDI_STATIC
                   $<$<CONFIG:Debug>:-DD3D_DEBUG_INFO>)

# Additional SYSTEM_DEFINES
list(APPEND SYSTEM_DEFINES -DHAS_WIN32_NETWORK)

# The /MP option enables /FS by default.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  if(DEFINED ENV{MAXTHREADS})
    set(MP_FLAG "/MP$ENV{MAXTHREADS}")
  else()
    set(MP_FLAG "/MP")
  endif()

  set(CMAKE_CXX_FLAGS "/permissive- ${MP_FLAG} ${CMAKE_CXX_FLAGS}")
endif()

# Google Test needs to use shared version of runtime libraries
set(gtest_force_shared_crt ON CACHE STRING "" FORCE)


# -------- Linker options ---------

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

# For #pragma comment(lib X)
# TODO: It would certainly be better to handle these libraries via CMake modules.
link_directories(${DEPENDS_PATH}/lib)

# Additional libraries
list(APPEND DEPLIBS bcrypt.lib d3d11.lib DInput8.lib DSound.lib winmm.lib Mpr.lib Iphlpapi.lib WS2_32.lib
                    PowrProf.lib setupapi.lib Shlwapi.lib dwmapi.lib dxguid.lib DelayImp.lib WindowsApp.lib)

# NODEFAULTLIB option
set(_nodefaultlibs_RELEASE libcmt)
set(_nodefaultlibs_DEBUG libcmt msvcrt)
foreach(_lib ${_nodefaultlibs_RELEASE})
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /NODEFAULTLIB:\"${_lib}\"")
endforeach()
foreach(_lib ${_nodefaultlibs_DEBUG})
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:\"${_lib}\"")
endforeach()

# DELAYLOAD option
set(_delayloadlibs libmariadb.dll libxslt.dll dnssd.dll dwmapi.dll sqlite3.dll d3dcompiler_47.dll)
foreach(_lib ${_delayloadlibs})
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DELAYLOAD:\"${_lib}\"")
endforeach()

# Make the Release version create a PDB
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
# Minimize the size or the resulting DLLs
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF")


# -------- Visual Studio options ---------

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

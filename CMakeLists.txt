cmake_minimum_required(VERSION 3.12...4.0)

project(pvz95-coop LANGUAGES C CXX)

include(CMake/ProjectVersion.cmake)

option(BUILD_STATIC "Statically link dependencies for a standalone exe (Windows MinGW, Linux)" OFF)
if(BUILD_STATIC)
	if(WIN32 AND NOT MSVC)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
		set(BUILD_SHARED_LIBS OFF)
		add_link_options(-static)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
		set(BUILD_SHARED_LIBS OFF)
	elseif(MSVC)
		message(WARNING "BUILD_STATIC is not supported for MSVC. Use a vcpkg -static triplet instead.")
		set(BUILD_STATIC OFF)
	else()
		message(WARNING "BUILD_STATIC is only supported on Windows with MinGW-based toolchains and on Linux.")
		set(BUILD_STATIC OFF)
	endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_GENERATOR MATCHES "Ninja")
	set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

if(MSVC)
	if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	elseif(DEFINED VCPKG_TARGET_TRIPLET)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS
	src/*.cpp
	src/SexyAppFramework/*.cpp
	src/SexyAppFramework/graphics/*.cpp
	src/SexyAppFramework/imagelib/*.cpp
	src/SexyAppFramework/misc/*.cpp
	src/SexyAppFramework/paklib/*.cpp
	src/SexyAppFramework/sound/*.cpp
	src/SexyAppFramework/widget/*.cpp
	src/SexyAppFramework/fcaseopen/*.c
	src/PvzpLib/*.cpp
	src/Lawn/*.cpp
	src/Lawn/Widget/*.cpp
	src/Lawn/System/*.cpp
	src/GameRules/*.cpp
	src/Multiplayer/*.cpp
)

set(LOW_MEMORY OFF CACHE BOOL "Enable low memory mode")
if(NINTENDO_SWITCH)
	set(VORBIS_LIB vorbisfile vorbis)
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch
	)
elseif(EMSCRIPTEN)
	# libopenmpt has no Emscripten port; must be pre-built with emmake.
	find_library(OPENMPT_LIBRARY NAMES openmpt libopenmpt REQUIRED)
	set(OPENMPT_LIB ${OPENMPT_LIBRARY})

	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/emscripten/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/emscripten
	)
else()
	if(VCPKG_TOOLCHAIN)
		find_package(Vorbis CONFIG REQUIRED)
		find_package(Ogg CONFIG REQUIRED)
		find_package(libopenmpt CONFIG REQUIRED)
		find_package(mpg123 CONFIG REQUIRED)
		set(VORBIS_LIB Vorbis::vorbis Vorbis::vorbisfile)
		set(OGG_LIB Ogg::ogg)
		set(OPENMPT_LIB libopenmpt::libopenmpt)
		set(MPG123_LIB MPG123::libmpg123)
	else()
		find_library(OGG_LIBRARY NAMES ogg libogg REQUIRED)
		find_library(VORBIS_LIBRARY NAMES vorbis libvorbis REQUIRED)
		find_library(VORBISFILE_LIBRARY NAMES vorbisfile libvorbisfile REQUIRED)
		find_library(OPENMPT_LIBRARY NAMES openmpt libopenmpt REQUIRED)
		find_library(MPG123_LIBRARY NAMES mpg123 libmpg123 REQUIRED)

		set(VORBIS_LIB ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY})
		set(OGG_LIB ${OGG_LIBRARY})
		set(OPENMPT_LIB ${OPENMPT_LIBRARY})
		set(MPG123_LIB ${MPG123_LIBRARY})
	endif()
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default
	)
endif()

if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
	set(WIN_CONSOLE_DEFAULT ON)
	set(PVZ_DEBUG_DEFAULT ON)
else()
	set(WIN_CONSOLE_DEFAULT OFF)
	set(PVZ_DEBUG_DEFAULT OFF)
endif()

option(PVZ_DEBUG "Enable PVZ_DEBUG macro to enable cheats, hidden features, etc." ${PVZ_DEBUG_DEFAULT})
option(CONSOLE "Show console on Windows" ${WIN_CONSOLE_DEFAULT})
option(DO_FIX_BUGS "Define DO_FIX_BUGS macro (Community fixes for original game bugs of 1.2.0.1073 GOTY Edition)" OFF)

if(EMSCRIPTEN)
	list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_MPG123=1")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_MPG123=1 -fexceptions")
else()
	find_package(Threads REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(PNG REQUIRED)
	find_package(SDL2 REQUIRED)
	if(WIN32 OR (APPLE AND NOT IOS))
		find_package(Freetype REQUIRED)
		set(PVZ_HAS_FREETYPE TRUE)
	endif()
	if(NOT WIN32 AND NOT NINTENDO_SWITCH)
		find_package(Iconv REQUIRED)
		set(PVZ_USE_SYSTEM_ICONV TRUE)
	endif()
endif()

function(_pvz_patch_sdl2_iface_libs target)
	get_target_property(_libs ${target} INTERFACE_LINK_LIBRARIES)
	if(NOT _libs)
		return()
	endif()
	set(_patched)
	foreach(_lib IN LISTS _libs)
		if(_lib STREQUAL "SDL2::SDL2")
			list(APPEND _patched SDL2::SDL2-static)
		else()
			list(APPEND _patched "${_lib}")
		endif()
	endforeach()
	set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_patched}")
endfunction()

if(BUILD_STATIC AND TARGET SDL2::SDL2-static)
	if(TARGET SDL2::SDL2main)
		_pvz_patch_sdl2_iface_libs(SDL2::SDL2main)
	endif()
	set(SDL2_LINK_TARGET SDL2::SDL2-static)
else()
	set(SDL2_LINK_TARGET SDL2::SDL2)
endif()

set(SDL_MIXER_X_STATIC ON CACHE BOOL "Build static library of SDL Mixer X")
set(SDL_MIXER_X_SHARED OFF CACHE BOOL "Build shared library of SDL Mixer X")
set(USE_MIDI OFF CACHE BOOL "SDL Mixer X midi")
set(USE_MODPLUG OFF CACHE BOOL "SDL Mixer X modplug")
set(USE_XMP OFF CACHE BOOL "SDL Mixer X xmp")
set(USE_OPUS OFF CACHE BOOL "SDL Mixer X opus")
set(USE_DRFLAC OFF CACHE BOOL "SDL Mixer X flac")
set(USE_PXTONE OFF CACHE BOOL "SDL Mixer X pxtone")
add_subdirectory(src/SexyAppFramework/sound/SDL-Mixer-X)

# Patch SDL-Mixer-X's transitive SDL2 dep without touching vendored CMake files
if(BUILD_STATIC AND TARGET SDL2::SDL2-static)
	_pvz_patch_sdl2_iface_libs(SDL2_mixer_ext_Static)
endif()

# Android requires a shared library loaded by SDLActivity; other platforms use an executable.
if(ANDROID)
	add_library(pvz-portable SHARED ${SOURCES})
elseif(IOS)
	add_executable(pvz-portable MACOSX_BUNDLE ${SOURCES})
elseif(APPLE)
	configure_file(macos/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/macos/Info.plist" @ONLY)
	add_executable(pvz-portable MACOSX_BUNDLE ${SOURCES})
else()
	add_executable(pvz-portable ${SOURCES})
endif()

target_include_directories(pvz-portable PRIVATE
	${PLAT_INCLUDES}
	${PROJECT_BINARY_DIR}
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/src/SexyAppFramework
	${PROJECT_SOURCE_DIR}/src/SexyAppFramework/sound/SDL-Mixer-X/include
	${SDL2_INCLUDE_DIRS}
)
target_compile_definitions(pvz-portable PRIVATE
	$<$<BOOL:${PVZ_DEBUG}>:PVZ_DEBUG>
	$<$<BOOL:${DO_FIX_BUGS}>:DO_FIX_BUGS>
	$<$<BOOL:${PVZ_USE_SYSTEM_ICONV}>:PVZ_USE_SYSTEM_ICONV>
	$<$<BOOL:${PVZ_HAS_FREETYPE}>:PVZ_HAS_FREETYPE>
)
target_compile_features(pvz-portable PRIVATE cxx_std_20)
set_target_properties(pvz-portable PROPERTIES
	CXX_EXTENSIONS OFF
	OUTPUT_NAME pvz95-coop
)
if(APPLE AND NOT IOS)
	set_target_properties(pvz-portable PROPERTIES
		MACOSX_BUNDLE_BUNDLE_NAME "PvZ 95 Co-op"
		MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.langqi99.pvz95-coop"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/macos/Info.plist"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${PVZP_VERSION_PLAIN}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${PVZP_BUILD_NUMBER}"
	)
endif()

include(CTest)
if(BUILD_TESTING AND NOT ANDROID AND NOT EMSCRIPTEN AND NOT NINTENDO_SWITCH AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
	add_executable(pvz95-rules-tests
		tests/RulesetTests.cpp
		src/GameRules/Ruleset.cpp
	)
	target_include_directories(pvz95-rules-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-rules-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-rules-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-rules COMMAND pvz95-rules-tests)

	add_executable(pvz95-encoding-tests
		tests/EncodingTests.cpp
		src/SexyAppFramework/misc/Buffer.cpp
	)
	target_include_directories(pvz95-encoding-tests PRIVATE
		${PROJECT_SOURCE_DIR}/src
		${PROJECT_SOURCE_DIR}/src/SexyAppFramework
		${SDL2_INCLUDE_DIRS}
	)
	target_compile_features(pvz95-encoding-tests PRIVATE cxx_std_20)
	target_link_libraries(pvz95-encoding-tests PRIVATE ${SDL2_LINK_TARGET})
	if(PVZ_USE_SYSTEM_ICONV)
		target_compile_definitions(pvz95-encoding-tests PRIVATE PVZ_USE_SYSTEM_ICONV)
		target_link_libraries(pvz95-encoding-tests PRIVATE Iconv::Iconv)
	endif()
	set_target_properties(pvz95-encoding-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-encoding COMMAND pvz95-encoding-tests)

	add_executable(pvz95-protocol-tests
		tests/ProtocolTests.cpp
		src/Multiplayer/Protocol.cpp
	)
	target_include_directories(pvz95-protocol-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-protocol-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-protocol-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-protocol COMMAND pvz95-protocol-tests)

	add_executable(pvz95-discovery-tests
		tests/LanDiscoveryTests.cpp
		src/Multiplayer/LanDiscovery.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
	)
	target_include_directories(pvz95-discovery-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-discovery-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-discovery-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-discovery COMMAND pvz95-discovery-tests)

	add_executable(pvz95-lobby-tests
		tests/LobbyTests.cpp
		src/Multiplayer/Lobby.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/SharedInputState.cpp
	)
	target_include_directories(pvz95-lobby-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-lobby-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-lobby-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-lobby COMMAND pvz95-lobby-tests)

	add_executable(pvz95-stream-tests
		tests/PacketStreamTests.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
	)
	target_include_directories(pvz95-stream-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-stream-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-stream-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-stream COMMAND pvz95-stream-tests)

	add_executable(pvz95-tcp-tests
		tests/TcpSocketTests.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
	)
	target_include_directories(pvz95-tcp-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-tcp-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-tcp-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-tcp COMMAND pvz95-tcp-tests)

	add_executable(pvz95-channel-tests
		tests/ReliableChannelTests.cpp
		src/Multiplayer/ReliableChannel.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
	)
	target_include_directories(pvz95-channel-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-channel-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-channel-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-channel COMMAND pvz95-channel-tests)

	add_executable(pvz95-host-session-tests
		tests/HostSessionTests.cpp
		src/Multiplayer/HostSession.cpp
		src/Multiplayer/Lobby.cpp
		src/Multiplayer/ReliableChannel.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
		src/Multiplayer/SharedInputState.cpp
	)
	target_include_directories(pvz95-host-session-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-host-session-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-host-session-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-host-session COMMAND pvz95-host-session-tests)

	add_executable(pvz95-client-session-tests
		tests/ClientSessionTests.cpp
		src/Multiplayer/ClientSession.cpp
		src/Multiplayer/HostSession.cpp
		src/Multiplayer/Lobby.cpp
		src/Multiplayer/ReliableChannel.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
		src/Multiplayer/SharedInputState.cpp
	)
	target_include_directories(pvz95-client-session-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-client-session-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-client-session-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-client-session COMMAND pvz95-client-session-tests)

	add_executable(pvz95-coordinator-tests
		tests/LanCoordinatorTests.cpp
		src/Multiplayer/LanCoordinator.cpp
		src/Multiplayer/LanDiscovery.cpp
		src/Multiplayer/ClientSession.cpp
		src/Multiplayer/HostSession.cpp
		src/Multiplayer/Lobby.cpp
		src/Multiplayer/ReliableChannel.cpp
		src/Multiplayer/PacketStream.cpp
		src/Multiplayer/Protocol.cpp
		src/Multiplayer/Socket.cpp
		src/Multiplayer/SharedInputState.cpp
	)
	target_include_directories(pvz95-coordinator-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-coordinator-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-coordinator-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-coordinator COMMAND pvz95-coordinator-tests)

	add_executable(pvz95-shared-input-tests
		tests/SharedInputStateTests.cpp
		src/Multiplayer/SharedInputState.cpp
	)
	target_include_directories(pvz95-shared-input-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-shared-input-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-shared-input-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-shared-input COMMAND pvz95-shared-input-tests)

	add_executable(pvz95-session-barrier-tests
		tests/SessionBarrierTests.cpp
		src/Multiplayer/SessionBarrier.cpp
	)
	target_include_directories(pvz95-session-barrier-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-session-barrier-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-session-barrier-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-session-barrier COMMAND pvz95-session-barrier-tests)

	add_executable(pvz95-action-timeline-tests
		tests/ActionTimelineTests.cpp
		src/Multiplayer/ActionTimeline.cpp
	)
	target_include_directories(pvz95-action-timeline-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-action-timeline-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-action-timeline-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-action-timeline COMMAND pvz95-action-timeline-tests)

	add_executable(pvz95-state-hash-timeline-tests
		tests/StateHashTimelineTests.cpp
		src/Multiplayer/StateHashTimeline.cpp
	)
	target_include_directories(pvz95-state-hash-timeline-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-state-hash-timeline-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-state-hash-timeline-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-state-hash-timeline COMMAND pvz95-state-hash-timeline-tests)

	add_executable(pvz95-local-input-tests
		tests/LocalInputTests.cpp
	)
	target_include_directories(pvz95-local-input-tests PRIVATE
		${PROJECT_SOURCE_DIR}/src
		${PROJECT_SOURCE_DIR}/src/SexyAppFramework
	)
	target_compile_features(pvz95-local-input-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-local-input-tests PROPERTIES CXX_EXTENSIONS OFF)
	add_test(NAME pvz95-local-input COMMAND pvz95-local-input-tests)

	add_executable(pvz95-deterministic-hash-tests
		tests/DeterministicHashTests.cpp
		src/Multiplayer/DeterministicHash.cpp
	)
	target_include_directories(pvz95-deterministic-hash-tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
	target_compile_features(pvz95-deterministic-hash-tests PRIVATE cxx_std_20)
	set_target_properties(pvz95-deterministic-hash-tests PROPERTIES CXX_EXTENSIONS OFF)
	target_compile_definitions(pvz95-deterministic-hash-tests PRIVATE PVZ95_HASH_PRIMITIVES_ONLY)
	add_test(NAME pvz95-deterministic-hash COMMAND pvz95-deterministic-hash-tests)

	if(MSVC)
		target_compile_options(pvz95-rules-tests PRIVATE /utf-8)
		target_compile_options(pvz95-encoding-tests PRIVATE /utf-8)
		target_compile_options(pvz95-protocol-tests PRIVATE /utf-8)
		target_compile_options(pvz95-discovery-tests PRIVATE /utf-8)
		target_compile_options(pvz95-lobby-tests PRIVATE /utf-8)
		target_compile_options(pvz95-stream-tests PRIVATE /utf-8)
		target_compile_options(pvz95-tcp-tests PRIVATE /utf-8)
		target_compile_options(pvz95-channel-tests PRIVATE /utf-8)
		target_compile_options(pvz95-host-session-tests PRIVATE /utf-8)
		target_compile_options(pvz95-client-session-tests PRIVATE /utf-8)
		target_compile_options(pvz95-coordinator-tests PRIVATE /utf-8)
		target_compile_options(pvz95-shared-input-tests PRIVATE /utf-8)
		target_compile_options(pvz95-session-barrier-tests PRIVATE /utf-8)
		target_compile_options(pvz95-action-timeline-tests PRIVATE /utf-8)
		target_compile_options(pvz95-state-hash-timeline-tests PRIVATE /utf-8)
		target_compile_options(pvz95-local-input-tests PRIVATE /utf-8)
		target_compile_options(pvz95-deterministic-hash-tests PRIVATE /utf-8)
		target_link_libraries(pvz95-tcp-tests PRIVATE ws2_32)
		target_link_libraries(pvz95-channel-tests PRIVATE ws2_32)
		target_link_libraries(pvz95-host-session-tests PRIVATE ws2_32)
		target_link_libraries(pvz95-client-session-tests PRIVATE ws2_32)
		target_link_libraries(pvz95-coordinator-tests PRIVATE ws2_32)
	endif()

	if(WIN32)
		target_link_libraries(pvz95-discovery-tests PRIVATE ws2_32)
	endif()
endif()

if(WIN32)
	target_link_libraries(pvz-portable PRIVATE ws2_32)
elseif(PVZ_USE_SYSTEM_ICONV)
	target_link_libraries(pvz-portable PRIVATE Iconv::Iconv)
endif()

if (LOW_MEMORY)
	target_compile_definitions(pvz-portable PRIVATE LOW_MEMORY)
endif()

if(EMSCRIPTEN)
	set_property(TARGET pvz-portable APPEND PROPERTY LINK_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/wasm/shell.html"
	)
	target_link_libraries(pvz-portable PRIVATE
		SDL2_mixer_ext_Static
		${OPENMPT_LIB}
	)
	target_link_options(pvz-portable PRIVATE
		"SHELL:-sUSE_SDL=2"
		"SHELL:-sUSE_ZLIB=1"
		"SHELL:-sUSE_LIBPNG=1"
		"SHELL:-sUSE_LIBJPEG=1"
		"SHELL:-sUSE_OGG=1"
		"SHELL:-sUSE_VORBIS=1"
		"SHELL:-sUSE_MPG123=1"
		"SHELL:-fexceptions"
		"SHELL:-sASYNCIFY"
		"SHELL:-sALLOW_MEMORY_GROWTH=1"
		"SHELL:-sMAXIMUM_MEMORY=1073741824"
		"SHELL:-sGROWABLE_ARRAYBUFFERS=0"
		"SHELL:-sFORCE_FILESYSTEM=1"
		"SHELL:-sEXPORTED_RUNTIME_METHODS=[\"FS\",\"callMain\"]"
		"SHELL:-sINVOKE_RUN=0"
		"SHELL:-lidbfs.js"
		"SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/wasm/shell.html"
	)
	set_target_properties(pvz-portable PROPERTIES SUFFIX ".html")
else()
	target_link_libraries(pvz-portable PRIVATE
		SDL2_mixer_ext_Static 
		${OPENMPT_LIB} 
		${MPG123_LIB} 
		${VORBIS_LIB} 
		${OGG_LIB} 
		PNG::PNG
		JPEG::JPEG
		ZLIB::ZLIB
		SDL2::SDL2main
		${SDL2_LINK_TARGET}
		Threads::Threads
		$<$<BOOL:${PVZ_HAS_FREETYPE}>:Freetype::Freetype>
	)
endif()

if (WIN32)
	if(MSVC)
		target_compile_options(pvz-portable PRIVATE /utf-8)
	endif()

	target_sources(pvz-portable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/LawnProject.rc)

	set_source_files_properties(
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/LawnProject.rc
		PROPERTIES
			COMPILE_DEFINITIONS "APP_ICON_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/icon.ico\""
			OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico"
	)

	target_link_libraries(pvz-portable PRIVATE ws2_32 user32 gdi32 winmm imm32 shlwapi)
	target_compile_definitions(pvz-portable PRIVATE WINDOWS)

	if(NOT CONSOLE)
		if(MSVC)
			set_target_properties(pvz-portable PROPERTIES WIN32_EXECUTABLE TRUE)
		else()
			target_link_options(pvz-portable PRIVATE -mwindows)
		endif()
	endif()
elseif (NINTENDO_SWITCH)
	target_link_libraries(pvz-portable PRIVATE GLESv2 EGL glapi drm_nouveau)

	nx_generate_nacp(pvz-portable.nacp
		NAME "PvZ Portable"
		AUTHOR "wszqkzqk, etc., a community-driven port"
		VERSION "${PVZP_VERSION_PLAIN}"
	)

	nx_create_nro(pvz-portable
		NACP pvz-portable.nacp
		ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon-switch.jpg
	)
elseif (ANDROID)
	target_link_libraries(pvz-portable PRIVATE GLESv2 EGL android log)
	set_target_properties(pvz-portable PROPERTIES OUTPUT_NAME "main")
elseif (IOS)
	target_link_libraries(pvz-portable PRIVATE
		"-framework Foundation"
		"-framework UIKit"
		"-framework OpenGLES"
		"-framework QuartzCore"
		"-framework CoreGraphics"
		"-framework CoreMotion"
		"-framework GameController"
		"-framework AVFoundation"
		"-framework AudioToolbox"
		"-framework Metal"
	)

	configure_file(ios/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
	set_target_properties(pvz-portable PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.wszqkzqk.pvz-portable"
		MACOSX_BUNDLE_BUNDLE_NAME "PvZ Portable"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
		XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
		XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.github.wszqkzqk.pvz-portable"
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
		XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
	)

	# Add launch storyboard to the bundle
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard")
		target_sources(pvz-portable PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard")
		set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard"
			PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	endif()

	# Add asset catalog (app icon)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
		target_sources(pvz-portable PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
		set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets"
			PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	endif()
endif()

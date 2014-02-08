project(farsh)

add_subdirectory(../inanity inanity)

include_directories(
	../inanity/deps/bullet/repo/src
	../inanity/deps/freetype/repo/include
	../inanity/deps/harfbuzz/generated
)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# target for copying required files to binary dir
add_custom_target(copy)
add_custom_command(TARGET copy PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/D3Dcompiler_47.dll ${CMAKE_CURRENT_BINARY_DIR}/D3Dcompiler_47.dll
)
add_custom_command(TARGET copy PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/assets
)
add_custom_command(TARGET copy PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)

add_executable(farsh WIN32
	# sources
	main.cpp
	meta.cpp
	Geometry.cpp
	GeometryFormats.cpp
	Material.cpp
	Painter.cpp
	Game.cpp
	Skeleton.cpp
	BoneAnimation.cpp
)
target_link_libraries(farsh
	# inanity
	inanity-platform
	inanity-dx11
	inanity-gl
	inanity-graphics
	inanity-platform
	inanity-shaders
	inanity-input
	inanity-bullet
	inanity-physics
	inanity-gui
	inanity-crypto
	inanity-sqlitefs
	inanity-sqlite
	inanity-data
	inanity-lua
	inanity-base
	# inanity deps
	inanity-dep-lua
	inanity-dep-bullet-dynamics
	inanity-dep-bullet-collision
	inanity-dep-bullet-linearmath
	inanity-dep-libpng
	inanity-dep-zlib
	inanity-dep-glew
	inanity-dep-sqlite
	inanity-dep-freetype
	inanity-dep-harfbuzz
	# external
	${OPENGL_LIBRARIES}
)
add_dependencies(farsh copy)

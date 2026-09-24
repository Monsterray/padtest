# PadTest on PSn00bSDK: cmake --preset default . && cmake --build build
cmake_minimum_required(VERSION 3.21)

project(
	padtest
	LANGUAGES    C ASM
	VERSION      2.3.0
	DESCRIPTION  "PadTest DX: PlayStation controller tester"
)

# The title bar shows the version and the date of the commit built. Versions: 2.0.0 the
# PSn00bSDK port, then a minor step for a change in what the program does and a patch step
# for a fix; a commit that changes only CI or docs keeps the version.
execute_process(
	COMMAND git log -1 --format=%cs
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE PADTEST_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
)
if(NOT PADTEST_DATE)
	set(PADTEST_DATE "unknown date")
endif()
# Configure again after each commit, so the date follows it
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git/logs/HEAD)
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/.git/logs/HEAD)
endif()

option(PADTEST_WERROR "Treat compiler warnings as errors" OFF)

psn00bsdk_add_executable(padtest GPREL padtest.c controllers.c graphics.c text.c gs.c)

set_target_properties(padtest PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
target_include_directories(padtest SYSTEM PRIVATE ${PSN00BSDK_INCLUDE})
target_compile_definitions(
	padtest PRIVATE
		PADTEST_VERSION="${PROJECT_VERSION}"
		PADTEST_DATE="${PADTEST_DATE}"
)
target_compile_options(
	padtest PRIVATE
		-Wall -Wextra -Wpedantic
		-Wshadow -Wundef -Wcast-qual -Wcast-align -Wpointer-arith -Wwrite-strings
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition
		-Wconversion -Wsign-conversion -Wdouble-promotion -Wfloat-equal
		-Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2
		-Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wnull-dereference
		-Wstack-usage=1024 -Wvla -Wredundant-decls -Wjump-misses-init
		$<$<BOOL:${PADTEST_WERROR}>:-Werror>
)

psn00bsdk_add_cd_image(iso padtest iso.xml DEPENDS padtest system.cnf)

# PadTest on PSn00bSDK: cmake --preset default . && cmake --build build
cmake_minimum_required(VERSION 3.21)

project(
	padtest
	LANGUAGES    C ASM
	VERSION      1.2.0
	DESCRIPTION  "PadTest DX: PlayStation controller tester"
)

option(PADTEST_WERROR "Treat compiler warnings as errors" OFF)

psn00bsdk_add_executable(padtest GPREL padtest.c controllers.c graphics.c text.c gs.c)

set_target_properties(padtest PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
target_include_directories(padtest SYSTEM PRIVATE ${PSN00BSDK_INCLUDE})
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

# PadTest on PSn00bSDK: cmake --preset default . && cmake --build build
cmake_minimum_required(VERSION 3.21)

project(
	padtest
	LANGUAGES    C ASM
	VERSION      1.2.0
	DESCRIPTION  "PadTest DX: PlayStation controller tester"
)

psn00bsdk_add_executable(padtest GPREL padtest.c controllers.c graphics.c text.c gs.c)

psn00bsdk_add_cd_image(iso padtest iso.xml DEPENDS padtest system.cnf)

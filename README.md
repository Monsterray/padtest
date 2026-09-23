# PadTest DX
[![build](https://github.com/Monsterray/padtest/actions/workflows/build.yml/badge.svg)](https://github.com/Monsterray/padtest/actions/workflows/build.yml)
### Gamepad test application for PlayStation 1

A fork of Shendo and ggrtk's PadTest 1.1, ported to PSn00bSDK and extended for emulator
debugging (WiiStation). It also shows, for each port:
* the raw reply to the last poll (ID, 5Ah, data bytes, in pairs);
* the reply length, the config commands answered as a DualShock does (`cfg n/10`) and the
  button bits pressed so far (`btn n`);
* for analog pads, how many distinct values each axis gave (`axes LX LY RX RY`).

Each frame it also copies a status block to VRAM at (640,256), 64x8 halfwords, layout in
`include/dx.h`: the last poll, the time from each byte to its /ACK and the time a byte took
(root counter 2 at the system clock), the replies to config commands 43h 45h 46h 47h 4Ch
44h 4Dh, per-button press counts and a bit map of every raw axis value. An emulator VRAM
dump holds it; WiiStation's `scripts/padtest_dx.py` decodes it.

The controller port is driven as the hardware requires (psx-spx, PSn00bSDK's pads example):
/CS low 20 us before the first byte, /ACK awaited for up to 100 us after each byte but the
last, the /ACK flag cleared only after /ACK is high again, and the reply length taken from
the ID byte.

![padtestscreen](https://raw.githubusercontent.com/ShendoXT/padtest/master/images/screenshot.png)
## Supported controllers:
* Digital (SCPH-1080) controller
* DualShock analog (SCPH-1200) controller
* PlayStation Mouse

## Requirements:
* A way to run homebrew on PlayStation, be it modchip, cart, swap method or FreePSXBoot.
* (For developers) PSn00bSDK 0.24 or later (https://github.com/Lameguy64/PSn00bSDK), with
  `PSN00BSDK_LIBS` set to its `lib/libpsn00b` folder, and CMake 3.21 or later.

## How to compile:
`cmake --preset default .` and then `cmake --build build`. The output is `build/padtest.exe`
and the CD image `build/padtest.bin` + `build/padtest.cue`. `build.sh` does the same on a
machine where another cmake comes first on the PATH.

The images are TIM files converted to C arrays (`images/*.h`, `include/font.h`).

The build uses strict warnings (`-Wall -Wextra -Wpedantic -Wconversion` and more, see
`CMakeLists.txt`); `-DPADTEST_WERROR=ON` makes them errors. CI (`.github/workflows/build.yml`)
builds with that on, runs cppcheck, and keeps the CD image as an artifact.

### Usage:
Connect a controller of your choice to either port and test it's buttons.

Analog controllers should automatically switch to analog "red led mode".    
To test rumble press L3 for big motor and R3 for small motor.

This software is intended to be ran on the actual PlayStation 1 or PSone console.
Since it's using direct memory access to SIO ports it may not work on emulators or other consoles (PlayStation 2).

## FreePSXBoot
Included in the release is a UPX compressed executable.<br>
It is identical in functionality but is smaller (37 Kb) because it is compressed.<br>
It can be used with FreePSXBoot and ran directly on boot as it fits on a MemoryCard.
Make sure to use -fastload option while building the memory card image.

# PadTest DX
[![build](https://github.com/Monsterray/padtest/actions/workflows/build.yml/badge.svg)](https://github.com/Monsterray/padtest/actions/workflows/build.yml)
### Gamepad test application for PlayStation 1

A fork of Shendo and ggrtk's PadTest 1.1, ported to PSn00bSDK and extended for emulator
debugging (WiiStation). It reads both ports the way a PS1 game does, multitaps included:

* A **multitap** (SCPH-1070) on either port is detected and all four slots are read, every
  frame, with both methods games use: the long read of all four slots (TAP byte 01h, the
  reply 80h 5Ah and 8 bytes a slot, on the transfer after the request) and single-slot
  reads (first byte 01h..04h). Any mix works: two multitaps (eight players), slots empty or
  filled, a pad on one port and a multitap on the other, a multitap plugged in or pulled out
  while it runs.
* Per slot (or per port without a multitap) it shows the ID, the config test's result
  (`cfg n/16`: config replies that match a PS1 DualShock), the button bits pressed so far
  (`btn n`), the data of the last single read and the slot's block of the long read
  (`L...`). For a pad on its own port it draws the controller as PadTest did, with the raw
  reply, the reply length and, for analog pads, the distinct values each axis gave.
* Under each multitap, `probe A B C ok|differs`: the multitap's request rules tried once
  (docs/PROTOCOL.md), where psx-spx, Mednafen and DuckStation do not all agree. On a real
  multitap that line says which is right.

Each frame it also copies a status block to VRAM at (640,256), 64x32 halfwords, layout in
`include/dx.h` (version 4): per port the multitap's state and probe results; per slot the
last read, the time from each byte to its /ACK and the time a byte took (root counter 2 at
the system clock), the replies to config commands 43h 45h 46h 47h 4Ch 44h 4Dh, per-button
press counts and first-press frames, a bit map of every raw axis value and the slot's long
read block; and the frame's time budget in scanlines. An emulator VRAM dump holds it;
WiiStation's `scripts/padtest_dx.py` decodes it, and `scripts/padtest_dx.sh` runs a matrix
of controller setups in one boot.

The controller port is driven as the hardware requires (psx-spx, PSn00bSDK's pads example):
/CS low 20 us before the first byte, /ACK awaited for up to 100 us after each byte, the
/ACK flag cleared only after /ACK is high again, and the reply length taken from the /ACKs,
never from the ID byte. **docs/PROTOCOL.md** lists every byte sent each frame, every rule
it relies on and its source.

![padtestscreen](https://raw.githubusercontent.com/ShendoXT/padtest/master/images/screenshot.png)
## Supported controllers:
* Digital (SCPH-1080) controller
* DualShock analog (SCPH-1200) controller
* PlayStation Mouse
* Multitap (SCPH-1070, SCPH-111) on either or both ports, any controllers in its slots

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

# What PadTest DX sends, and why

PadTest DX drives the controller ports itself (no BIOS pad routines, no IRQ handler), so
every byte it sends and every reply it accepts is a decision. This file records each one
and where it comes from. The sources:

- **psx-spx**: "Controller and Memory Card Signals", "Controllers - Communication Sequence",
  "Controller and Memory Card Multitap Adaptor" (psx-spx.consoledev.net, from nocash's
  hardware notes).
- **Mednafen**: `src/psx/input/multitap.cpp`, whose header comment holds notes "from tests on
  the real thing" and a full-read byte dump from a real SCPH-1070 (read from the
  libretro-mirrors/mednafen-git copy).
- **DuckStation**: `src/core/multitap.cpp` and `src/core/pad.cpp` (timing).

Where they disagree, the ROM measures both cases and reports which happened; on a real PS1
that settles it.

## Every transfer

- /CS (DTR) low, then 20 us before the first byte; /CS high and 20 us after the last.
- One byte at a time: wait for TX ready, write, wait up to 100 us for the reply byte
  (psx-spx: 8 bits at the kernel's 250 kHz take 32 us), then wait up to 100 us for /ACK
  (the kernel's timeout, psx-spx "Signals"). Clear the /ACK flag only after /ACK is high
  again (SIO_STAT bit 7 = 0).
- **The reply length is never taken from the ID byte.** psx-spx: the device pulls /ACK
  after every byte but its last. So the ROM keeps clocking while /ACK comes, up to its
  buffer (20 bytes for a single read, 36 for a long read, 10 for a config command), and a
  device that acknowledges one byte too many shows up as a reply one byte too long.
- Times come from root counter 2 at the system clock: `byte_ticks` (write of byte 1 to its
  reply) and `ack[]` (end of each byte to /ACK). Hardware: 32 us and 6.8..13.7 us
  (DuckStation's measurement).

## Each frame, each port

1. **Request**: a single-slot read with the TAP byte (third byte) 01h, `0n 42 01 MM MM`, to
   the first slot that answered last frame (A at first).
2. **Long read**: `01 42 00`, then for each slot `42 00 MM MM 00 00 00 00`, 36 bytes.
3. **Single-slot read** of every other slot, `0n 42 00 MM MM ...` (n = 1..4), or its next
   config command.

On a port with a plain controller, 1 and 2 are two ordinary reads (the pad ignores the TAP
byte) and the reads of 02h..04h go unanswered.

Decisions behind it:

| Rule | Source |
|---|---|
| A single-slot read addresses slot n with first byte 0nh (n = 1..4). An empty slot, or an address other than 01h..04h, gets no /ACK after the address byte. | psx-spx (method 2); Mednafen's notes ("will not respond (no DSR pulse)") |
| A plain controller answers address 01h only. | psx-spx "Signals": devices reply only if the address matches; Mednafen, DuckStation |
| Bit 0 of the third byte asks for a long read on the **next** transfer; it does not change the current one. | psx-spx; Mednafen; DuckStation |
| The request needs a device in the addressed slot (otherwise the transfer ends after byte 0). psx-spx says slot A; Mednafen's notes say the slot the first byte addresses, and once set no slot A device is needed. The ROM sends it to the first slot that answered, so a multitap with slot A empty is still read. | psx-spx; Mednafen |
| A long read is `HiZ 80 5A`, then 8 bytes for each slot A..D: the controller's ID, 5Ah and data, padded with FFh where the controller has less (digital pad: 4 bytes) and all FFh for an empty slot. 35 bytes, the last unacknowledged. | psx-spx; Mednafen's byte dump |
| With no controller in any slot, a long read stops after `HiZ 80 5A`, and nothing can send a request, so the port looks empty. | Mednafen |
| In a long read the four controllers are clocked together during bytes 3..10 with the bytes the console sent for their slot blocks in the **previous** long read (42h 00h.. if that one did not complete). A block that does not start with a command they accept makes the next long read stop after 4 bytes: `HiZ 80 5A` and slot A's ID. | Mednafen (model); psx-spx's "garbage response" is the same 4 bytes |
| Each slot block of the long read starts with 42h, so the next long read is never cut short. | follows from the row above |
| A button may change between two transfers of one frame (a person on hardware; in WiiStation the input script's clock ticks at a scanline inside the ROM's reads), so the long read's buttons are checked against the slot's single read of this frame or the last. | measured in WiiStation |

### The two places the sources disagree

psx-spx's table says a request sent during a long read makes the next transfer "garbage" (4
bytes). Mednafen and DuckStation give a long read again. Mednafen's model explains psx-spx's
table: the garbage comes from what the slot blocks held, not from the request, and a test
that sent 00h in the blocks would see exactly psx-spx's alternation. DuckStation never cuts
a long read short.

The ROM's **probe sequence** (once each time a multitap appears, after the config tests)
tries both cases:

| # | Transfer | Mednafen | psx-spx table | DuckStation |
|---|---|---|---|---|
| 0 | single read, TAP 1 | slot e's reply | same | same |
| 1 | long, TAP 1 | 35 bytes | 35 | 35 |
| 2 | long, TAP 1, 42h blocks | **35** | **4** | 35 |
| 3 | long, TAP 1, 00h blocks | 35 | 35 (after garbage) | 35 |
| 4 | long, TAP 1, 42h blocks | **4** (`FF 80 5A` + slot A's ID) | 4 | **35** |
| 5 | long, TAP 0 | 35 | 35 | 35 |
| 6 | single read, TAP 0 | slot e's reply | same | same |
| 7 | address 00h | no /ACK (1 byte) | | |
| 8 | address 05h | no /ACK (1 byte) | | |
| 9 | single read, TAP 1 | slot e's reply | | |
| 10 | long with command 43h | **4** | | **3** |
| 11 | single read, TAP 0 | slot e's reply | | |

The screen shows `probe A B C ok|differs` under each multitap: the bytes of probes 2, 4 and
10, and whether every probe went as Mednafen's model says. On a real SCPH-1070 that line is
the answer. WiiStation follows Mednafen (`probe 35 4 4 ok`).

## The config test

One command a frame, each slot on its own, after its ID changes (so twice for a DualShock:
digital, then analog after 44h). The commands and the DualShock's (SCPH-1200) replies are
the tables in `controllers.c` (psx-spx "Configuration Commands"; DuckStation, MiSTer and
PsxNewLib agree). A digital pad answers 42h and nothing else. The third byte of every config
command is 00h, so a config command never asks a multitap for a long read.

## The status block

`include/dx.h`, version 4: per port the multitap's state and probe results, per slot (port
device = slot A) the last single read, its /ACK times, the config replies, button presses
and the frame of each button's first press after the config test, stick ranges, and the
slot's block of the last long read. Also the frame's time budget in scanlines (root
counter 1, hblank): reading eight slots at hardware speed takes about 136 of 263 NTSC
lines.

WiiStation's `scripts/padtest_dx.py` decodes it and judges the controller matrix
(`scripts/padtest_dx.sh`).

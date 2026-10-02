#ifndef DX_H
#define DX_H

/*
 * Status block: what the program received from each port and each multitap slot, for a
 * tool to read. Each frame it is copied to VRAM at (DX_VRAM_X, DX_VRAM_Y), DX_VRAM_W x
 * DX_VRAM_H halfwords, rows in order. An emulator VRAM dump holds it; WiiStation's
 * scripts/padtest_dx.py decodes it. All fields are little-endian.
 * Change DX_VERSION when the layout or the meaning of a field changes.
 *
 * Version 4: two ports of four slots each. A port without a multitap uses slot A only: the
 * single-slot reads of slots B..D go unanswered there, as on a PS1. docs/PROTOCOL.md has
 * the transfers made each frame and why.
 */

#include <stdint.h>

#define DX_MAGIC		0x58445450	/*"PTDX"*/
#define DX_VERSION		4
#define DX_VRAM_X		640
#define DX_VRAM_Y		256
#define DX_VRAM_W		64
#define DX_VRAM_H		32

#define DX_SLOTS		4
#define DX_CFG_N		16		/*Config commands, in the order they are sent (see controllers.c)*/
#define DX_PROBE_N		12		/*Multitap probe transfers (controllers.c, Probe)*/
#define DX_LONG_LEN		36		/*Bytes clocked in a long read: 35, and one more that must go unacknowledged*/
#define DX_NO_ACK		0xFFFF
#define DX_NO_FRAME		0xFFFF

/*SlotDX.flags*/
#define DX_SLOT_ANSWERED	(1u << 0)	/*The last single-slot transfer was acknowledged*/
#define DX_SLOT_CONFIG		(1u << 1)	/*The last single-slot transfer was a config command*/

/*One controller: the device on a port, or the one in a slot of a multitap*/
typedef struct
{
	uint8_t  reply[20];			/*Last single-slot read: byte 0 = HiZ, 1 = ID, 2 = 5Ah, then data*/
	uint16_t ack[20];			/*End of each byte to /ACK, system clock ticks (F_CPU); DX_NO_ACK = none*/
	uint8_t  reply_len;			/*Bytes clocked: the device acknowledged all but the last*/
	uint8_t  type;				/*ID byte of the last single-slot read*/
	uint8_t  cfg_state;
	uint8_t  flags;				/*DX_SLOT_**/
	uint16_t buttons;			/*Last single-slot read, 1 = pressed*/
	uint16_t byte_ticks;		/*Last read: write of byte 1 to its reply, system clock ticks (hardware: 32 us)*/
	uint32_t polls;				/*Single-slot reads that got a reply*/
	uint32_t type_changes;
	uint16_t press[16];			/*Presses (0 to 1 changes) of each button bit*/
	uint16_t first[16];			/*Frame of each bit's first press since the config test ended; DX_NO_FRAME = none*/
	uint16_t multi;				/*Reads where more than one bit went to 1 together*/
	uint16_t long_diff;			/*Frames where the long read's buttons were neither this frame's nor the last frame's single-slot read's*/
	uint32_t long_polls;		/*Long reads in which this slot held a device (ID byte not FFh)*/
	uint8_t  long_reply[8];		/*This slot's eight bytes of the last long read*/
	uint8_t  axis_min[4];		/*Raw 0..255: LX, LY, RX, RY. Analog reads only*/
	uint8_t  axis_max[4];
	uint8_t  seen[4][32];		/*Bit map of each raw value an axis gave*/
	uint8_t  cfg[DX_CFG_N][8];	/*Bytes 1..8 of the reply to each config command*/
	uint8_t  cfg_len[DX_CFG_N];	/*Its length in bytes*/
}SlotDX;

/*One controller port, and what the multitap protocol did on it*/
typedef struct
{
	uint8_t  tap;				/*The last long read answered 80h: a multitap*/
	uint8_t  en_slot;			/*Slot the last request (TAP byte 01h) went to, 0..3*/
	uint8_t  long_len;			/*Bytes clocked in the last long read*/
	uint8_t  probes;			/*Probe sequences run (one each time a multitap appears)*/
	uint16_t tap_on;			/*Times a multitap appeared on the port*/
	uint16_t tap_off;			/*Times it went away*/
	uint16_t empty_frames;		/*Frames in which nothing on the port answered*/
	uint16_t plain_frames;		/*Frames in which a device that is not a multitap answered the long read*/
	uint32_t long_reads;		/*Long reads that answered 80h*/
	uint8_t  long_reply[DX_LONG_LEN];	/*The last long read*/
	uint8_t  probe_len[DX_PROBE_N];		/*Each probe transfer: bytes clocked*/
	uint8_t  probe_id[DX_PROBE_N];		/*Its byte 1*/
	uint8_t  probe_last[DX_PROBE_N];	/*Its last byte*/
}PortDX;

typedef struct
{
	uint32_t magic;
	uint16_t version;
	uint16_t size;				/*sizeof(StatusDX)*/
	uint32_t frame;				/*Main loop count*/
	uint32_t vblank;			/*VBlank count*/
	uint16_t lines_read[2];		/*Last frame: scanlines (root counter 1, hblank) reading each port*/
	uint16_t lines_frame;		/*Last frame: scanlines from the end of VSync to the next VSync*/
	uint16_t late;				/*Frames that took more than one vblank*/
	PortDX   port[2];
	SlotDX   slot[2][DX_SLOTS];
}StatusDX;

_Static_assert(sizeof(SlotDX) == 436, "SlotDX layout");
_Static_assert(sizeof(PortDX) == 88, "PortDX layout");
_Static_assert(sizeof(StatusDX) <= DX_VRAM_W * DX_VRAM_H * 2, "StatusDX does not fit");

extern StatusDX Dx;

/*Scanlines (hblanks) on root counter 1, free-running*/
uint16_t Lines(void);

/*Copy the status block to VRAM*/
void UploadDX(void);

#endif

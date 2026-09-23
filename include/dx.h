#ifndef DX_H
#define DX_H

/*
 * Status block: what the program received from each port, for a tool to read.
 * Each frame it is copied to VRAM at (DX_VRAM_X, DX_VRAM_Y), DX_VRAM_W x DX_VRAM_H
 * halfwords, rows in order. An emulator VRAM dump holds it; WiiStation's
 * scripts/padtest_dx.py decodes it. All fields are little-endian.
 * Change DX_VERSION when the layout or the meaning of a field changes.
 */

#include <stdint.h>

#define DX_MAGIC		0x58445450	/*"PTDX"*/
#define DX_VERSION		3
#define DX_VRAM_X		640
#define DX_VRAM_Y		256
#define DX_VRAM_W		64
#define DX_VRAM_H		8

#define DX_CFG_N		16		/*Config commands, in the order they are sent (see controllers.c)*/
#define DX_NO_ACK		0xFFFF

typedef struct
{
	uint8_t  reply[20];			/*Last poll: byte 0 = HiZ, 1 = ID, 2 = 5Ah, then data*/
	uint16_t ack[20];			/*End of each byte to /ACK, system clock ticks (F_CPU); DX_NO_ACK = none or last byte*/
	uint8_t  reply_len;			/*Bytes clocked: the device acknowledged all but the last*/
	uint8_t  type;				/*ID byte of the last poll*/
	uint8_t  cfg_state;
	uint8_t  pad0;
	uint16_t buttons;			/*Last poll, 1 = pressed*/
	uint16_t byte_ticks;		/*Last poll: write of byte 1 to its reply, system clock ticks (hardware: 32 us)*/
	uint32_t polls;				/*Polls that got a reply*/
	uint32_t type_changes;
	uint16_t press[16];			/*Presses (0 to 1 changes) of each button bit*/
	uint16_t multi;				/*Polls where more than one bit went to 1 together*/
	uint16_t pad2;
	uint8_t  axis_min[4];		/*Raw 0..255: LX, LY, RX, RY. Analog polls only*/
	uint8_t  axis_max[4];
	uint8_t  seen[4][32];		/*Bit map of each raw value an axis gave*/
	uint8_t  cfg[DX_CFG_N][8];	/*Bytes 1..8 of the reply to each config command*/
	uint8_t  cfg_len[DX_CFG_N];	/*Its length in bytes*/
}PortDX;

typedef struct
{
	uint32_t magic;
	uint16_t version;
	uint16_t size;				/*sizeof(StatusDX)*/
	uint32_t frame;				/*Main loop count*/
	uint32_t vblank;			/*VBlank count*/
	PortDX   port[2];
}StatusDX;

_Static_assert(sizeof(PortDX) == 392, "PortDX layout");
_Static_assert(sizeof(StatusDX) <= DX_VRAM_W * DX_VRAM_H * 2, "StatusDX does not fit");

extern StatusDX Dx;

/*Copy the status block to VRAM*/
void UploadDX(void);

#endif

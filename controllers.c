#include <stdint.h>
#include <string.h>
#include <psxgpu.h>
#include <hwregs_c.h>
#include "include/controllers.h"
#include "include/dx.h"

/*
 * Controller port protocol, polled (no IRQ handler). Rules from psx-spx
 * "Controller and Memory Card Signals" and PSn00bSDK's examples/io/pads/spi.c:
 * - /CS (DTR) goes low about 20 us before the first byte.
 * - The device pulls /ACK low for about 2 us after each byte but its last.
 *   The BIOS gives up if /ACK has not come 100 us after the byte.
 * - The /ACK flag (SIO_STAT bit 9) can be cleared (SIO_CTRL bit 4) only after
 *   /ACK is high again (SIO_STAT bit 7 = 0).
 * A transfer goes on for as long as the device acknowledges, up to the buffer: the reply
 * length is never taken from the ID byte, so a device that acknowledges one byte too
 * many shows up as a reply one byte too long.
 * Times come from root counter 2, free-running at the system clock.
 *
 * Multitap (SCPH-1070), psx-spx "Controller and Memory Card Multitap Adaptor" and the
 * hardware notes in Mednafen's psx/input/multitap.cpp; docs/PROTOCOL.md has the details:
 * - The first byte addresses a slot: 01h..04h = A..D (single-slot read).
 * - Bit 0 of the third byte (TAP) asks for a long read on the NEXT transfer, and only a
 *   slot that holds a device gets that far. A long read is 80h 5Ah, then eight bytes for
 *   each slot, FFh where a slot is empty or its device has said all it has.
 * - In a long read the bytes sent for a slot go to that slot's device on the NEXT long
 *   read: a slot block that does not start with a command the devices accept (42h) makes
 *   the next long read stop after four bytes (psx-spx's "garbage" response).
 */

/*SIO_STAT bits*/
#define STAT_TX_READY		(1u << 0)
#define STAT_RX_READY		(1u << 1)
#define STAT_ACK_LOW		(1u << 7)
#define STAT_ACK_IRQ		(1u << 9)

/*SIO_CTRL bits*/
#define CTRL_TX_ENABLE		(1u << 0)
#define CTRL_SELECT			(1u << 1)	/*DTR: /CS low on the selected port*/
#define CTRL_ACK_CLEAR		(1u << 4)
#define CTRL_RESET			(1u << 6)
#define CTRL_ACK_IRQ_EN		(1u << 12)
#define CTRL_PORT2			(1u << 13)

#define IRQ_SIO0			7

/*Microseconds to system clock ticks. Good up to about 1900 us (16-bit counter)*/
#define US(n)				((uint16_t)((uint32_t)(n) * (F_CPU / 1000) / 1000))

#define SINGLE_LEN			20		/*Bytes clocked at most in a single-slot read*/
#define CFG_LEN				10		/*In a config command: its 9, and one that must go unacknowledged*/

StatusDX Dx;
Controller Pads[2][DX_SLOTS];

/*A long read is due a probe sequence: the multitap has appeared, and its slots have finished
  the config test (a pad in config mode answers F3h, which would blur the results)*/
static uint8_t ProbeDue[2];

/*
 * Config test, one command a frame after the ID changes (psx-spx "Configuration Commands").
 * It checks what a PS1 DualShock (SCPH-1200) answers, and what it does not: 45h before
 * config mode, and a second 4Dh that must return the first one's map. Byte 0, the address,
 * is set to the slot being tested.
 */
static const uint8_t CfgCmd[DX_CFG_N][9] =
{
	{1, 0x45, 0, 0, 0, 0, 0, 0, 0},				/*Normal mode: not a command, no /ACK*/
	{1, 0x43, 0, 1, 0, 0, 0, 0, 0},				/*Enter config mode*/
	{1, 0x42, 0, 0, 0, 0, 0, 0, 0},				/*Config mode read: F3h, sticks even in digital mode*/
	{1, 0x45, 0, 0, 0, 0, 0, 0, 0},				/*Type and LED*/
	{1, 0x46, 0, 0, 0, 0, 0, 0, 0},				/*Actuator 0*/
	{1, 0x46, 0, 1, 0, 0, 0, 0, 0},				/*Actuator 1*/
	{1, 0x46, 0, 2, 0, 0, 0, 0, 0},				/*No actuator 2: 00h*/
	{1, 0x47, 0, 0, 0, 0, 0, 0, 0},
	{1, 0x48, 0, 0, 0, 0, 0, 0, 0},
	{1, 0x4C, 0, 0, 0, 0, 0, 0, 0},
	{1, 0x4C, 0, 1, 0, 0, 0, 0, 0},
	{1, 0x4F, 0, 0xFF, 0xFF, 0x03, 0, 0, 0},	/*DualShock 2 only: 00h on a DualShock*/
	{1, 0x44, 0, 1, 3, 0, 0, 0, 0},				/*Analog on, locked; resets the rumble map*/
	{1, 0x4D, 0, 0, 1, 255, 255, 255, 255},		/*Map the rumble motors: returns FFh (none)*/
	{1, 0x4D, 0, 0, 1, 255, 255, 255, 255},		/*Again: returns the map just set*/
	{1, 0x43, 0, 0, 0, 0, 0, 0, 0},				/*Exit config mode*/
};

/*
 * Expected bytes 1..8 of each reply from a DualShock (SCPH-1200), and its length; -1 = any.
 * psx-spx, DuckStation, MiSTer and PsxNewLib agree on these.
 */
static const int16_t CfgWant[DX_CFG_N][8] =
{
	{0xFF,   -1,   -1,   -1,   -1,   -1,   -1,   -1},
	{  -1, 0x5A,   -1,   -1,   -1,   -1,   -1,   -1},
	{0xF3, 0x5A,   -1,   -1,   -1,   -1,   -1,   -1},
	{0xF3, 0x5A, 0x01, 0x02,   -1, 0x02, 0x01, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
	{0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xF3, 0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	{0xF3, 0x5A, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};
static const uint8_t CfgWantLen[DX_CFG_N] = {2, 0, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};	/*0 = any*/

uint16_t Lines(void)
{
	return (uint16_t)TIMER_VALUE(1);
}

static uint16_t Ticks(void)
{
	return (uint16_t)TIMER_VALUE(2);
}

static uint16_t TicksSince(uint16_t start)
{
	return (uint16_t)(Ticks() - start);
}

static void Delay(uint16_t ticks)
{
	uint16_t start = Ticks();
	while (TicksSince(start) < ticks);
}

/*Wait until any of the status bits is set (set = 1) or clear (set = 0), for up to limit ticks*/
static int WaitStat(uint16_t bits, int set, uint16_t limit, uint16_t *took)
{
	uint16_t start = Ticks();
	int ok;

	for (;;)
	{
		ok = ((SIO_STAT(0) & bits) != 0) == set;
		if (ok || TicksSince(start) >= limit) break;
	}
	if (took) *took = TicksSince(start);
	return ok;
}

static void ResetPad(Controller* ctrl);

/*Forget the first presses: a new device, or the config test starting again*/
static void ResetFirst(SlotDX *p)
{
	memset(p->first, 0xFF, sizeof(p->first));
}

void InitPad(void)
{
	/*Root counter 2: system clock, free-running, no IRQ. Root counter 1: hblanks, for the
	  frame's time budget*/
	TIMER_CTRL(2) = 0;
	TIMER_CTRL(1) = 0x0100;

	SIO_CTRL(0) = CTRL_RESET;
	SIO_MODE(0) = 0x000D;		/*Baud factor 1, 8 data bits, no parity*/
	SIO_BAUD(0) = 0x0088;		/*F_CPU / 0x88 = 250 kHz*/
	SIO_CTRL(0) = 0;

	Dx.magic = DX_MAGIC;
	Dx.version = DX_VERSION;
	Dx.size = sizeof(StatusDX);
	for (int i = 0; i < 2; i++)
		for (int s = 0; s < DX_SLOTS; s++)
		{
			SlotDX *p = &Dx.slot[i][s];

			memset(p->axis_min, 255, 4);
			memset(p->long_reply, 0xFF, sizeof(p->long_reply));
			ResetFirst(p);
			p->type = PAD_NONE;
			ResetPad(&Pads[i][s]);
		}
}

/*Controller data back to its defaults: nothing connected*/
static void ResetPad(Controller* ctrl)
{
	/*Clear controller data*/
	memset(ctrl, 0, sizeof(Controller));

	/*Treat controller as disconnected*/
	ctrl->Type = PAD_NONE;

	/*Reset cursor to center of the screen*/
	ctrl->CursorX = 160;
	ctrl->CursorY = 120;
}

/*
 * Exchange up to len bytes with the device on port pad_n (0 or 1), for as long as it
 * acknowledges. Returns the bytes exchanged. ack (may be 0) gets the time from the end of
 * each byte to /ACK, DX_NO_ACK where none came; byte_ticks (may be 0) the time byte 1 took.
 * Both in system clock ticks.
 */
static int Transfer(int pad_n, const uint8_t *in, uint8_t *out, int len, uint16_t *ack, uint16_t *byte_ticks)
{
	const uint16_t sel = (uint16_t)(CTRL_TX_ENABLE | CTRL_SELECT | CTRL_ACK_IRQ_EN | (pad_n ? CTRL_PORT2 : 0));
	int n = 0;

	memset(out, 0xFF, (size_t)len);
	if (ack) for (int i = 0; i < len; i++) ack[i] = DX_NO_ACK;

	/*Drop bytes left from an interrupted exchange*/
	while (SIO_STAT(0) & STAT_RX_READY) (void)SIO_DATA(0);

	/*Select the port, clear the /ACK flag, give the device time to wake*/
	SIO_CTRL(0) = sel | CTRL_ACK_CLEAR;
	Delay(US(20));

	for (int x = 0; x < len; x++)
	{
		uint16_t took;

		WaitStat(STAT_TX_READY, 1, US(100), 0);
		SIO_DATA(0) = in[x];

		/*8 bits at 250 kHz: 32 us*/
		if (!WaitStat(STAT_RX_READY, 1, US(100), &took)) break;
		if (x == 1 && byte_ticks) *byte_ticks = took;
		out[x] = (uint8_t)SIO_DATA(0);
		n = x + 1;

		/*/ACK: more to come. None within 100 us: the device has stopped*/
		if (!WaitStat(STAT_ACK_IRQ, 1, US(100), &took)) break;
		if (ack) ack[x] = took;

		WaitStat(STAT_ACK_LOW, 0, US(100), 0);
		SIO_CTRL(0) = sel | CTRL_ACK_CLEAR;
		IRQ_STAT = (uint16_t)~(1u << IRQ_SIO0);
	}

	/*Deselect, and keep /CS high a moment before the next transfer*/
	SIO_CTRL(0) = 0;
	Delay(US(20));
	return n;
}

/*A single-slot read of the buttons: address slot + 1, 42h, the TAP byte, the motors*/
static int ReadSlot(int port, int slot, int tap, uint8_t *rx, uint16_t *ack, uint16_t *byte_ticks)
{
	const Controller *c = &Pads[port][slot];
	const uint8_t tx[SINGLE_LEN] = {(uint8_t)(slot + 1), 0x42, (uint8_t)tap, c->SmallMotor, c->BigMotor};

	return Transfer(port, tx, rx, SINGLE_LEN, ack, byte_ticks);
}

/*
 * A long read. Every slot block of the command bytes is cmd (42h), 00h and the slot's motor
 * bytes; cmd 0 makes them all 00h (the probe's "garbage" case). Returns the bytes clocked.
 */
static int LongRead(int port, int tap, uint8_t cmd, uint8_t *rx)
{
	uint8_t tx[DX_LONG_LEN] = {0x01, 0x42, (uint8_t)tap};

	if (cmd)
		for (int s = 0; s < DX_SLOTS; s++)
		{
			tx[3 + 8 * s] = cmd;
			tx[5 + 8 * s] = Pads[port][s].SmallMotor;
			tx[6 + 8 * s] = Pads[port][s].BigMotor;
		}
	return Transfer(port, tx, rx, DX_LONG_LEN, 0, 0);
}

/*Update the slot's counters from a read of its buttons*/
static void RecordPoll(SlotDX *p, Controller *ctrl, const uint8_t *rx)
{
	uint16_t rose = (uint16_t)(ctrl->Buttons & ~ctrl->PrevButtons);

	p->polls++;
	p->buttons = ctrl->Buttons;
	if (rose & (rose - 1u)) p->multi++;
	for (unsigned b = 0; b < 16; b++)
		if (rose & (1u << b))
		{
			p->press[b]++;
			if (ctrl->ConfigState > DX_CFG_N && p->first[b] == DX_NO_FRAME)
				p->first[b] = (uint16_t)(Dx.frame < DX_NO_FRAME ? Dx.frame : DX_NO_FRAME - 1);
		}
	ctrl->PrevButtons = ctrl->Buttons;

	if (ctrl->Type != PAD_ANALOG) return;

	/*Reply order is RX, RY, LX, LY; the block's is LX, LY, RX, RY*/
	const uint8_t axis[4] = {rx[7], rx[8], rx[5], rx[6]};
	for (int a = 0; a < 4; a++)
	{
		uint8_t v = axis[a];
		if (v < p->axis_min[a]) p->axis_min[a] = v;
		if (v > p->axis_max[a]) p->axis_max[a] = v;
		p->seen[a][v >> 3] |= (uint8_t)(1u << (v & 7));
	}
}

/*A single-slot read of a slot, with the TAP byte given, and what it says about the slot*/
static void SingleRead(int port, int slot, int tap)
{
	uint8_t rx[SINGLE_LEN];
	uint16_t ack[SINGLE_LEN];
	Controller *ctrl = &Pads[port][slot];
	SlotDX *p = &Dx.slot[port][slot];

	p->reply_len = (uint8_t)ReadSlot(port, slot, tap, rx, ack, &p->byte_ticks);
	memcpy(p->reply, rx, sizeof(p->reply));
	memcpy(p->ack, ack, sizeof(p->ack));
	p->flags = p->reply_len > 1 ? DX_SLOT_ANSWERED : 0;

	/*No /ACK after the address byte: nothing there (the data line floats high, FFh)*/
	if (p->reply_len < 2 || rx[1] == PAD_NONE)
	{
		if (ctrl->Type != PAD_NONE) ResetFirst(p);
		ResetPad(ctrl);
		p->type = PAD_NONE;
		return;
	}

	/*A pad in config mode answers a read with F3h. Not a new device: the config test put it there*/
	if (rx[1] == PAD_CONFIG) return;

	/*Check if controller type changed from previous reading*/
	if (ctrl->Type != rx[1])
	{
		ctrl->ConfigState = 0;
		p->type_changes++;
		ResetFirst(p);
	}

	/*Store type*/
	ctrl->Type = rx[1];
	p->type = ctrl->Type;

	/*Get digital buttons*/
	ctrl->Buttons = (uint16_t)~((rx[3] << 8) | rx[4]);

	/*Check if this is analog controller*/
	if (ctrl->Type == PAD_ANALOG)
	{
		/*Get analog sticks: 0..255 with 128 at rest, as -128..127*/
		ctrl->LeftStickX = (int8_t)(rx[7] - 128);
		ctrl->LeftStickY = (int8_t)(rx[8] - 128);
		ctrl->RightStickX = (int8_t)(rx[5] - 128);
		ctrl->RightStickY = (int8_t)(rx[6] - 128);
	}

	/*Check if this is a mouse: bytes 5 and 6 are signed movement*/
	if (ctrl->Type == PAD_MOUSE)
	{
		ctrl->CursorX += (int8_t)rx[5];
		ctrl->CursorY += (int8_t)rx[6];

		/*Clipping*/
		if (ctrl->CursorX < 0) ctrl->CursorX = 0;
		if (ctrl->CursorY < 0) ctrl->CursorY = 0;
		if (ctrl->CursorX > 320) ctrl->CursorX = 320;
		if (ctrl->CursorY > 240) ctrl->CursorY = 240;
	}

	RecordPoll(p, ctrl, rx);
}

/*The slot's next config command, in place of its read*/
static void ConfigStep(int port, int slot)
{
	uint8_t tx[CFG_LEN] = {0}, rx[CFG_LEN];
	Controller *ctrl = &Pads[port][slot];
	SlotDX *p = &Dx.slot[port][slot];
	int c = ctrl->ConfigState - 1;

	memcpy(tx, CfgCmd[c], sizeof(CfgCmd[c]));
	tx[0] = (uint8_t)(slot + 1);
	p->cfg_len[c] = (uint8_t)Transfer(port, tx, rx, CFG_LEN, 0, 0);
	memcpy(p->cfg[c], &rx[1], 8);
	p->flags = (uint8_t)(DX_SLOT_CONFIG | (p->cfg_len[c] > 1 ? DX_SLOT_ANSWERED : 0));

	/*Gone in the middle of the test: start again when something is plugged in*/
	if (p->cfg_len[c] < 2)
	{
		ResetFirst(p);
		ResetPad(ctrl);
		p->type = PAD_NONE;
	}
}

/*Record one probe transfer*/
static void ProbeNote(PortDX *q, int i, const uint8_t *rx, int n)
{
	q->probe_len[i] = (uint8_t)n;
	q->probe_id[i] = rx[1];
	q->probe_last[i] = n ? rx[n - 1] : 0xFF;
}

/*
 * The multitap's request and long-read rules, once each time a multitap appears. What each
 * transfer should give (docs/PROTOCOL.md; scripts/padtest_dx.py judges it):
 *   0  single read, TAP 1      slot e's reply: the request
 *   1  long read, TAP 1        long (35 bytes)
 *   2  long read, TAP 1        long: a request during a long read holds (Mednafen, DuckStation)
 *   3  long, TAP 1, blocks 00h long; the 00h blocks go to the pads on the next long read
 *   4  long read, TAP 1        4 bytes, FFh 80h 5Ah and slot A's ID: the pads refused 00h
 *   5  long read, TAP 0        long again (the request in 4 was taken)
 *   6  single read, TAP 0      slot e's reply (5 asked for none)
 *   7  address 00h             no device: 1 byte
 *   8  address 05h             no device: 1 byte
 *   9  single read, TAP 1      slot e's reply: the request
 *  10  long, command 43h       not 42h: cut short (Mednafen: 4 bytes; DuckStation: 3)
 *  11  single read, TAP 0      slot e's reply
 */
static void Probe(int port, int e)
{
	PortDX *q = &Dx.port[port];
	uint8_t rx[DX_LONG_LEN], tx[DX_LONG_LEN];
	int n;

	n = ReadSlot(port, e, 1, rx, 0, 0);		ProbeNote(q, 0, rx, n);
	n = LongRead(port, 1, 0x42, rx);		ProbeNote(q, 1, rx, n);
	n = LongRead(port, 1, 0x42, rx);		ProbeNote(q, 2, rx, n);
	n = LongRead(port, 1, 0, rx);			ProbeNote(q, 3, rx, n);
	n = LongRead(port, 1, 0x42, rx);		ProbeNote(q, 4, rx, n);
	n = LongRead(port, 0, 0x42, rx);		ProbeNote(q, 5, rx, n);
	n = ReadSlot(port, e, 0, rx, 0, 0);		ProbeNote(q, 6, rx, n);

	memset(tx, 0, sizeof(tx));
	tx[1] = 0x42;
	n = Transfer(port, tx, rx, SINGLE_LEN, 0, 0);	ProbeNote(q, 7, rx, n);
	tx[0] = 0x05;
	n = Transfer(port, tx, rx, SINGLE_LEN, 0, 0);	ProbeNote(q, 8, rx, n);

	n = ReadSlot(port, e, 1, rx, 0, 0);		ProbeNote(q, 9, rx, n);
	tx[0] = 0x01;
	tx[1] = 0x43;
	n = Transfer(port, tx, rx, DX_LONG_LEN, 0, 0);	ProbeNote(q, 10, rx, n);
	n = ReadSlot(port, e, 0, rx, 0, 0);		ProbeNote(q, 11, rx, n);

	q->probes++;
}

/*The long read's result: is there a multitap, and what each slot block holds*/
static void RecordLong(int port, const uint8_t *rx, int n)
{
	PortDX *q = &Dx.port[port];
	int tap = n >= 3 && rx[1] == PAD_MULTITAP;

	q->long_len = (uint8_t)n;
	memcpy(q->long_reply, rx, sizeof(q->long_reply));
	if (tap)
	{
		q->long_reads++;
		for (int s = 0; s < DX_SLOTS; s++)
		{
			SlotDX *p = &Dx.slot[port][s];

			memcpy(p->long_reply, &rx[3 + 8 * s], sizeof(p->long_reply));
			if (p->long_reply[0] != PAD_NONE) p->long_polls++;
		}
	}
	else if (n >= 2)
		q->plain_frames++;

	if (tap && !q->tap)
	{
		q->tap_on++;
		ProbeDue[port] = 1;
	}
	if (!tap && q->tap) q->tap_off++;
	q->tap = (uint8_t)tap;
}

/*
 * Each frame, on each port (docs/PROTOCOL.md):
 *   1. a single-slot read with the TAP byte 01h, to the first slot that answered last
 *      frame: the request for a long read;
 *   2. the long read, TAP byte 00h, a 42h read command in each slot block;
 *   3. a single-slot read of each other slot (01h..04h), or its next config command.
 * On a port without a multitap, 1 and 2 are both reads of the pad, and 3 goes unanswered.
 */
void ReadPort(int port)
{
	PortDX *q = &Dx.port[port];
	const int e = q->en_slot;
	uint8_t rx[DX_LONG_LEN];
	uint16_t before[DX_SLOTS];
	uint8_t had[DX_SLOTS];
	const uint16_t start = Lines();
	int n, any, mid_config = 0;

	for (int s = 0; s < DX_SLOTS; s++)
	{
		mid_config |= Pads[port][s].ConfigState >= 1 && Pads[port][s].ConfigState <= DX_CFG_N;
		before[s] = Dx.slot[port][s].buttons;
		had[s] = Dx.slot[port][s].flags == DX_SLOT_ANSWERED;	/*last frame's was a read*/
	}
	if (ProbeDue[port] && q->tap && !mid_config)
	{
		Probe(port, e);
		ProbeDue[port] = 0;
	}

	/*1. The request. A slot in the middle of its config test is not read: its reply would be F3h*/
	if (Pads[port][e].ConfigState >= 1 && Pads[port][e].ConfigState <= DX_CFG_N)
		(void)ReadSlot(port, e, 1, rx, 0, 0);
	else
		SingleRead(port, e, 1);

	/*2. The long read*/
	n = LongRead(port, 0, 0x42, rx);
	RecordLong(port, rx, n);
	any = n >= 2;

	/*3. The other slots, or each slot's config command*/
	for (int s = 0; s < DX_SLOTS; s++)
	{
		Controller *ctrl = &Pads[port][s];
		SlotDX *p = &Dx.slot[port][s];

		if (ctrl->ConfigState >= 1 && ctrl->ConfigState <= DX_CFG_N)
			ConfigStep(port, s);
		else if (s != e)
			SingleRead(port, s, 0);

		if (ctrl->Type != PAD_NONE && ctrl->ConfigState <= DX_CFG_N) ctrl->ConfigState++;
		p->cfg_state = ctrl->ConfigState;
		if (p->flags & DX_SLOT_ANSWERED) any = 1;

		/*The long read should give the buttons of the slot's single-slot read, this frame's or
		  the last one's: a button may change between two transfers of one frame*/
		if (q->tap && had[s] && p->flags == DX_SLOT_ANSWERED && p->long_reply[0] == p->type)
		{
			const uint16_t lb = (uint16_t)~((p->long_reply[2] << 8) | p->long_reply[3]);
			if (lb != p->buttons && lb != before[s]) p->long_diff++;
		}
	}
	if (!any) q->empty_frames++;

	/*Next frame's request goes to the first slot that answered*/
	q->en_slot = 0;
	for (int s = DX_SLOTS - 1; s >= 0; s--)
		if (Dx.slot[port][s].flags & DX_SLOT_ANSWERED) q->en_slot = (uint8_t)s;
	Dx.lines_read[port] = (uint16_t)(Lines() - start);
}

int CfgMatches(int port, int slot)
{
	const SlotDX *p = &Dx.slot[port][slot];
	int ok = 0;

	for (int c = 0; c < DX_CFG_N; c++)
	{
		int good = !CfgWantLen[c] || p->cfg_len[c] == CfgWantLen[c];
		for (int i = 0; i < 8; i++)
			if (CfgWant[c][i] >= 0 && p->cfg[c][i] != CfgWant[c][i]) good = 0;
		ok += good;
	}
	return ok;
}

void UploadDX(void)
{
	static uint32_t buf[DX_VRAM_W * DX_VRAM_H / 2];
	RECT r = {DX_VRAM_X, DX_VRAM_Y, DX_VRAM_W, DX_VRAM_H};

	Dx.vblank = (uint32_t)VSync(-1);
	memcpy(buf, &Dx, sizeof(Dx));
	LoadImage(&r, buf);
	DrawSync(0);
}

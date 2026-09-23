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
 * Times come from root counter 2, free-running at the system clock.
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

StatusDX Dx;

/*
 * Config commands, sent one a frame after the ID changes (psx-spx "Configuration Commands").
 * 45h, 46h, 47h and 4Ch only read constants; their replies go to the status block.
 */
static const uint8_t CfgCmd[DX_CFG_N][9] =
{
	{1, 0x43, 0, 1, 0, 0, 0, 0, 0},				/*Enter config mode*/
	{1, 0x45, 0, 0, 0, 0, 0, 0, 0},				/*Type and LED*/
	{1, 0x46, 0, 0, 0, 0, 0, 0, 0},				/*Actuator 0*/
	{1, 0x46, 0, 1, 0, 0, 0, 0, 0},				/*Actuator 1*/
	{1, 0x47, 0, 0, 0, 0, 0, 0, 0},
	{1, 0x4C, 0, 0, 0, 0, 0, 0, 0},
	{1, 0x4C, 0, 1, 0, 0, 0, 0, 0},
	{1, 0x44, 0, 1, 3, 0, 0, 0, 0},				/*Analog on, locked*/
	{1, 0x4D, 0, 0, 1, 255, 255, 255, 255},		/*Map the rumble motors*/
	{1, 0x43, 0, 0, 0, 0, 0, 0, 0},				/*Exit config mode*/
};

/*
 * Expected bytes 1..8 of each reply from a DualShock (SCPH-1200); -1 = any.
 * psx-spx, DuckStation and PCSX-Redux agree on these.
 */
static const int16_t CfgWant[DX_CFG_N][8] =
{
	{  -1, 0x5A,   -1,   -1,   -1,   -1,   -1,   -1},
	{0xF3, 0x5A,   -1, 0x02,   -1, 0x02, 0x01, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
	{0xF3, 0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14},
	{0xF3, 0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0xF3, 0x5A,   -1,   -1,   -1,   -1,   -1,   -1},
	{0xF3, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

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

void InitPad(void)
{
	/*Root counter 2: system clock, free-running, no IRQ*/
	TIMER_CTRL(2) = 0;

	SIO_CTRL(0) = CTRL_RESET;
	SIO_MODE(0) = 0x000D;		/*Baud factor 1, 8 data bits, no parity*/
	SIO_BAUD(0) = 0x0088;		/*F_CPU / 0x88 = 250 kHz*/
	SIO_CTRL(0) = 0;

	Dx.magic = DX_MAGIC;
	Dx.version = DX_VERSION;
	Dx.size = sizeof(StatusDX);
	for (int i = 0; i < 2; i++)
	{
		memset(Dx.port[i].axis_min, 255, 4);
		Dx.port[i].type = PAD_NONE;
	}
}

void ResetPad(Controller* ctrl)
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
 * Exchange up to len bytes with the device on port pad_n (0 or 1).
 * Stops at the reply length the ID byte gives, or when the device does not
 * acknowledge a byte. Returns the bytes exchanged. ack (may be 0) gets the
 * time from the end of each byte to /ACK, byte_ticks (may be 0) the time byte 1
 * took, both in system clock ticks.
 */
static int SendData(int pad_n, const uint8_t *in, uint8_t *out, int len, uint16_t *ack, uint16_t *byte_ticks)
{
	const uint16_t sel = (uint16_t)(CTRL_TX_ENABLE | CTRL_SELECT | CTRL_ACK_IRQ_EN | (pad_n ? CTRL_PORT2 : 0));
	int n = 0;

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

		/*The ID byte gives the reply length: 3 bytes + 2 per halfword in its low nibble*/
		if (x == 1)
		{
			int halfwords = (out[1] & 0x0F) ? (out[1] & 0x0F) : 16;
			int want = out[1] == PAD_NONE ? 2 : 3 + 2 * halfwords;
			if (want < len) len = want;
		}
		if (x == len - 1) break;

		/*/ACK: more to come. None within 100 us: the device has stopped*/
		if (!WaitStat(STAT_ACK_IRQ, 1, US(100), &took)) break;
		if (ack) ack[x] = took;

		WaitStat(STAT_ACK_LOW, 0, US(100), 0);
		SIO_CTRL(0) = sel | CTRL_ACK_CLEAR;
		IRQ_STAT = (uint16_t)~(1u << IRQ_SIO0);
	}

	SIO_CTRL(0) = 0;
	return n;
}

/*Update the status block counters from a poll reply*/
static void RecordPoll(PortDX *p, Controller *ctrl, const uint8_t *rx)
{
	uint16_t rose = (uint16_t)(ctrl->Buttons & ~ctrl->PrevButtons);

	p->polls++;
	p->buttons = ctrl->Buttons;
	if (rose & (rose - 1u)) p->multi++;
	for (unsigned b = 0; b < 16; b++)
		if (rose & (1u << b)) p->press[b]++;
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

void ReadPad(Controller* ctrl, int pad_n)
{
	uint8_t DataToSend[] =  {1, 0x42, 0, 0, 0, 0, 0, 0, 0};			/*Standard data polling command*/
	uint8_t ReceivedData[20];
	uint16_t Ack[20];
	PortDX *p = &Dx.port[pad_n];

	memset(ReceivedData, 0, sizeof(ReceivedData));
	for (unsigned i = 0; i < sizeof(Ack) / sizeof(Ack[0]); i++) Ack[i] = DX_NO_ACK;

	if (ctrl->ConfigState >= 1 && ctrl->ConfigState <= DX_CFG_N)
	{
		int c = ctrl->ConfigState - 1;
		SendData(pad_n, CfgCmd[c], ReceivedData, sizeof(CfgCmd[c]), 0, 0);
		memcpy(p->cfg[c], &ReceivedData[1], 8);
	}
	else
	{
		DataToSend[3] = ctrl->SmallMotor;
		DataToSend[4] = ctrl->BigMotor;

		/*Read button status*/
		p->reply_len = (uint8_t)SendData(pad_n, DataToSend, ReceivedData, sizeof(DataToSend), Ack, &p->byte_ticks);
		memcpy(p->reply, ReceivedData, sizeof(p->reply));
		memcpy(p->ack, Ack, sizeof(p->ack));

		/*Check if anything is connected (line not floating high)*/
		if(ReceivedData[1] == PAD_NONE)
		{
			ResetPad(ctrl);
			p->type = PAD_NONE;
		}
		else
		{
			/*Check if controller type changed from previous reading*/
			if(ctrl->Type != ReceivedData[1])
			{
				ctrl->ConfigState = 0;
				p->type_changes++;
			}

			/*Store type*/
			ctrl->Type = ReceivedData[1];
			p->type = ctrl->Type;

			/*Get digital buttons*/
			ctrl->Buttons = (uint16_t)~((ReceivedData[3] << 8) | ReceivedData[4]);

			/*Check if this is analog controller*/
			if(ctrl->Type == PAD_ANALOG)
			{
				/*Get analog sticks: 0..255 with 128 at rest, as -128..127*/
				ctrl->LeftStickX = (int8_t)(ReceivedData[7] - 128);
				ctrl->LeftStickY = (int8_t)(ReceivedData[8] - 128);
				ctrl->RightStickX = (int8_t)(ReceivedData[5] - 128);
				ctrl->RightStickY = (int8_t)(ReceivedData[6] - 128);
			}

			/*Check if this is a mouse: bytes 5 and 6 are signed movement*/
			if(ctrl->Type == PAD_MOUSE){
				ctrl->CursorX += (int8_t)ReceivedData[5];
				ctrl->CursorY += (int8_t)ReceivedData[6];

				/*Clipping*/
				if(ctrl->CursorX < 0) ctrl->CursorX = 0;
				if(ctrl->CursorY < 0) ctrl->CursorY = 0;
				if(ctrl->CursorX > 320) ctrl->CursorX = 320;
				if(ctrl->CursorY > 240) ctrl->CursorY = 240;
			}

			RecordPoll(p, ctrl, ReceivedData);
		}
	}

	if(ctrl->ConfigState <= DX_CFG_N) ctrl->ConfigState++;
	p->cfg_state = ctrl->ConfigState;
}

int CfgMatches(int pad_n)
{
	int ok = 0;

	for (int c = 0; c < DX_CFG_N; c++)
	{
		int good = 1;
		for (int i = 0; i < 8; i++)
			if (CfgWant[c][i] >= 0 && Dx.port[pad_n].cfg[c][i] != CfgWant[c][i]) good = 0;
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

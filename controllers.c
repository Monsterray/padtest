#include <stdint.h>
#include <string.h>
#include <psxgpu.h>
#include <hwregs_c.h>
#include "include/controllers.h"
#include "include/dx.h"

/*
 * Loops to wait for /ACK after a byte. A device sends it some microseconds after each
 * byte but its last; a timeout means it stopped early.
 */
#define ACK_TIMEOUT		20000

StatusDX Dx;

/*
 * Config commands, sent one a frame after the ID changes (psx-spx "Configuration Commands").
 * 45h, 46h, 47h and 4Ch only read constants; their replies go to the status block.
 */
static const unsigned char CfgCmd[DX_CFG_N][9] =
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

/*Expected bytes 1..8 of each reply from a DualShock; -1 = any*/
static const short CfgWant[DX_CFG_N][8] =
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

void InitPad(){
	PADSIO_CTRL(0) = 0x40;
	PADSIO_BAUD(0) = 0x88;
	PADSIO_MODE(0) = 13;
	PADSIO_CTRL(0) = 0;
	BusyLoop(10);
	PADSIO_CTRL(0) = 2;
	BusyLoop(10);
	PADSIO_CTRL(0) = 0x2002;
	BusyLoop(10);
	PADSIO_CTRL(0) = 0;

	Dx.magic = DX_MAGIC;
	Dx.version = DX_VERSION;
	Dx.size = sizeof(StatusDX);
	for (int i = 0; i < 2; i++)
	{
		memset(Dx.port[i].axis_min, 255, 4);
		Dx.port[i].type = PAD_NONE;
	}
}

void BusyLoop(int count){
	volatile int cycles = count;
	while (cycles--);
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

int SendData(int pad_n, const unsigned char *in, unsigned char *out, int len, uint16_t *ack)
{
	int n = 0;

	if (!in || !out)
		return 0;

	/*This is how the BIOS does it*/
	uint16_t mask = pad_n == 0 ? 0x0000 : 0x2000;

	PADSIO_CTRL(0) = mask | 2;
	PADSIO_DATA(0);
	BusyLoop(40);
	PADSIO_CTRL(0) = mask | 0x1003;

	while (!(PADSIO_STATUS(0) & 1));

	for(int x = 0; x < len; x++)
	{
		int w;

		/*Wait for TX ready*/
		while((PADSIO_STATUS(0) & 4) < 1);
		
		PADSIO_DATA(0) = *in;
		in++;

		BusyLoop(25);

		/*Read RX status flag*/
		while((PADSIO_STATUS(0) & 2) < 1);
		
		/*Busy loop only after initial byte*/
		if(x == 0) BusyLoop(40);

		*out = PADSIO_DATA(0);
		out++;
		n = x + 1;

		/*The ID byte gives the reply length: 3 bytes + 2 per halfword in its low nibble*/
		if (x == 1)
		{
			int want = 3 + 2 * ((out[-1] & 15) ? (out[-1] & 15) : 16);
			if (out[-1] == 0xFF) want = 2;
			if (want < len) len = want;
		}
		if (x == len - 1) break;

		/*
		 * /ACK low sets the IRQ flag (status bit 9).
		 * Clear it in SIO (control bit 4) and in I_STAT.
		 */
		for (w = 0; w < ACK_TIMEOUT && !(PADSIO_STATUS(0) & 0x200); w++);
		if (ack) ack[x] = w < ACK_TIMEOUT ? w : DX_NO_ACK;
		if (w == ACK_TIMEOUT) break;
		PADSIO_CTRL(0) = mask | 0x1013;
		IRQ_STAT = ~0x80;
	}
	
	PADSIO_CTRL(0) = 0;
	return n;
}

/*Update the status block counters from a poll reply*/
static void RecordPoll(PortDX *p, Controller *ctrl, const unsigned char *rx)
{
	uint16_t rose = ctrl->Buttons & ~ctrl->PrevButtons;

	p->polls++;
	p->buttons = ctrl->Buttons;
	if (rose & (rose - 1)) p->multi++;
	for (int b = 0; b < 16; b++)
		if (rose & (1 << b)) p->press[b]++;
	ctrl->PrevButtons = ctrl->Buttons;

	if (ctrl->Type != PAD_ANALOG) return;

	/*Reply order is RX, RY, LX, LY; the block's is LX, LY, RX, RY*/
	const unsigned char axis[4] = {rx[7], rx[8], rx[5], rx[6]};
	for (int a = 0; a < 4; a++)
	{
		unsigned char v = axis[a];
		if (v < p->axis_min[a]) p->axis_min[a] = v;
		if (v > p->axis_max[a]) p->axis_max[a] = v;
		p->seen[a][v >> 3] |= 1 << (v & 7);
	}
}

void ReadPad(Controller* ctrl, int pad_n)
{
	unsigned char DataToSend[] =  {1, 0x42, 0, 0, 0, 0, 0, 0, 0};			/*Standard data polling command*/
	unsigned char ReceivedData[20];
	uint16_t Ack[20];
	PortDX *p = &Dx.port[pad_n];

	/*Clear receive buffer*/
	memset(&ReceivedData, 0, sizeof(ReceivedData));
	memset(Ack, 0xFF, sizeof(Ack));

	if (ctrl->ConfigState >= 1 && ctrl->ConfigState <= DX_CFG_N)
	{
		int c = ctrl->ConfigState - 1;
		SendData(pad_n, CfgCmd[c], ReceivedData, sizeof(CfgCmd[c]), 0);
		memcpy(p->cfg[c], &ReceivedData[1], 8);
	}
	else
	{
		DataToSend[3] = ctrl->SmallMotor;
		DataToSend[4] = ctrl->BigMotor;

		/*Read button status*/
		p->reply_len = SendData(pad_n, DataToSend, ReceivedData, sizeof(DataToSend), Ack);
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
			ctrl->Buttons = ~((ReceivedData[3] << 8) | ReceivedData[4]);

			/*Check if this is analog controller*/
			if(ctrl->Type == PAD_ANALOG)
			{
				/*Get analog sticks*/
				ctrl->LeftStickX = ReceivedData[7] - 128;
				ctrl->LeftStickY = ReceivedData[8] - 128;
				ctrl->RightStickX = ReceivedData[5] - 128;
				ctrl->RightStickY = ReceivedData[6] - 128;
			}

			/*Check if this is a mouse*/
			if(ctrl->Type == PAD_MOUSE){
				ctrl->CursorX += (char)ReceivedData[5];
				ctrl->CursorY += (char)ReceivedData[6];

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

	Dx.vblank = VSync(-1);
	memcpy(buf, &Dx, sizeof(Dx));
	LoadImage(&r, buf);
	DrawSync(0);
}

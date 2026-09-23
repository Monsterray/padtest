#include <stdio.h>
#include "include/gs.h"
#include "include/graphics.h"
#include "include/text.h"
#include "include/dx.h"

#include "images/buttons.h"
#include "images/mouse.h"

void InitGraphics(void)
{
	/*Video mode from the console's region*/
	GsInit();

	/*Load font to VRAM*/
	InitText();
	
	/*Load controller buttons and mouse images*/
	GsLoadTim(Buttons_tim);
	GsLoadTim(Mouse_tim);
}

/*Draw green plus (used for analog sticks)*/
static void DrawPlus(int x, int y)
{
	GsLine PlusLine;
	
	PlusLine.x[0] = x - 2;
	PlusLine.x[1] = x + 2;
	PlusLine.y[0] = y;
	PlusLine.y[1] = y;
	PlusLine.r = 109;
	PlusLine.g = 193;
	PlusLine.b = 99;
	PlusLine.attribute = 0;
	
	GsSortLine(&PlusLine);
	
	PlusLine.x[0] = x;
	PlusLine.x[1] = x;
	PlusLine.y[0] = y - 2;
	PlusLine.y[1] = y + 2;
	
	GsSortLine(&PlusLine);
}

void DrawTitle(const char* softwareTitle, const char* copyright)
{
	int FontX = 0;
	GsRectangle TopRect;

	/*Draw top rectangle*/
	TopRect.x = 0;
	TopRect.y = 0;
	TopRect.w = 320;
	TopRect.h = 40;
	TopRect.r = 0;
	TopRect.g = 76;
	TopRect.b = 163;
	TopRect.attribute = 0;
	
	GsSortRectangle(&TopRect);
	
	GsPrintString(16, 16, 128, 128, 128, false, softwareTitle);
	
	FontX = GetPrintedStringWidth(false, "PORT 1");
	GsPrintString(80 - (FontX/2), 46, 128, 128, 128, false, "PORT 1");
	
	FontX = GetPrintedStringWidth(false, "PORT 2");
	GsPrintString(240 - (FontX/2), 46, 128, 128, 128, false, "PORT 2");
	
	/*Credits go top right: the bottom lines hold the diagnostics*/
	GsPrintString(304 - GetPrintedStringWidth(false, copyright), 16, 128, 128, 128, false, copyright);
}

/*Draw the mouse, its buttons and its cursor*/
static void DrawMouse(int x, int y, const Controller* ctrl)
{
	GsSprite MouseSprite;

	MouseSprite.x = x + 26;
	MouseSprite.y = y + 10;
	MouseSprite.w = 86;
	MouseSprite.h = 128;
	MouseSprite.u = 64;
	MouseSprite.v = 0;
	MouseSprite.r = MouseSprite.g = MouseSprite.b = 128;
	MouseSprite.cx = 320;
	MouseSprite.cy = 242;
	MouseSprite.tpage = 7;
	MouseSprite.attribute = COLORMODE(COLORMODE_8BPP);

	GsSortSimpleSprite(&MouseSprite);

	MouseSprite.v = 128;
	MouseSprite.h = 43;

	/*Left mouse button*/
	if(ctrl->Buttons & MOUSE_LB){
		MouseSprite.w = 45;
		GsSortSimpleSprite(&MouseSprite);
	}

	/*Right mouse button*/
	if(ctrl->Buttons & MOUSE_RB){
		MouseSprite.x = x + 26 + 45;
		MouseSprite.u = 64 + 45;
		MouseSprite.w = 41;
		GsSortSimpleSprite(&MouseSprite);
	}

	/*Draw cursor*/
	DrawPlus(ctrl->CursorX, ctrl->CursorY);
}

/*Draw a button sprite; the pressed image is 16 pixels to the right of the released one*/
static void DrawButton(const GsSprite *s, unsigned pressed)
{
	GsSprite b = *s;

	if (pressed) b.u += 16;
	GsSortSimpleSprite(&b);
}

/*Draw controller at the specified coordinates*/
void DrawController(int x, int y, Controller* ctrl)
{
    GsSprite PadSprite;

	int FontX = 0;
	int AnalogEnabled = 0;
	unsigned buttons = ctrl->Buttons;
	int StickX[2] = {0, 0};
	int StickY[2] = {0, 0};
	char TempString[50];
	
	/*Check what kind of controller is connected to the port*/
	switch(ctrl->Type)
	{
		default:
			FontX = GetPrintedStringWidth(false, "Not supported");
			GsPrintString(x + 70 - (FontX/2), 56, 128, 128, 128, false, "Not supported");
			return;
			
		case PAD_NONE:
			FontX = GetPrintedStringWidth(false, "Not connected");
			GsPrintString(x + 70 - (FontX/2), 56, 128, 128, 128, false, "Not connected");
			return;
			
        case PAD_MOUSE:
			FontX = GetPrintedStringWidth(false, "Mouse");
			GsPrintString(x + 70 - (FontX/2), 56, 128, 128, 128, false, "Mouse");
			DrawMouse(x, y, ctrl);
            return;

		case PAD_DIGITAL:
			FontX = GetPrintedStringWidth(false, "Digital");
			GsPrintString(x + 70 - (FontX/2), 56, 128, 128, 128, false, "Digital");
			break;
			
		case PAD_ANALOG:
			AnalogEnabled = 1;
			FontX = GetPrintedStringWidth(false, "Analog");
			GsPrintString(x + 70 - (FontX/2), 56, 128, 128, 128, false, "Analog");
			StickX[0] = ctrl->LeftStickX;
			StickY[0] = ctrl->LeftStickY;
			StickX[1] = ctrl->RightStickX;
			StickY[1] = ctrl->RightStickY;
			break;
	}
	
	PadSprite.x = x + 10;
	PadSprite.y = y;
	PadSprite.w = 16;
	PadSprite.h = 16;
	PadSprite.u = 32;
	PadSprite.v = 16;
	PadSprite.r = PadSprite.g = PadSprite.b = 128;
	PadSprite.cx = 320;
	PadSprite.cy = 241;
	PadSprite.tpage = 7;
	PadSprite.attribute = COLORMODE(COLORMODE_8BPP);
	
	/*L1*/
	DrawButton(&PadSprite, buttons & PAD_L1);
	
	
	/*L2*/
	PadSprite.v -= 16;
	PadSprite.y += 16;
	
	DrawButton(&PadSprite, buttons & PAD_L2);
	
	
	/*UP*/
	PadSprite.u -= 32;
	PadSprite.y += 32;
	
	DrawButton(&PadSprite, buttons & PAD_UP);
	
	
	/*LEFT*/
	PadSprite.v += 32;
	PadSprite.x -= 10;
	PadSprite.y += 10;
	
	DrawButton(&PadSprite, buttons & PAD_LEFT);
	
	/*DOWN*/
	PadSprite.v -= 16;
	PadSprite.x += 10;
	PadSprite.y += 10;
	
	DrawButton(&PadSprite, buttons & PAD_DOWN);
	
	
	/*RIGHT*/
	PadSprite.v += 32;
	PadSprite.x +=10;
	PadSprite.y -= 10;
	
	DrawButton(&PadSprite, buttons & PAD_RIGHT);
	
	
	/*SELECT*/
	PadSprite.u += 32;
	PadSprite.v -= 16;
	PadSprite.x += 26;
	
	DrawButton(&PadSprite, buttons & PAD_SELECT);
	
	
	/*START*/
	PadSprite.v += 16;
	PadSprite.x += 26;
	
	DrawButton(&PadSprite, buttons & PAD_START);
	
	
	/*SQUARE*/
	PadSprite.u -= 32;	
	PadSprite.v += 64;
	PadSprite.x += 26;
	
	DrawButton(&PadSprite, buttons & PAD_SQUARE);
	
	/*CROSS*/
	PadSprite.v -= 32;
	PadSprite.x += 13;
	PadSprite.y += 13;	
	
	DrawButton(&PadSprite, buttons & PAD_CROSS);
	
	
	/*CIRCLE*/
	PadSprite.v -= 16;
	PadSprite.x += 13;
	PadSprite.y -= 13;	
	
	DrawButton(&PadSprite, buttons & PAD_CIRCLE);
	
	
	/*TRIANGLE*/
	PadSprite.v += 32;
	PadSprite.x -= 13;
	PadSprite.y -= 13;	
	
	DrawButton(&PadSprite, buttons & PAD_TRIANGLE);
	
	
	/*R2*/
	PadSprite.u += 32;
	PadSprite.v -= 96;
	PadSprite.y -= 29;
	
	DrawButton(&PadSprite, buttons & PAD_R2);
	
	
	/*R1*/
	PadSprite.v += 16;
	PadSprite.y -= 16;
	
	DrawButton(&PadSprite, buttons & PAD_R1);
	
	/*Return if this is not analog controller*/
	if(AnalogEnabled == 0) return;
	
	PadSprite.x = x + 26;
	PadSprite.y = y + 80;
	PadSprite.w = 32;
	PadSprite.h = 32;
	PadSprite.u = 32;
	PadSprite.v = 64;
	PadSprite.r = PadSprite.g = PadSprite.b = 128;
	PadSprite.cx = 320;
	PadSprite.cy = 241;
	PadSprite.tpage = 7;
	PadSprite.attribute = COLORMODE(COLORMODE_8BPP);
	
	/*Left analog stick*/
	if(buttons & PAD_LANALOGB)
	{
		PadSprite.v += 32;
		GsSortSimpleSprite(&PadSprite);
		PadSprite.v -= 32;
		
		/*Rumble big motor*/
		ctrl->BigMotor = 255;
	}
	else 
	{
		GsSortSimpleSprite(&PadSprite);
		ctrl->BigMotor = 0;
	}
	
	/*Left stick position*/
	DrawPlus(x + 42 + (StickX[0]/8), y + 96 + (StickY[0]/8));
	sprintf(TempString, "X: %d\nY: %d", StickX[0], StickY[0]);
	GsPrintString(x + 26, y + 116, 128, 128, 128, false, TempString);
	
	/*Right analog stick*/
	PadSprite.x += 52;
	if(buttons & PAD_RANALOGB)
	{
		PadSprite.v += 32;
		GsSortSimpleSprite(&PadSprite);
		PadSprite.v -= 32;
		
		/*Rumble small motor*/
		ctrl->SmallMotor = 255;
	}
	else
	{
		GsSortSimpleSprite(&PadSprite);
		ctrl->SmallMotor = 0;
	}
	
	DrawPlus(x + 94 + (StickX[1]/8), y + 96 + (StickY[1]/8));
	sprintf(TempString, "X: %d\nY: %d", StickX[1], StickY[1]);
	GsPrintString(x + 78, y + 116, 128, 128, 128, false, TempString);
}

/*Draw what the port sent: raw reply, reply length, config replies, sweep counters*/
void DrawDX(int x, int PadId)
{
	PortDX *p = &Dx.port[PadId];
	char s[64] = "";
	int i, n = 0, bits = 0;

	if (p->type == PAD_NONE) return;

	/*ID, 5Ah, then the data bytes, in pairs to fit the column*/
	for (i = 1; i < p->reply_len && i < 9; i++) n += sprintf(s + n, (i & 1) ? "%02X" : "%02X ", p->reply[i]);
	GsPrintString(x, 203, 128, 128, 128, false, s);

	for (i = 0; i < 16; i++) if (p->press[i]) bits++;
	sprintf(s, "len%d cfg%d/%d btn%d", p->reply_len, CfgMatches(PadId), DX_CFG_N, bits);
	GsPrintString(x, 213, 128, 128, 128, false, s);

	if (p->type != PAD_ANALOG) return;

	/*Distinct values each axis gave: LX LY RX RY*/
	n = sprintf(s, "axes");
	for (int a = 0; a < 4; a++)
	{
		int c = 0;
		for (i = 0; i < 32; i++) c += __builtin_popcount(p->seen[a][i]);
		n += sprintf(s + n, " %d", c);
	}
	GsPrintString(x, 223, 128, 128, 128, false, s);
}

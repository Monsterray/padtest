#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include <stdint.h>

/*Controller IDs: the low byte of halfword 0 of a reply (psx-spx "Controller ID")*/
#define PAD_NONE			0xFF	/*Nothing connected: the data line floats high*/
#define PAD_MOUSE			0x12
#define PAD_NEGCON			0x23
#define PAD_KONAMI_GUN		0x31	/*Konami Justifier/Hyperblaster (IRQ10)*/
#define PAD_DIGITAL			0x41	/*Digital pad, or an analog pad in digital mode*/
#define PAD_FLIGHT			0x53	/*Analog stick, or an analog pad in green LED mode*/
#define PAD_GUNCON			0x63	/*Namco GunCon*/
#define PAD_ANALOG			0x73	/*Analog pad in red LED mode*/
#define PAD_MULTITAP		0x80	/*Multitap, in a long read (all four slots)*/
#define PAD_CONFIG			0xF3	/*Any pad in config mode*/

/*Buttons, in Controller.Buttons = ~((reply[3] << 8) | reply[4]), 1 = pressed*/
#define PAD_L2				(1u << 0)
#define PAD_R2				(1u << 1)
#define PAD_L1				(1u << 2)
#define PAD_R1				(1u << 3)
#define PAD_TRIANGLE		(1u << 4)
#define PAD_CIRCLE			(1u << 5)
#define PAD_CROSS			(1u << 6)
#define PAD_SQUARE			(1u << 7)
#define PAD_SELECT			(1u << 8)
#define PAD_LANALOGB		(1u << 9)
#define PAD_RANALOGB		(1u << 10)
#define PAD_START			(1u << 11)
#define PAD_UP				(1u << 12)
#define PAD_RIGHT			(1u << 13)
#define PAD_DOWN			(1u << 14)
#define PAD_LEFT			(1u << 15)

/*Mouse buttons, in Controller.Buttons*/
#define MOUSE_RB			(1u << 2)
#define MOUSE_LB			(1u << 3)

/*All properties of a controller*/
typedef struct
{	uint8_t Type;
	uint8_t ConfigState;
	uint8_t SmallMotor;
	uint8_t BigMotor;
	uint16_t Buttons;
	uint16_t PrevButtons;
	int8_t LeftStickX;
	int8_t LeftStickY;
	int8_t RightStickX;
	int8_t RightStickY;
	int CursorX;
	int CursorY;
}Controller;

/*Set up the controller port (SIO0) and the timer used to time it*/
void InitPad(void);

/*Config replies of a port's slot that match a DualShock's*/
int CfgMatches(int port, int slot);

/*Read everything on one port (0 or 1): its device, or a multitap and its four slots*/
void ReadPort(int port);

/*What each port and slot holds; slot 0 is the port's own device when there is no multitap*/
extern Controller Pads[2][4];

#endif

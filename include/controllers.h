#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include <stdint.h>

/*Copied from pad.c. volatile: the compiler must not merge or hoist hardware accesses*/
#define PADSIO_DATA(x)      *((volatile unsigned char*)(0x1f801040 + (x<<4)))
#define PADSIO_STATUS(x)    *((volatile unsigned short*)(0x1f801044 + (x<<4)))
#define PADSIO_MODE(x)      *((volatile unsigned short*)(0x1f801048 + (x<<4)))
#define PADSIO_CTRL(x)      *((volatile unsigned short*)(0x1f80104a + (x<<4)))
#define PADSIO_BAUD(x)      *((volatile unsigned short*)(0x1f80104e + (x<<4)))

/*Types of controllers*/
#define PAD_NONE			0xFF
#define PAD_DIGITAL         0x41
#define PAD_ANALOG          0x73
#define PAD_FLIGHT			0x53
#define PAD_MOUSE			0x12

/*Buttons, in Controller.Buttons = ~((reply[3] << 8) | reply[4]) (PSXSDK's values)*/
#define PAD_L2				(1 << 0)
#define PAD_R2				(1 << 1)
#define PAD_L1				(1 << 2)
#define PAD_R1				(1 << 3)
#define PAD_TRIANGLE		(1 << 4)
#define PAD_CIRCLE			(1 << 5)
#define PAD_CROSS			(1 << 6)
#define PAD_SQUARE			(1 << 7)
#define PAD_SELECT			(1 << 8)
#define PAD_LANALOGB		(1 << 9)
#define PAD_RANALOGB		(1 << 10)
#define PAD_START			(1 << 11)
#define PAD_UP				(1 << 12)
#define PAD_RIGHT			(1 << 13)
#define PAD_DOWN			(1 << 14)
#define PAD_LEFT			(1 << 15)
#define MOUSE_RB			0x4
#define MOUSE_LB			0x8

/*All properties of a controller*/
typedef struct
{	unsigned char Type;
	unsigned char ConfigState;
	unsigned char SmallMotor;
	unsigned char BigMotor;
	unsigned short Buttons;
	unsigned short PrevButtons;
	char LeftStickX;
	char LeftStickY;
	char RightStickX;
	char RightStickY;
	int CursorX;
	int CursorY;
}Controller;

/*Setup SIO port for controllers*/
void InitPad();

/*Send data to PAD_SIO. Returns the bytes sent: the device acknowledged all but the last.
  ack (may be 0) gets the loops waited for /ACK after each byte*/
int SendData(int pad_n, const unsigned char *in, unsigned char *out, int len, uint16_t *ack);

/*Config replies of a port that match a DualShock's*/
int CfgMatches(int pad_n);

/*Reset controller data to default values*/
void ResetPad(Controller* ctrl);

/*Read controller data from a single port*/
void ReadPad(Controller* ctrl, int pad_n);

/*
* Critical timing loop for the controllers
* BIOS actually does it this way
* Thanks to OpenBIOS for the info
*/
void BusyLoop(int count);

#endif
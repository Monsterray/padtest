#ifndef GS_H
#define GS_H

/*
 * The PSXSDK drawing calls that PadTest uses, on PSn00bSDK.
 * Primitives are drawn in the order they are sorted.
 */

#include <stdint.h>
#include <stdbool.h>
#include <psxgpu.h>

#define COLORMODE(x)		(x)
#define COLORMODE_8BPP		1

/*tpage is the PSXSDK texture page number: x = (tpage & 15) * 64, y = (tpage >> 4) * 256*/
typedef struct
{	int x, y, w, h;
	int u, v;
	unsigned char r, g, b;
	int cx, cy;
	int tpage;
	int attribute;
}GsSprite;

typedef struct
{	int x, y, w, h;
	unsigned char r, g, b;
	int attribute;
}GsRectangle;

typedef struct
{	int x[2], y[2];
	unsigned char r, g, b;
	int attribute;
}GsLine;

/*Set up the GPU: two 320x240 buffers at y = 0 and y = 256*/
void GsInit(int pal);

/*Upload a TIM image (and its CLUT) to VRAM. The data must be 4-byte aligned*/
void GsLoadTim(const void *tim);

/*Show the buffer drawn last, clear the other one and start a new list*/
void GsFlip(void);

void GsSortSimpleSprite(GsSprite *s);
void GsSortRectangle(GsRectangle *r);
void GsSortLine(GsLine *l);

/*Draw the list and wait for the GPU*/
void GsDrawList(void);

#endif

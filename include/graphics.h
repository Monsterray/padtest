#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "controllers.h"

/*Set everything up*/
void InitGraphics(void);

/*Draw title bar*/
void DrawTitle(const char* softwareTitle, const char* copyright);

/*Draw controller on screen with all the properties*/
void DrawController(int x, int y, Controller* ctrl);

/*Draw the diagnostic lines for a port at the bottom of the screen*/
void DrawDX(int x, int PadId);

#endif
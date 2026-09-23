#ifndef TEXT_H
#define TEXT_H

#include <stdbool.h>
#include <stdint.h>

/*Upload the font to VRAM*/
void InitText(void);

/*Width in pixels of the first line of a string*/
int GetPrintedStringWidth(bool monospace, const char *string);

/*
 * Print a string at the specified coordinates in color (128 = full brightness).
 * x < 0 centres each line. Supports \n - newline. If monospace is true each
 * character is spaced 8px from the previous one.
 */
void GsPrintString(int x, int y, uint8_t Red, uint8_t Green, uint8_t Blue, bool monospace, const char *string);

#endif

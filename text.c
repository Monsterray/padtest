#include "include/gs.h"
#include "include/text.h"
#include "include/fontspace.h"
#include "include/font.h"

/*The font holds characters 20h..7Fh*/
#define FIRST_CHAR	0x20
#define LAST_CHAR	0x7F

void InitText(void)
{
	/*Load a custom font and upload it to VRAM*/
	GsLoadTim(FontTimData);
}

/*Advance of a character, or 0 if the font does not hold it*/
static int CharWidth(unsigned char c, bool monospace)
{
	if (c < FIRST_CHAR || c > LAST_CHAR) return 0;
	return monospace ? 8 : FontSpace[c - FIRST_CHAR] + 1;
}

int GetPrintedStringWidth(bool monospace, const char *string)
{
	int StringWidth = 0;

	for (; *string && *string != '\n'; string++)
		StringWidth += CharWidth((unsigned char)*string, monospace);
	
	return StringWidth;
}

void GsPrintString(int x, int y, uint8_t Red, uint8_t Green, uint8_t Blue, bool monospace, const char *string)
{
	GsSprite CharSprite;
	
	/*Set up character sprite*/
	if(x < 0)CharSprite.x = 160 - (GetPrintedStringWidth(monospace, string)/2);
	else CharSprite.x = x;
	
	CharSprite.y = y;
	CharSprite.w = 8;
	CharSprite.h = 8;
	CharSprite.r = Red;
	CharSprite.g = Green;
	CharSprite.b = Blue;
	CharSprite.cx = 320;
	CharSprite.cy = 240;
	CharSprite.tpage = 5;
	CharSprite.attribute = COLORMODE(COLORMODE_8BPP);

	for (; *string; string++)
	{
		unsigned char c = (unsigned char)*string;

		/*Check if this is a newline character*/
		if (c == '\n')
		{
			if(x < 0)CharSprite.x = 160 - (GetPrintedStringWidth(monospace, string + 1)/2);
			else CharSprite.x = x;
			
			CharSprite.y += 10;
			continue;
		}

		/*Skip characters the font does not hold*/
		if (!CharWidth(c, monospace)) continue;

		/*The font is 32 characters a row, 8x8 each*/
		CharSprite.u = ((c - FIRST_CHAR) % 32) * 8;
		CharSprite.v = ((c - FIRST_CHAR) / 32) * 8;

		/*Place sprite in the drawing list*/
		GsSortSimpleSprite(&CharSprite);
		
		/*Increase X offset*/
		CharSprite.x += CharWidth(c, monospace);
	}
}

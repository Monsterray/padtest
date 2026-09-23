#include <stdio.h>
#include <string.h>

#include "include/gs.h"
#include "include/text.h"
#include "include/controllers.h"
#include "include/graphics.h"
#include "include/dx.h"

#define SOFTWARE_TITLE		"PadTest 1.2 DX\n2026-09-23"
#define SOFTWARE_COPYRIGHT	"Shendo, ggrtk\nPSn00bSDK"

/*Controller for each port*/
Controller Controllers[2];

int main()
{
	InitGraphics();
	InitPad();

	/*Set default values for both controllers*/
	ResetPad(&Controllers[0]);
	ResetPad(&Controllers[1]);

	/*Main loop of the application*/
	while(1)
	{
		/*Flip main and back buffer, clear the new back buffer*/
		GsFlip();

		DrawTitle(SOFTWARE_TITLE, SOFTWARE_COPYRIGHT);
		ReadPad(&Controllers[0], 0);
		ReadPad(&Controllers[1], 1);

		/*Draw controllers on the screen*/
		DrawController(10, 65, 0, &Controllers[0]);
		DrawController(170, 65, 1, &Controllers[1]);
		DrawDX(10, 0);
		DrawDX(170, 1);

		/*Draw primitives from the list and wait for the GPU*/
		GsDrawList();

		Dx.frame++;
		UploadDX();

		/*Wait for vertical sync*/
		VSync(0);
	}

	return 0;
}

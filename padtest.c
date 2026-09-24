#include <stdio.h>
#include <string.h>

#include "include/gs.h"
#include "include/text.h"
#include "include/controllers.h"
#include "include/graphics.h"
#include "include/dx.h"

/*PADTEST_VERSION and PADTEST_DATE come from CMakeLists.txt: the project version and the
  date of the commit built*/
#define SOFTWARE_TITLE		"PadTest DX " PADTEST_VERSION "\n" PADTEST_DATE
#define SOFTWARE_COPYRIGHT	"Authors: Monsterray\nPorted from PadTest by Shendo, ggrtk"

/*Controller for each port*/
static Controller Controllers[2];

int main(void)
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
		DrawController(10, 53, &Controllers[0]);
		DrawController(170, 53, &Controllers[1]);
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

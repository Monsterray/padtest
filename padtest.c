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

int main(void)
{
	InitGraphics();
	InitPad();

	/*Main loop of the application*/
	int vb = VSync(-1);
	while(1)
	{
		const uint16_t t0 = Lines();

		/*Flip main and back buffer, clear the new back buffer*/
		GsFlip();

		DrawTitle(SOFTWARE_TITLE, SOFTWARE_COPYRIGHT);
		ReadPort(0);
		ReadPort(1);

		/*Draw each port: its controller, or a multitap's four slots*/
		for (int port = 0; port < 2; port++)
		{
			if (!Dx.port[port].tap) DrawController(10 + 160 * port, 53, &Pads[port][0]);
			DrawDX(10 + 160 * port, port);
		}

		/*Draw primitives from the list and wait for the GPU*/
		GsDrawList();

		Dx.frame++;
		Dx.lines_frame = (uint16_t)(Lines() - t0);
		UploadDX();

		/*Wait for vertical sync*/
		VSync(0);
		if (VSync(-1) - vb > 1) Dx.late++;
		vb = VSync(-1);
	}

	return 0;
}

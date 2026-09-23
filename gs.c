#include "include/gs.h"

#define PRIM_WORDS	0x8000

static DISPENV Disp[2];
static DRAWENV Draw[2];
static int Db;

static uint32_t PrimBuf[PRIM_WORDS];
static uint32_t *Next;
static void *First, *Last;
static int CurTPage;

/*Get space for a primitive and link it after the last one. Returns 0 if the buffer is full*/
static void *NewPrim(int words)
{
	void *p = Next;

	if (Next + words > PrimBuf + PRIM_WORDS) return 0;
	Next += words;

	if (Last) catPrim(Last, p);
	else First = p;
	Last = p;
	return p;
}

void GsInit(int pal)
{
	ResetGraph(0);
	SetVideoMode(pal ? MODE_PAL : MODE_NTSC);

	for (int i = 0; i < 2; i++)
	{
		SetDefDispEnv(&Disp[i], 0, i ? 0 : 256, 320, 240);
		SetDefDrawEnv(&Draw[i], 0, i ? 256 : 0, 320, 240);
		if (pal) Disp[i].screen.y = 24;
		setRGB0(&Draw[i], 0, 0, 0);
		Draw[i].isbg = 1;
	}

	GsFlip();
	SetDispMask(1);
}

void GsLoadTim(const void *tim)
{
	TIM_IMAGE t;

	GetTimInfo((const uint32_t *)tim, &t);
	LoadImage(t.prect, t.paddr);
	DrawSync(0);

	if (t.mode & 8)
	{
		LoadImage(t.crect, t.caddr);
		DrawSync(0);
	}
}

void GsFlip(void)
{
	Db = !Db;
	PutDispEnv(&Disp[Db]);
	PutDrawEnv(&Draw[Db]);

	Next = PrimBuf;
	First = Last = 0;
	CurTPage = -1;
}

void GsSortSimpleSprite(GsSprite *s)
{
	int tp = getTPage(s->attribute & 3, 0, (s->tpage & 15) * 64, (s->tpage >> 4) * 256);
	SPRT *p;

	if (tp != CurTPage)
	{
		DR_TPAGE *t = NewPrim(sizeof(DR_TPAGE) / 4);
		if (!t) return;
		setDrawTPage(t, 0, 0, tp);
		CurTPage = tp;
	}

	p = NewPrim(sizeof(SPRT) / 4);
	if (!p) return;
	setSprt(p);
	setXY0(p, s->x, s->y);
	setWH(p, s->w, s->h);
	setUV0(p, s->u, s->v);
	setRGB0(p, s->r, s->g, s->b);
	setClut(p, s->cx, s->cy);
}

void GsSortRectangle(GsRectangle *r)
{
	TILE *p = NewPrim(sizeof(TILE) / 4);

	if (!p) return;
	setTile(p);
	setXY0(p, r->x, r->y);
	setWH(p, r->w, r->h);
	setRGB0(p, r->r, r->g, r->b);
}

void GsSortLine(GsLine *l)
{
	LINE_F2 *p = NewPrim(sizeof(LINE_F2) / 4);

	if (!p) return;
	setLineF2(p);
	setXY2(p, l->x[0], l->y[0], l->x[1], l->y[1]);
	setRGB0(p, l->r, l->g, l->b);
}

void GsDrawList(void)
{
	if (!First) return;
	termPrim(Last);
	DrawOTag((const uint32_t *)First);
	DrawSync(0);
}

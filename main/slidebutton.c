
#include "stdio.h"
#include "slidebutton.h"

//static s_slide slide;

void redraw(s_slide* slide);
void static_draw(s_slide* slide);

void init_slide(s_slide* slide, int ax, int ay, int aw, int ah, int ap)
{
	slide->x = ax;
	slide->y = ay;
	slide->width = aw;
	slide->height = ah;
	slide->part = ap;
	slide->sldvalue = 0;
	slide->bg_color = TFT_OLIVE;
	slide->fg_color = TFT_ORANGE;
	slide->outline_color = TFT_RED;
	slide->minvalue = 0;
	slide->maxvalue= 100;
	slide->current = 50;
	static_draw(slide);
}

void set_slide_colors(s_slide* slide, color_t afg, color_t abg, color_t alc)
{
	slide->fg_color = afg;
	slide->bg_color = abg;
	slide->outline_color = alc;
	static_draw(slide);
}


void refresh(s_slide* slide, int newvalue)
{
	if (slide->sldvalue != newvalue)
	{
		slide->sldvalue = newvalue;
		redraw(slide);
	}
}

void static_draw(s_slide* slide)
{
	TFT_saveClipWin();
	TFT_resetclipwin();
	TFT_drawRect(slide->x -1, slide->y - 1, slide->width + 2, slide->height + 2, slide->outline_color);
	TFT_restoreClipWin();
}

void redraw(s_slide* slide)
{	float prop = 1.0;
	TFT_saveClipWin();
	TFT_resetclipwin();

	float range = (slide->maxvalue - slide->minvalue);
	if (range > 0)
	{
		prop = ((slide->sldvalue - slide->minvalue) / range);
	};

	printf("DEBUG!! redraw command with prop = %f\r\n", prop);

	TFT_fillRect(slide->x, slide->y, slide->width, slide->height, slide->bg_color);
	TFT_fillRect(slide->x, slide->y, slide->width, slide->height * prop, slide->fg_color);

	TFT_restoreClipWin();
}

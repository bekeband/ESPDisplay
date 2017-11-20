
#include "stdio.h"
#include "slidebutton.h"

static s_slide slide;

void redraw();
void static_draw();

void init_slide(int ax, int ay, int aw, int ah, int ap)
{
	slide.x = ax;
	slide.y = ay;
	slide.width = aw;
	slide.height = ah;
	slide.part = ap;
	slide.slide_value = 0;
	slide.bg_color = TFT_OLIVE;
	slide.fg_color = TFT_ORANGE;
	slide.outline_color = TFT_RED;
	slide.minvalue = 0;
	slide.maxvalue= 100;
	slide.current = 50;
	static_draw();
}

void set_slide_colors(color_t afg, color_t abg, color_t alc)
{
	slide.fg_color = afg;
	slide.bg_color = abg;
	slide.outline_color = alc;
}


void refresh(int newvalue)
{
	if (slide.slide_value != newvalue)
	{
		slide.slide_value = newvalue;
		redraw();
	}
}

void static_draw()
{
	TFT_saveClipWin();
	TFT_resetclipwin();
	TFT_drawRect(slide.x -1, slide.y - 1, slide.width + 2, slide.height + 2, slide.outline_color);
	TFT_restoreClipWin();
}

void redraw()
{
	TFT_saveClipWin();
	TFT_resetclipwin();

	float prop = ((slide.current - slide.minvalue) * slide.height / (slide.maxvalue - slide.minvalue));
	printf("prop value = %f\r\n", prop);



	TFT_fillRect(slide.x, slide.y, slide.width, slide.height, slide.fg_color);

	TFT_restoreClipWin();
}

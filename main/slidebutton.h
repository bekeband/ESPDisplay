/*
 * slidebutton.h
 *
 *  Created on: 2017. nov. 19.
 *      Author: bekeband
 */

#ifndef MAIN_SLIDEBUTTON_H_
#define MAIN_SLIDEBUTTON_H_

#include "tftspi.h"
#include "tft.h"

typedef struct {

	color_t fg_color;
	color_t bg_color;
	color_t outline_color;

	int sldvalue;
	int x;
	int y;
	int width;
	int height;
	int part;
	int minvalue;
	int maxvalue;
	int current;
}s_slide;

void init_slide(s_slide* slide, int ax, int ay, int aw, int ah, int ap);
void set_slide_colors(s_slide* slide, color_t afg, color_t abg, color_t alc);
void refresh(s_slide* slide, int newvalue);


#endif /* MAIN_SLIDEBUTTON_H_ */

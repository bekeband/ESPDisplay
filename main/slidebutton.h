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

	int slide_value;
	int x;
	int y;
	int width;
	int height;
	int part;
	int minvalue;
	int maxvalue;
	int current;
}s_slide;

void init_slide(int ax, int ay, int aw, int ah, int ap);
void set_slide_colors(color_t afg, color_t abg, color_t alc);
void refresh(int newvalue);


#endif /* MAIN_SLIDEBUTTON_H_ */

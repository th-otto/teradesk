/*
 * Teradesk. Copyright (c) 1993, 1994, 2002  W. Klaren,
 *                               2002, 2003  H. Robbers,
 *                                     2003  Dj. Vukovic
 *
 * This file is part of Teradesk.
 *
 * Teradesk is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Teradesk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Teradesk; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/* 
 * Delay for sliders and other arrows; can be adjusted 
 * for a comfortable feel (cca 100-300ms)
 */

#define ARROW_DELAY 250


/* 
 * Slider type: 0 - item(s) can not be selected in the listbox
 *              1 - item(s) can be selected in the listbox
 */

typedef struct sliderinfo
{
	int type;					/* slider type (0 or 1) */

	LSTYPE **list;				/* pointer to the list scrolled by this slider */

	int up_arrow;				/* 'up arrow' object index */
	int down_arrow;				/* 'down arrow' object index */
	int slider;					/* index of the slider object */
	int sparent;				/* index of the slider parent (background) object */
	int lines;					/* number of visible lines (list items) */
	int n;						/* total number of lines (list items) */
	int line;					/* index of the first visible item (0 - n-lines) */

	void (*set_selector) (struct sliderinfo *slider, boolean draw, XDINFO *info);

	int first;					/* index van eerste regel in objectboom */
	int (*findsel) (void);		/* funktie voor het vinden van het geselekteerde object */
} SLIDER;


void sl_init(OBJECT *tree, SLIDER *slider);
void sl_set_slider(OBJECT *tree, SLIDER *slider, XDINFO *info);
int sl_handle_button(int button, OBJECT *tree, SLIDER *sl, XDINFO *dialog);
int sl_form_do(OBJECT *tree, int start, SLIDER *slider, XDINFO *info);
void set_selector(SLIDER *slider, boolean draw, XDINFO *info);
void ls_sl_init (int n, void *set_sel, SLIDER *sl, LSTYPE **list);
int keyfunc(XDINFO *info, SLIDER *sl, int scancode); 
long calc_slpos(int newpos, long lines);
int calc_slmill(long pos, long lines);


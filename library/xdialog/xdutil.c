/* 
 * Xdialog Library. Copyright (c) 1993, 1994, 2002  W. Klaren,
 *                                      2002, 2003  H. Robbers,
 *                                      2003, 2004  Dj. Vukovic
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


#ifdef __PUREC__
 #include <np_aes.h>
 #include <vdi.h>
#else
 #include <aesbind.h>
 #include <vdibind.h>
#endif

#include <stddef.h>
#include <library.h>

#include "xdialog.h"
#include "internal.h"


/* Currently not used anywhere in TeraDesk

/*
 * If a rectangle has a nonzero size, return 1, otherwise 0
 */

int xd_isrect(RECT *r)
{
	if (r->w != 0 && r->h != 0)
		return 1;
	else
		return 0;
}

*/


/* 
 * Funktie voor het omzetten van een RECT structuur naar een pxy array. 
 * (set diagonal points of a rectangle, not the whole perimeter)
 */

void xd_rect2pxy(RECT *r, int *pxy)
{
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
}


/* 
 * Funktie voor het berekenen van de doorsnede van twee rechthoeken. 
 */

int xd_rcintersect(RECT *r1, RECT *r2, RECT *dest)
{
	int xmin, xmax, ymin, ymax, h1, h2;

	xmin = max(r1->x, r2->x);
	ymin = max(r1->y, r2->y);

	h1 = r1->x + r1->w - 1;
	h2 = r2->x + r2->w - 1;
	xmax = min(h1, h2);

	h1 = r1->y + r1->h - 1;
	h2 = r2->y + r2->h - 1;
	ymax = min(h1, h2);

	dest->x = xmin;
	dest->y = ymin;
	dest->w = xmax - xmin + 1;
	dest->h = ymax - ymin + 1;

	if ((dest->w <= 0) || (dest->h <= 0))
		return FALSE;

	return TRUE;
}


/*
 * Funktie die bepaalt of een bepaald punt binnen een gegeven
 * rechthoek ligt.
 *
 * Parmeters:
 *
 * x	- x coordinaat punt,
 * y	- y coordinaat punt,
 * r	- rechthoek.
 *
 * Resultaat : TRUE als het punt binnen de rechthoek ligt, FALSE
 *			   als dit niet het geval is.
 */

int xd_inrect(int x, int y, RECT *r)
{
	if ((x >= r->x) && (x < (r->x + r->w)) && (y >= r->y) && (y < (r->y + r->h)))
		return TRUE;
	else
		return FALSE;
}


/* 
 * Funktie voor het berekenen van de grootte van de schermbuffer. 
 * Compute size of the screen buffer, depending on resolution
 * and number of colours planes
 */

long xd_initmfdb(RECT *r, MFDB *mfdb)
{
	long size;

	mfdb->fd_w = (r->w + 16) & 0xFFF0;
	mfdb->fd_h = r->h;
	mfdb->fd_wdwidth = mfdb->fd_w / 16;
	mfdb->fd_stand = 0;
	mfdb->fd_nplanes = xd_nplanes;

	size = ((long) (mfdb->fd_w) * (long) r->h * (long) xd_nplanes) / 8L;

	return size;
}


/* 
 * Funktie voor het installeren van user-defined objects. 
 */

void xd_userdef(OBJECT *object, USERBLK *userblk, int cdecl(*code) (PARMBLK *parmblock))
{
	object->ob_type = (object->ob_type & 0xFF00) | G_USERDEF;
	userblk->ub_code = code;
	userblk->ub_parm = object->ob_spec.index;
	object->ob_spec.userblk = userblk;
}


/* 
 * Funktie voor het installeren van "extended" user-defined objects. 
 */

void xd_xuserdef(OBJECT *object, XUSERBLK *userblk, int cdecl(*code) (PARMBLK *parmblock))
{
	userblk->ub_code = code;
	userblk->ub_parm = userblk;
	userblk->ob_type = object->ob_type;
	userblk->ob_flags = object->ob_flags;
	userblk->ob_shift = 0;				/* for scrolling editable texts */
	userblk->ob_spec = object->ob_spec;

	object->ob_type = (object->ob_type & 0xFF00) | G_USERDEF;
/* why ?
	object->ob_flags &= ~(AES3D_1 | AES3D_2);
*/
	object->ob_spec.userblk = (USERBLK *)userblk;
}


/* 
 * Funktie die rechthoek om object bepaalt 
 */

void xd_objrect(OBJECT *tree, int object, RECT *r)
{
	OBJECT *obj = &tree[object];

	objc_offset(tree, object, &r->x, &r->y);
	r->w = obj->ob_width;
	r->h = obj->ob_height;
}


/* 
 * Funktie voor het bepalen van de parent van een object. 
 * find parent of object "object"
 */

int xd_obj_parent(OBJECT *tree, int object)
{
	int i = tree[object].ob_next, j;

	while (i >= 0)
	{
		if ((j = tree[i].ob_head) >= 0)
		{
			do
			{
				if (j == object)
					return i;
				j = tree[j].ob_next;
			}
			while (j != i);
		}
		i = tree[i].ob_next;
	}

	return -1;
}


/* 
 * Funktie voor het bepalen van de geselekteerde radiobutton 
 */

int xd_get_rbutton(OBJECT *tree, int rb_parent)
{
	int i = tree[rb_parent].ob_head;
	OBJECT *obj;

	while ((i > 0) && (i != rb_parent))
	{
		obj = &tree[i];

		if ((obj->ob_state & SELECTED) && (obj->ob_flags & RBUTTON))
			return i;

		i = obj->ob_next;
	}

	return -1;
}


/* 
 * Funktie voor het bepalen van de geselekteerde radiobutton 
 */

void xd_set_rbutton(OBJECT *tree, int rb_parent, int object)
{
	int i = tree[rb_parent].ob_head;	/* first child of parent */
	OBJECT *obj;

	while ((i > 0) && (i != rb_parent))	/* until last child */
	{
		obj = &tree[i];
		if (obj->ob_flags & RBUTTON)	/* watch radiobuttons only */
		{
			if (i == object)
				obj->ob_state |= SELECTED;	/* select this one */
			else
				obj->ob_state &= ~SELECTED;	/* deselect others */
		}

		i = obj->ob_next;
	}
}


/*
 * Funktie voor het verkrijgen van de 'ob_spec': werkt voor normale
 * zowel als USERDEF als XUSERDEF objecten!
 */

/* Use correct types ! */

OBSPEC xd_get_obspec(OBJECT *object)
{
	if ((object->ob_type & 0xFF) == G_USERDEF)
	{
		USERBLK *userblk = object->ob_spec.userblk;

		if (IS_XUSER(userblk))
			return ((XUSERBLK *)userblk)->ob_spec;
		else
			return *(OBSPEC *)&userblk->ub_parm;
	}
	else
		return object->ob_spec;
}


/* not optimum to use

/*
 * Get a pointer to a text field in an object.
 */

char *xd_ptext(OBJECT *ob)
{
	return xd_get_obspec(ob).tedinfo->te_ptext;

}


/*
 * Clear a text field
 */

void xd_zerotext(OBJECT *ob)
{
	*(xd_ptext(ob)) = 0;
}

*/


/*
 * Funktie voor het zetten van de 'ob_spec': werkt voor normale zowel
 * als USERDEF als XUSERDEF objecten!
 */

/* Use correct types ! */

/* Possibly an error; declaration changed to *obspec
void xd_set_obspec(OBJECT *object, OBSPEC obspec)
{
	if ((object->ob_type & 0xFF) == G_USERDEF)
	{
		USERBLK *userblk = object->ob_spec.userblk;

		if (IS_XUSER(userblk))
			((XUSERBLK *)userblk)->ob_spec = obspec;
		else
			userblk->ub_parm = *(long *)&obspec;
	}
	else
		object->ob_spec = obspec;
}
*/

void xd_set_obspec(OBJECT *object, OBSPEC *obspec)
{
	if ((object->ob_type & 0xFF) == G_USERDEF)
	{
		USERBLK *userblk = object->ob_spec.userblk;

		if (IS_XUSER(userblk))
			((XUSERBLK *)userblk)->ob_spec = *obspec;
		else
			userblk->ub_parm = *(long *)obspec;
	}
	else
		object->ob_spec = *obspec;
}




/* DjV 074 ---vvv--- */
/* There are cutrrently no tristate objects in teradesk

/* tristate-button functions... */

int xd_get_tristate(int ob_state)
{
	return ob_state & TRISTATE_MASK;
}

int xd_set_tristate(int ob_state, int state)
{
	return (ob_state & ~TRISTATE_MASK) | (state&0xff);
}

int xd_is_tristate(OBJECT *object)
{
	return ((object->ob_type >> 8) & 0xFF) == XD_RECTBUTTRI;
}
*/

/* DjV 074 ---^^^--- */


/*
 * Funktie voor het aanzetten van clipping.
 *
 * Parameter:
 *
 * r	- clipping rechthoek.
 */

void xd_clip_on(RECT *r)
{
	int pxy[4];

	xd_rect2pxy(r, pxy);
	vs_clip(xd_vhandle, 1, pxy);
}


/*
 * Funktie voor het uitschakelen van clipping.
 */

void xd_clip_off(void)
{
	int pxy[4];

	vs_clip(xd_vhandle, 0, pxy);
}

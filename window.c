/* 
 * Teradesk. Copyright (c) 1993 - 2002  W. Klaren,
 *                         2002 - 2003  H. Robbers,
 *                         2003 - 2007  Dj. Vukovic
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


#include <np_aes.h>	
#include <vdi.h>
#include <tos.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mint.h>
#include <xdialog.h>
#include <library.h>

#include "resource.h"
#include "desk.h"
#include "error.h"
#include "events.h"
#include "xfilesys.h"
#include "font.h"
#include "config.h"
#include "window.h"	/* before dir.h and viewer.h */
#include "dir.h"
#include "file.h"
#include "lists.h"
#include "slider.h"
#include "filetype.h"
#include "icon.h"
#include "icontype.h"
#include "copy.h"
#include "prgtype.h"
#include "viewer.h"
#include "applik.h"
#include "floppy.h"   
#include "showinfo.h"
#include "printer.h"
#include "open.h"	
#include "dragdrop.h"
#include "screen.h"
#include "xscncode.h"
#include "va.h"


#define WMINW 14 /* minimum window width (in character cell units) */
#define WMINH 4	 /* minimum window height (in character cell units) */

extern ITMFUNC 
	*dir_func;	/* pointer to directory window functions */

extern int
	aes_wfunc,	/* result of appl_getinfo(11, ...) */
	aes_ctrl;	/* this should probably be in xdialog.h */

extern char
	*infname;	/* name of the current configuration file */

#if _OVSCAN	
extern long
	over;		/* identification of overscan type */

extern int
	ovrstat;	/* state of overscan */
#endif

extern boolean
	clearline,
	onekey_shorts;	/* true if single-key menu shortcuts exist */

SEL_INFO
	selection;

NEWSINFO1 
	thisw;		/* structure used when saving/loading window data */

SINFO2 
	that;		/* structure used when saving/loading window data */

XDFONT 
	*cfg_font;	/* to data for the font loaded or saved to confi. file */


#if _MINT_
LNAME automask = {0};	/* to compose the autolocator mask */
#else
SNAME automask = {0};	/* no need for long names in single-TOS */
#endif

static RECT 
	icwsize = {0,0,0,0};	/* size of the iconified window */

static size_t 
	aml = 0;				/* length of string in automask */

static XFILE 
	*mem_file;				/* for saving temporarily closed windows */

XUSERBLK
	wxub;

boolean
	autoloc = FALSE,	/* true if autolocator is in effect */
	can_iconify,		/* true if current AES supports iconification */ 
	can_touch;

static boolean 
	wclose = FALSE;			/* true during windows being temporarily closed */

static char 
	floppid;				/* floppy id 'A' or 'B' */



int appl_control(int ap_id, int what, void *out);
void load_settings(char *newinfname);
static void desel_old(void);
static void itm_selall(WINDOW *w);
static int *itm_list(WINDOW *w, int *nselected);
static void wd_type_hndlmenu(WINDOW *w, int title, int item);
static void icw_draw(WINDOW *w);
static boolean in_window(WINDOW *w, int x, int y);
static boolean wd_isiconified(TYP_WINDOW *w);
int cdecl ub_bckbox(PARMBLK *pb);



WD_FUNC wd_type_functions =
{
	wd_type_hndlkey,
	wd_hndlbutton,
	wd_type_redraw,
	wd_type_topped,
	wd_type_bottomed,
	wd_type_topped,
	wd_type_close,	
	wd_type_fulled, 
	wd_type_arrowed,
	wd_type_hslider,
	wd_type_vslider,
	wd_type_sized,
	wd_type_moved,
	wd_type_hndlmenu,
	0L,
	wd_type_iconify,
	wd_type_uniconify
};


/*
 * Window font configuration table
 */

CfgEntry fnt_table[] =
{
	{CFG_HDR, "font" },
	{CFG_BEG},
	{CFG_D,   "iden", &thisw.font.id		},
	{CFG_D,   "size", &thisw.font.size	},
	{CFG_D,   "fcol", &thisw.font.colour	},
	{CFG_X,   "feff", &thisw.font.effects},
	{CFG_END},
	{CFG_LAST}
};


/*
 * Window position configuration table
 */

CfgEntry positions_table[] =
{
	{CFG_HDR, "pos"	},
	{CFG_BEG},
	{CFG_X,   "flag", &thisw.flags	},
	{CFG_D,   "xpos", &thisw.x	}, /* note: can't go off the left edge */
	{CFG_D,   "ypos", &thisw.y	},
	{CFG_D,   "winw", &thisw.ww	},
	{CFG_D,   "winh", &thisw.wh	},
	{CFG_D,   "xicw", &thisw.ix	},
	{CFG_D,   "yicw", &thisw.iy	},
	{CFG_D,   "wicw", &thisw.iw	},
	{CFG_D,   "hicw", &thisw.ih	},
	{CFG_END},
	{CFG_LAST}
};


/*
 * Load or save configuration of window font
 * Note: this is initialized to zero in wd_config.
 * There are no initialization or default routines for this
 */

CfgNest cfg_wdfont
{
	if ( io == CFG_SAVE )
		thisw.font = *cfg_font;
	else
		memclr(&thisw.font, sizeof(XDFONT));

	*error = handle_cfg(file, fnt_table, lvl, CFGEMP, io, NULL, NULL );

	if ( (io == CFG_LOAD) && (*error == 0) && thisw.font.size ) 
	{
		*cfg_font = thisw.font;
			fnt_setfont(thisw.font.id, thisw.font.size, cfg_font);
	}
}


/*
 * Turn off and reset autolocator and its name mask in the top window
 */

void autoloc_off(void)
{
	WINDOW *w = xw_top();

	autoloc = FALSE;
	aml = 0;
	*automask = 0;

	if(w && xw_type(w) == DIR_WIND && *(((DIR_WINDOW *)w)->info + 1) == '(' )
		dir_info((DIR_WINDOW *)w);
}


/* 
 * Some size-optimization mouse-related functions (reduced number of params)
 */

void arrow_mouse(void)
{
	graf_mouse(ARROW, NULL);
}


void hourglass_mouse(void)
{
	graf_mouse(HOURGLASS, NULL);
}


/*
 * Write window text in transparent mode at location (x,y)
 */

void w_transptext( int x, int y, char *text)
{
	xd_vswr_trans_mode(); 
	v_gtext(vdi_handle, x, y, text);
}


/*
 * Is a window item some kind of a file (i.e. a file or a program?)
 */

boolean isfileprog(ITMTYPE type)
{
	return (type == ITM_FILE || type == ITM_PROGRAM);
}


/* 
 * Ensure that a loaded window is in the screen in case of a resolution change 
 * (or if these data were loaded from a human-edited configuration file)
 * and that it has a certain minimum size.
 * Note 1: windows can't go off the left and upper edges.
 * Note 2: width and height are in character-cell units, x and y in pixels!
 */

static void wrect_in_screen(RECT *info, boolean normalsize)
{
	/* 
	 * Window should be at least 32 pixels in the screen 
	 * both horizontally and vertically
	 */

	info->x = minmax(0, info->x, xd_desk.w - 32);
	info->y = minmax(0, info->y, xd_desk.h - 32);

	info->w = min( info->w, xd_desk.w / xd_fnt_w );
	info->h = min( info->h, xd_desk.h / xd_fnt_h );

	/* Below is an arbitrary minimum window size (14 char x 5 lines) */

	if (normalsize )
	{	
		/* Note: do not use max() here, code would be longer */
		
		if (info->w < WMINW)
			info->w = WMINW;

		if (info->h < WMINH)
			info->h = WMINH;
	}
}


/*
 * Check if a window is in the screen 
 */

void wd_in_screen( WINFO *info ) 
{
	wrect_in_screen( (RECT *)(&(info->x)), (info->flags.iconified) ? FALSE : TRUE);
	wrect_in_screen( (RECT *)(&(info->ix)), TRUE );
}


/*
 * Determine unit cell size in a window.
 * It will be equal to character size (text font or directory font)
 * or icon size, depending on window type and display mode.
 * If parameter icons is false, icon size will not be used but 
 * size of the screeen (system) font will be used instead.
 * Generally, icons=true should be used when using wd_cellsize() to
 * calculate numbers of rows and columns; icons = false should be used
 * when determining window size.
 * See also wd_sizes().
 */

void wd_cellsize(TYP_WINDOW *w, int *cw, int *ch, boolean icons)
{
	if (w && xw_type(w) == TEXT_WIND)
	{
		*cw = txt_font.cw;
		*ch = txt_font.ch;
	}
	else /* assumed to be dir window; also if w is NULL */
	{
		if (options.mode == TEXTMODE)
		{
			*cw = dir_font.cw;
			*ch = dir_font.ch + DELTA;
		}
		else if(icons)
		{
			*cw = ICON_W;
			*ch = ICON_H;
		}
		else
		{
			*cw = xd_fnt_w;
			*ch = xd_fnt_h;
		}
	}
} 


/*
 * Configuration of windows positions
 * Note: in this routine it is assumed that "iconified size" rectangle data
 * immedately follows "window size" rectangle data both in NEWSINFO1 thisw 
 * and WINFO. If NEWSINFO1 or WINFO is modified in this aspect, code below 
 * will fail!
 */

CfgNest positions
{
	WINFO 
		*w = thisw.windows;

	int
		i;

	size_t
		s = 2 * sizeof(RECT); /* data size */


	if (io == CFG_SAVE)
	{
		/* Save data... */

		for (i = 0; i < MAXWINDOWS; i++)
		{
			thisw.i = i;

			memcpy(&thisw.x, &(w->x), s); /* copy all in one row */

			thisw.flags = w->flags;
	
			/* 
			 * In order to save on size of configuration file,
			 * don't save iconified size of noniconified windows 
			 */

			if (!(w->flags.iconified))
				memclr(&(thisw.ix), sizeof(RECT));

			*error = CfgSave(file, positions_table, lvl, CFGEMP);

			w++;

			if ( *error != 0 )
				break;
		}
	}
	else
	{
		/* Load data... */

		memclr(&thisw.x, (size_t)(s + sizeof(thisw.flags)));

		*error = CfgLoad(file, positions_table, MAX_KEYLEN, lvl);
		
		if ( (*error == 0) && (thisw.i < MAXWINDOWS) )
		{
			w += thisw.i;

			memcpy(&(w->x), &(thisw.x), s); /* copy all in one row */

			w->flags = thisw.flags;

			thisw.i++;
		}
	}
}


/*
 * Configuration table for one window type
 */

CfgEntry wtype_table[] =
{
	{CFG_HDR,  NULL }, /* keyword will be substituted */
	{CFG_BEG},
	{CFG_NEST, "font", cfg_wdfont },
	{CFG_NEST, "pos", positions	},		/* Repeating group */
	{CFG_END},
	{CFG_LAST}
};


/********************************************************************
 *																	*
 * Functions for changing the View menu.							*
 *																	*
 ********************************************************************/


/*
 * Check (mark) only one of several menu items which are in sequence;
 * unmark all others.
 * n=number of items, obj = first item, i= item which is to be checked
 */

static void menu_checkone(int n, int obj, int i)
{
	int j;
	
	for ( j = 0; j < n; j++ )
		menu_icheck(menu, j + obj, (i == j) ? 1 : 0);
}


/* 
 * Set sorting order.
 * Note: it is tacitly assumed here that "sorting" 
 * menu items follow in a fixed sequence after "sort by name" and
 * that there are exactly five sorting options. 
 */

static void wd_set_sort(int type)
{
	int
		i = 1 - (type >> 4);


	menu_checkone( 5, MSNAME, (type & ~WD_REVSORT) );
	menu_checkone( 1, MREVS,  i);
}


/* 
 * Check/mark (or the opposite) menu items for showing directory info fields 
 * It is assumed that these are in fixed order: 
 * MSHSIZ, MSHDAT, MSHTIM, MSHATT, MSHOWN;
 * check it with order of WD_SHSIZ, WD_SHDAT, WD_SHTIM, WD_SHATT, WD_SHOWN
 */

static void wd_set_fields( int fields )
{
	int i;

	for ( i = 0; i < 5; i++ )
		menu_icheck(menu, MSHSIZ + i, ((fields & (WD_SHSIZ << i) ) ? 1 : 0));
}


/*
 * Count open windows (text, directory or accessory type)
 * Note: this routine relies on definitions of DIR_WIND, TEXT_WIND and 
 * ACC_WIND being in a sequence.
 */

int wd_wcount(void)
{
	WINDOW 
		*h = xw_first();

	int 
		n = 0;


	while (h)
	{
		if (xw_type(h) >= DIR_WIND && xw_type(h) <= ACC_WIND)
			n++;

		h = xw_next(h);
	}

	return n;
}


/*
 * Reset "selection"
 */

void wd_noselection(void)
{
	selection.w = NULL;
	selection.selected = -1;
	selection.n = 0;
}


/*
 * Note the first selected item in a window.
 * Attention: the item list is deallocated here!
 */

static void wd_iselection(WINDOW *w, int n, int *list)
{
	wd_noselection();

	if(w && n > 0)
	{
		selection.n = n;
		selection.w = w;

		if (n == 1)
			selection.selected = list[0];
	}

	free(list);
}


#if _MINT_
/* 
 * Top the specified application
 * This works without any open windows, but unfortunately
 * not in all AESses. Will be ok in N.AES, XaAES and Magic 
 */

void wd_top_app(int apid)
{
	if(naes || aes_ctrl) 
		appl_control(apid, 12, NULL);	/* N.AES, XaAES */
	else 
		wind_set(-1, WF_TOP, apid);		/* Magic */
}
#endif


/*
 * Save the id of the top window (code=0) or restore it (code=1).
 * To be used when opening dialogs asked for by the clients of the 
 * AV or the FONT protocol. This should prevent TeraDesk from remaining
 * topped in a multitasking environment, after closing the dialog.
 * Probably relevant only in multitasking.
 * Note: this is not exactly right- if there are no open windows,
 * problems may be encountered.
 *
 */

#if _MINT_
void wd_restoretop(int code, int *whandle, int *wap_id)
{
	int p2, p3, p4;

	if(mint || geneva)
	{
		if(code == 0)
		{
			/* 
			 * Memorize which window is currently topped 
			 * Note: wind_get(, WF_TOP,...) is supposed to
			 * return ap_id of the owner in p2, but this does not
			 * seem to work, thence another call to get it.
			 * If failed, set *wap_id to -1; Data in p2 to p4 is
			 * returned but currently ignored.
			 */

			*wap_id = -1;
/* maybe there is no need ???
			if((aes_wfunc & 17) == 17) /* WF_TOP and WF_OWNER supported ? */ 
*/
				if(wind_get(0, WF_TOP, whandle, &p2, &p3, &p4))
					if(!wind_get( *whandle, WF_OWNER, wap_id, &p2, &p3, &p4))
						*wap_id = -1;
		}
		else
		{
			/* 
			 * Send a message to top the memorized window, but 
			 * do not send to itself- TeraDesk is already topped.
			 * Do this only if data are valid.
			 */

			if (*wap_id >= 0 && *wap_id != ap_id)
			{
				WINDOW w;
				w.xw_handle = *whandle;
				w.xw_ap_id = *wap_id;
				xw_send(&w, WM_TOPPED);
			}

			/* Just in case... */

			wd_top_app(av_current);
		}
	}
}
#endif


/*
 * Does a pointer point to a directory or text window?
 */

boolean wd_dirortext(WINDOW *w)
{
	return (w && (xw_type(w) == DIR_WIND || xw_type(w) == TEXT_WIND));
}


/********************************************************************
 *																	*
 * Funkties voor het enablen en disablen van menupunten.			*
 *																	*
 ********************************************************************/


/*
 * Make it shorter and a little bit slower...
 */

void wd_menu_ienable(int item, int enable)
{
	menu_ienable(menu, item, enable);
}


/* 
 * Funktie die menupunten enabled en disabled, die met objecten te
 * maken hebben. 
 * *w: pointer to the window in which the selection is made
 */

void itm_set_menu(WINDOW *w)
{

	WINDOW
		*wtop;				/* pointer to top window */

	int 
		n = 0,				/* number of selected items */
	    *list = NULL,		/* pointer to a list of selected items */
		wtoptype = 0,		/* type of the topped window */
		nwin = 0,			/* number of open windows */
	    i = 0;				/* counter */

	char 
		drive[8];			/* disk drive name string */

	ITMTYPE 
		type,				/* type of an item in the list */
		type2 = ITM_FILE;	/* same */

	boolean 
		topicf = FALSE,		/* true if top window is iconified */
		nonsel,				/* true if nothing selected and dir/text topped */
		showinfo = FALSE,	/* true if show info is enabled */
	    enab = FALSE,		/* flag for enabling diverse options */
	    enab2 = FALSE;		/* flag for enabling diverse options */

	/*
	 * Does this (selection) window exist at all, and is there a list 
	 * of selected items? If the window does not exist, locally zero
	 * the pointer to it.
	 */

	if((list = itm_list(w, &n)) == NULL)
		w = NULL;

	/* 
	 * Find out the type of the top window (ONLY directory/text/acc).
	 * Also find if windows are iconified and how many exist.
	 * Note: in earlier versions xw_top() was used here instead of xw_first()
	 * but that could not properly detect open AV-client windows
	 * when the desktop was topped.
	 */

	wtop = xw_first();

	if ( wtop )
		topicf =  wd_isiconified((TYP_WINDOW *)wtop); 

	/* Now find the real topped window */

	wtop = xw_top();

	if ( wtop )
		wtoptype = xw_type(wtop);


	/* Count directory, text and accessory windows */

	nwin = wd_wcount();

	nonsel = (!n && wd_dirortext(wtop));

	if(n > 0)
		showinfo = TRUE;

	/* 
	 * Enable show info even if there is no selection, but top window
	 * is a directory window (show info of that drive then)
	 * or a text window (show info on the displayed file then)
	 */

	can_touch = showinfo;

	if ( nonsel )
	{
		can_touch = FALSE;
		showinfo = TRUE;
	}

	wd_menu_ienable(MSHOWINF, showinfo);
	wd_menu_ienable(MSEARCH, showinfo);

	/* 
	 * New dir is OK only if a dir window is topped and not iconified, 
	 * and either nothing is selected or a single item is selected and 
	 * a link can be created (assuming that mint +non-tos filesystem
	 * always mean that a link can be created; it should be better
	 * checked with dpathconf!!! 
	 */

	if 
	( 
		(wtoptype == DIR_WIND && !topicf)  &&
		( 
			n == 0
#if _MINT_
 			|| (mint && (n == 1))  
#endif
		)
	)
		enab = TRUE;

	wd_menu_ienable(MNEWDIR, enab);

	/*
	 * "enab" will be set if there are selected items and
	 * all of the selected items are files, folders or programs.
	 * "enab2" (for printing) will be reset if any item is not a file.
	 */

	enab = (n > 0); 
	enab2 = enab;		/* will always become false sooner then enab */

	i = 0;
	while ( (i < n) && enab )
	{
		type = itm_type(w, list[i++]);
		enab = ( isfile(type) );  
		enab2 = ( enab2 && isfileprog(type) );
	}	

	/*
 	 * enab2 (for printing) will also be set if a dir or text window 
	 * is topped  (and not iconified) and nothing is selected
	 */

	if ( nonsel && !topicf )
		enab2 = TRUE;
  
	/*
	 * Enable delete only if there are only files, programs and 
	 * folders among selected items;
	 * Enable print only if there are only files among selected items
	 * or a window is open (to print directory or text)
	 */

	wd_menu_ienable(MDELETE, enab && (n > 0));
	wd_menu_ienable(MPRINT, enab2);

	/* Determine enabling states for Compare, Programtype and App menu items */

	if (n == 0)
	{
		enab = TRUE;
		enab2 = TRUE;
	}
	else /* n > 0 */
	{
		enab = FALSE;
		enab2 = FALSE;

		type = itm_type(w, list[0]);

		if (n >= 2)
			type2 = itm_type(w, list[1]);

		if ( (n < 3) && isfileprog(type) && isfileprog(type2) )
		{
			enab = TRUE;
			if ( type == ITM_PROGRAM )
				enab2 = TRUE;
		}
	}

	/* Compare will be enabled only if there are not more than two files selected */

	wd_menu_ienable(MCOMPARE, enab );

	/* 'Select all' is possible only on directory or desk window */

	menu_ienable
	(
		menu, 
		MSELALL, 
		(int)((wtoptype == DIR_WIND && !topicf) || wtoptype == DESK_WIND)
	);

	/* Enable duplication of dir and text windows */

	wd_menu_ienable(MDUPLIC, (int)wd_dirortext(wtop));

	/* Enable window cycling if there is more than one window open */

	wd_menu_ienable(MCYCLE, (nwin > 1) ? 1 : 0 );

	/* 
	 * Enable setting of applications and program types only for
	 * appropriate selected item types (see above)
	 */

	if ( n > 1 )
	{
		enab = FALSE;
		enab2 = FALSE;
	}

	wd_menu_ienable(MPRGOPT, enab);
	wd_menu_ienable(MAPPLIK, enab2); 

	/* Enable setting of window icons */

	wd_menu_ienable(MIWDICN, (n < 2) );

  	/*
	 * enable disk formatting and disk copying
	 * only if a single drive is selected and it is A or B;
	 * use the opportunity to say which drive is to be used 
	 * (is it always uppercase A or B, even in other filesystems ?)
	 */

	enab = FALSE;

	if ( (n == 1) && (type == ITM_DRIVE) )
	{
		char *fullname = itm_fullname ( w, *list );

		strsncpy ( drive, fullname , sizeof(drive) );
		free(fullname);

		drive[0] &= 0x5F; /* to uppercase */

		if 
		(   
			   ( drive[0] >= 'A' )
			&& ( drive[0] <= 'B' )
			&& ( drive[1] == ':' )
		)
		{  
			floppid = drive[0];      /* i.e. floppid= 65dec (A) or 66dec (B) */
			enab = TRUE;
		}  
	}

/* By removing floppy formatting/copying from the resource, this will be deleted */

#if MFFORMAT
	wd_menu_ienable(MFCOPY, enab );
	wd_menu_ienable(MFFORMAT, enab );
#endif

	if(nwin && !topicf)
		enab = TRUE;
	else
		enab = FALSE;

	/* Enable window fulling, iconifying, closing... if there are open windows */

	wd_menu_ienable(MFULL, enab);
	wd_menu_ienable(MICONIF, nwin);
	wd_menu_ienable(MCLOSE, (nwin && wtoptype == DIR_WIND));
	wd_menu_ienable(MCLOSEW, nwin);
	wd_menu_ienable(MCLOSALL, nwin);
	menu_icheck(menu, MICONIF, topicf);

	/* Notify the first selected item */

	wd_iselection(w, n, list); /* attention: list deallocated */
}


/*
 * What this routine does is also set in itm_set_menu,
 * but below is smaller/faster.
 */

void wd_setselection(WINDOW *w)
{
	int n, *list;

	list = itm_list(w, &n);		/* note: allocate list, or set to null */
	wd_iselection(w, n, list);	/* note: also deallocates list */
}


/********************************************************************
 *																	*
 * Funkties voor het deselecteren van objecten in windows.			*
 *																	*
 ********************************************************************/

/*
 * Deselect all selected objects in directory windows or
 * on the disktop. This function also turns off the autolocator
 */

void wd_deselect_all(void)
{
	WINDOW *w = xw_first();


	while (w)
	{
		if (xw_type(w) == DIR_WIND)
			((ITM_WINDOW *)w)->itm_func->itm_select(w, -1, 0, TRUE);

		w = xw_next(w);
	}

	((ITM_WINDOW *)desk_window)->itm_func->itm_select(desk_window, -1, 0, TRUE);

	wd_setselection(NULL);
	autoloc_off();
 }


/* 
 * Funktie die aangeroepen moet worden, als door een andere oorzaak
 * dan het met de muis selekteren of deselekteren van objecten,
 * objecten gedeselekteerd worden. 
 */

void wd_reset(WINDOW *w)
{
	if (w)
	{
		if (selection.w == w)
			wd_setselection(w); 
	}
	else
	{
		if (xw_exist(selection.w) == 0)
			wd_setselection(NULL); 
	}
	
	autoloc_off();
}


/********************************************************************
 *																	*
 * Funktie voor het opvragen van het pad van een window.			*
 *																	*
 ********************************************************************/


/*
 * Return a pointer to the string containing the path 
 * of the uppermost directory window
 */

const char *wd_toppath(void)
{
	WINDOW *w = xw_first();


	while (w)
	{
		if (xw_type(w) == DIR_WIND)
			return ((DIR_WINDOW *)w)->path; 

		w = xw_next(w);
	}

	return NULL;
}


/* 
 * Return a pointer to the string containing path of a directory window
 * or path+filename of a text window. Otherwise return NULL.
 */

const char *wd_path(WINDOW *w)
{
	if(wd_dirortext(w))
		return ((TYP_WINDOW *)w)->path; 

	return NULL;		
}


/*
 * This function loops through all windows and performs desired action
 * on directory windows. Action is defined in an exernal function that has
 * the pointer to the window as an argument.
 */

void wd_do_dirs(void *func)
{
	WINDOW *w = xw_first();

	void(*dirfunc)(WINDOW *w) = func;


	while(w)
	{
		if(xw_type(w) == DIR_WIND)
			dirfunc(w);

		w = xw_next(w);
	}
}


/********************************************************************
 *																	*
 * Funkties voor het verversen van windows als er een file gewist	*
 * of gekopieerd is.												*
 *																	*
 ********************************************************************/

void wd_set_update(wd_upd_type type, const char *fname1, const char *fname2)
{
	WINDOW *w = xw_first();


	while (w)
	{
		if (xw_type(w) == DIR_WIND)
			((ITM_WINDOW *)w)->itm_func->wd_set_update(w, type, fname1, fname2);

		w = xw_next(w);
	}

	((ITM_WINDOW *)desk_window)->itm_func->wd_set_update(desk_window, type, fname1, fname2);
	app_update(type, fname1, fname2);
}


void wd_do_update(void)
{
	wd_do_dirs(dir_func->wd_do_update);
	((ITM_WINDOW *)desk_window)->itm_func->wd_do_update(desk_window);
}


void wd_update_drv(int drive)
{
	WINDOW
		*w = xw_first();

	ITMFUNC
		*thefunc;


	while (w)
	{
		if (xw_type(w) == DIR_WIND)
		{
			thefunc = ((ITM_WINDOW *)w)->itm_func;

			if (drive == -1)
				/* update all */
				thefunc->wd_set_update(w, WD_UPD_ALLWAYS, NULL, NULL);
			else
			{			
				const char *path = thefunc->wd_path(w);

				boolean goodpath = (((path[0] & 0x5F - 'A') == drive) && (path[1] == ':')) ? TRUE : FALSE;
#if _MINT_
				if (mint)
				{
					if ((drive == ('U' - 'A')) && goodpath ) /* this is for U drive */
						thefunc->wd_set_update(w, WD_UPD_ALLWAYS, NULL, NULL);
					else
					{
						XATTR attr;

						if ((x_attr(0, FS_LFN, path, &attr) == 0) && (attr.dev == drive)) /* follow the link */
							thefunc->wd_set_update(w, WD_UPD_ALLWAYS, NULL, NULL);
					}
				}
				else
#endif
				{
					if ( goodpath )
						thefunc->wd_set_update(w, WD_UPD_ALLWAYS, NULL, NULL);
				}
			}

		}

		w = xw_next(w);
	}

	wd_do_update();
}


/*
 * A size-savng aux. routine; copies noniconified window size to
 * current window size. To be used before opening a window.
 */

void wd_restoresize(WINFO *winfo)
{
	if(winfo->flags.iconified)
	{
		*(RECT *)(&(winfo->x)) = *(RECT *)(&(winfo->ix));

		if(!can_iconify)
			winfo->flags.iconified = 0;
	}
}


/*
 * Reset some window flags
 */

void wd_setnormal(WINFO *info)
{
	info->flags.fulled = 0;
	info->flags.fullfull = 0;
	info->flags.iconified = 0;
}


/*
 * Close a directory, text or accessory window.
 * Note: take care not to attempt to use it on desk window
 */

void wd_type_close( WINDOW *w, int mode)
{
	int wt = xw_type(w);

	/* Act according to window type */

	if (wt == ACC_WIND)
		va_close(w);
	else if(wd_dirortext(w))
	{
		/* 
		 * fulled and setmask flags are preserved for temporary close;
		 * otherwise, they are reset.
		 * Window size and coordinates are reset to values before iconifying
		 */

		if (!wclose)
		{
			WINFO *info = ((TYP_WINDOW *)w)->winfo;

			wd_restoresize(info);
			wd_setnormal(info);

			info->flags.setmask = 0;
		}

		if (wt == TEXT_WIND)
			txt_closed(w);
		else
			dir_close(w, mode);
	}		
}


/*
 * Handle menus in windows (currently in text window only)
 */

static void wd_type_hndlmenu(WINDOW *w, int title, int item)
{
	if (xw_type(w) == TEXT_WIND)
		txt_hndlmenu(w, title, item);
}


/*
 * Delete all windows EXCEPT pseudowindows opened through AV-protocol
 */

void wd_del_all(void)
{
	WINDOW
		*prev,
		*w = xw_last();


	while (w)
	{
		prev = xw_prev(w);

		if ( xw_type(w) != ACC_WIND )
			wd_type_close(w, 1);		

		w = prev;
	}
}


/*
 * Check if a window was successfully opened. If so, return TRUE.
 * Note: this routine resets error to 0!
 * Used for checking opening of saved windows.
 * Currently, return status is, in fact, irelevant.
 */

boolean wd_checkopen(int *error)
{
	if (*error)
	{
		alert_printf(1, AWOPERR, get_message(*error));
		*error = 0;
		return FALSE;
	}

	return TRUE;
}


/********************************************************************
 *																	*
 * Funktie voor het afhandelen van menu events.						*
 *																	*
 ********************************************************************/


/* 
 * Sort directory windows according to the selected key 
 * (name, size, date...) 
 */

static void wd_sort(int sort) 
{
	options.sort = sort;	/* save this in options */

	wd_set_sort(sort);		/* mark a menu item */
	wd_do_dirs(dir_sort);	/* sort directory */
}


void wd_seticons(void)
{
	wd_do_dirs(dir_seticons);
}


/*
 * Sometimes it is convenient to fake that the file in a topped text window
 * is selected in a directory window. Path must be kept to be deallocated
 * after later operations.
 */

static void wd_ontext(WINDOW *ww, char **thepath)
{
	char
		*thisname = ((TXT_WINDOW *)xw_top())->path,
		*thename;


	*thepath = fn_get_path( thisname );

	if (*thepath)
	{
		thename = fn_get_name( thisname );
		dir_simw((DIR_WINDOW *)ww, *thepath, thename, ITM_FILE);
	}
	else
		xform_error(ENSMEM);
}


/* 
 * Handle some of the main menu activities 
 * (the rest is covered in main.c).
 * Checking of window existence is not needed in some cases, because
 * those menu items would have been already disabled otherwise.
 * See itm_set_menu()
 */

void wd_hndlmenu(int item, int keystate)
{
	DIR_WINDOW
		simw;			/* space for a simulated dir window */

	WINDOW 
		*w, 			/* pointer to the window in which a selection is made */
		*ww,			/* pointer to the real or simulated window to search in or show info of */
		*wfirst,		/* pointer to the topmost TeraDesk's window */
		*wtop;			/* pointer to the top window */

	int 
		i,				/* aux. counter */
		n = 0,			/* number of items selected */ 
		*list = NULL,	/* pointer to a list of selected items indices */
		*listi,			/* pointer to an item in this list */
		wtoptype = 0; 	/* type of the top window */

	static int 
		fmsg[] = {0, 0, 0, 0, -1, -1, -1, -1};	/* a message to itself */

	char			
		*thepath = NULL,/* aux. path */
		*toppath = NULL;/* path of the topped window */


	w = selection.w;
	ww = w;

	/* Create a list of selected items */

	if(w)
		list = itm_list(w, &n); /* n = 0 if a list can not be allocated */

	/* Take the opportunity to update window order */

	xw_bottom();	
	wtop = xw_top();

	if ( wtop )
		wtoptype = xw_type(wtop);

	wfirst = xw_first();

	/* Now, what to do */

	switch (item)
	{
		case MOPEN:
		{
			if(n)
			{
				int
					k,
					delayt;
	
				ITMTYPE
					it;
	
				listi = list;
	
				for(i = 0; i < n; i++ )
				{
					delayt = 0;
					k = keystate;
					it = itm_type(w, *listi);
	
					/* Provision for multiple openings... */
	
					if (n > 1)
					{
						wd_drawall();
	
						if(it == ITM_FOLDER || it == ITM_DRIVE || it == ITM_PREVDIR)
							k |= K_ALT; /* open directories in new windows */
						else if (it != ITM_FILE || app_find(itm_name(w, list[i]), TRUE) != NULL )
							delayt = 2000; /* a program or a file assigned to a program */
					}
	
					if (itm_open(w, *listi, k))
					{
						/* 
						 * In single-TOS, can't deselect after a program
						 * because windows will have been reopened
						 */
	
						if(selection.w)
							itm_select(w, *listi, 2, TRUE); /* deselect this item */
					}
	
					/* Wait some time for programs to settle */
	
					wait(delayt);
	
					listi++;
				}
			}
			else
				/* If nothing is selected, open the dialog to enter item spec. */
				item_open( NULL, 0, 0, NULL, NULL );
		
			break;
		}
		case MSHOWINF:
		case MSEARCH:
		{
			*dirname = 0; /* this is the buffer for the search pattern */
	
			if (w == NULL && wtop)
			{
				/* Nothing is selected, info on the disk of the top window  */
	
	 			if ( wtoptype == DIR_WIND )
				{			
					if ( (thepath = strdup(wd_toppath()) ) != NULL )
					{
						/* 
						 * Note: if the next line is removed, and
						 * item type is set to ITM_FOLDER, then
						 * info on the current directory will be shown
						 * instead on the drive
						 */
	
						if(item == MSHOWINF)
							thepath[3] = 0;
	
						ww = (WINDOW *)&simw;
						dir_simw(&simw, thepath, ".", ITM_DRIVE);
					}
				}
				else if ( wtoptype == TEXT_WIND )
				{
					ww = (WINDOW *)&simw;
					wd_ontext(ww, &thepath);
				}
			}
	
			/* Perform search and/or show information */
	
			if(n || (thepath && (list = itm_list(ww, &n)) != NULL)) 
			{
				if(n == 1 && itm_type(ww, list[0]) == ITM_FILE)
					cv_fntoform(searching, SMASK, itm_name(ww, list[0]));
	
				if ( item == MSHOWINF || !app_specstart( AT_SRCH, ww, list, n, 0 ) )
					item_showinfo(ww, n, list, (item == MSHOWINF) ? FALSE : TRUE);
	
				if(w == NULL) /* deselect all in a simulated window */
					wd_noselection();
			}
	
			free(thepath);
			break;
		}
		case MNEWDIR:
		{
#if _MINT_
			if 
			(
				mint && 
				(n == 1) &&
				(wtoptype == DIR_WIND) &&
				((( (DIR_WINDOW *)wtop)->fs_type & FS_LNK) != 0)
			)
			{
				/*
				 * Create a symbolic link.
				 * Note: with a change in dir_newlink and the resource file
				 * (add Abort & Skip buttons) this can easily be modified
				 * to handle more than one item at a time.
				 */
	
				if ( (thepath = itm_fullname(w, list[0])) != NULL)
				{
					dir_newlink(wtop, thepath);
					free(thepath);
				}
				else
					xform_error(ENSMEM);
			}
			else
#endif
				if ( n == 0 && wtoptype == DIR_WIND )
					dir_newfolder(wtop);
			break;
		}
		case MCOMPARE:
		{
			if(w == NULL && wtoptype == TEXT_WIND)
			{
				ww = (WINDOW *)&simw;
				wd_ontext(ww, &thepath);
			}
	
			list = itm_list(ww, &n);
	
			if ( !app_specstart( AT_COMP, ww, list, n, 0 ) )
				compare_files(ww, n, list); /* Only items #0 and #1 from the list */
	
			if(w == NULL) /* deselect all in a simulated window */
				wd_noselection();
	
			free(thepath);
			break;
		}
		case MDELETE:
		{
			if(n)
				itmlist_wop(w, n, list, CMD_DELETE);
			break;
		}
		case MPRINT:
		{
			int cmd = CMD_PRINT;
	
			printmode = PM_TXT;
	
			if(w == NULL && wtop)
			{
				/* Nothing selected, print directory or contents of text window */
	
				if ( wtoptype == DIR_WIND )
				{
						/* Select everything in this window */
	
					cmd = CMD_PRINTDIR;
					ww = wtop;
					itm_selall(ww);
				}
				else if ( wtoptype == TEXT_WIND )
				{
					if ( ((TXT_WINDOW *)wtop)->hexmode )
						printmode = PM_HEX;
	
					ww = (WINDOW *)&simw;
	
					wd_ontext(ww, &thepath);
				}
			}
	
			if(n || ((thepath || (wtoptype == DIR_WIND)) && (list = itm_list(ww, &n)) != NULL)) 
			{
				itmlist_wop( ww, n, list, cmd); 
	
				if(cmd == CMD_PRINTDIR || w == NULL)
					wd_deselect_all();
			}
	
			free(thepath);
			break;
		}
#if MFFORMAT
		case MFCOPY:
		{
			formatfloppy(floppid, FALSE);
			break;
		}
		case MFFORMAT:
		{
			if ( !app_specstart(AT_FFMT, w, list, n, 0) )
				formatfloppy(floppid, TRUE);
			break;
		}
	#endif
		case MSETMASK:
		{
			int oa = options.attribs;
	
			if ( wtoptype == DIR_WIND )
				dir_filemask((DIR_WINDOW *)wtop);
			else
				wd_filemask(NULL);	/* the dialog */
	
			if(options.attribs != oa)	/* show hidden or parent... for all windows */
				wd_do_dirs(dir_newdir);
	
			break;
		}
		case MSHOWICN:
		{
			options.mode = !options.mode;
			menu_icheck(menu, MSHOWICN, options.mode);
			wd_sizes();	
			wd_do_dirs(dir_mode);	
			break;
		}
		case MAARNG:
		{
			options.aarr = !options.aarr; 				/* set option */
			menu_icheck(menu, MAARNG, options.aarr);	/* set menu item */
			wd_do_dirs(dir_disp_mode);
			break;
		}
		case MSNAME:
		case MSEXT:
		case MSDATE:
		case MSSIZE:
		case MSUNSORT:
		{
			wd_sort( (item - MSNAME) | (options.sort & WD_REVSORT) );
			break;
		}
		case MREVS:
		{
			options.sort ^= WD_REVSORT;
			wd_sort(options.sort);
			break;
		}
		case MSHSIZ: /* beware of the order of cases vs flag values */
		case MSHDAT:
		case MSHTIM:
		case MSHATT:
		case MSHOWN:
		{
			options.fields ^= (WD_SHSIZ << (item - MSHSIZ));
			wd_do_dirs(dir_newdir);
			wd_set_fields(options.fields);
			break;
		}
		case MFULL:
		{
			i = WM_FULLED;
			goto sendit;
		}
		case MCLOSE:
		{
			i = -1;
			goto closeit;
		}
		case MICONIF:
		{
			if(wd_isiconified((TYP_WINDOW *)wfirst))
				i = WM_UNICONIFY;
			else
				i = WM_ICONIFY;
			
			sendit:;
	
			if(wfirst)
			{
				/* fake a message about the top window */
				fmsg[0] = i;
				fmsg[3] = wfirst->xw_handle;
				xw_hndlmessage(fmsg);
			}
	
			break;
		}
		case MCLOSALL:
		{
			va_delall(-1, FALSE);
			wd_del_all();
		}
		case MCLOSEW:
		{
			i = 1;
			
			closeit:;
	
			if(wfirst)
				wd_type_close(wfirst, i);
	
			break;
		}
		case MDUPLIC:
		{
			toppath = strdup(wd_path(wtop));
	
			if ( toppath )
			{
				if ( wtoptype == DIR_WIND )
					dir_add_dwindow(toppath);
				else if (wtoptype == TEXT_WIND )
					txt_add_window(NULL, 0, 0, toppath);
				else
					free(toppath);
			}
	
			break;
		}
		case MSELALL:
		{
			if ( wtoptype == DIR_WIND || wtoptype == DESK_WIND )
			{
				if (w != wtop)
					desel_old();
	
				itm_selall(wtop);
			}
			break;
		}
		case MCYCLE:
		{
			xw_cycle();
			break;     
		}
		case MIDSKICN:
		{
			if(n > 0 && xw_type(w) == DESK_WIND)
				dsk_chngicon(n, list, TRUE);
			else 
				dsk_insticon(w, n, list);
		
			break;
		}
		case MIWDICN:
		{
			icnt_settypes();
			break;
		}
		case MLOADOPT:
		{
			load_settings((n == 1 && itm_type(w, list[0]) == ITM_FILE) ? itm_fullname(w, list[0]) : locate(infname, L_LOADCFG));
			break;
		}
	}

	free(list);
}


/********************************************************************
 *																	*
 * Functions for initialisation of the windows modules and loading	*
 * and saving windows.										        *
 *																	*
 ********************************************************************/

/*
 * Create some reasonable random-like default window sizes and positions.
 * Each new window of a type will generally be located a little bit
 * to the right and down. Directory and text windows start from
 * different positions on the screen.
 */

static void wd_defsize(int type)
{
	WINFO
		*wi;

	int 
		i,
		x0, x1,
		y0, y1,
		w1, w3, 
		h1;


	for (i = 0; i < MAXWINDOWS; i++)
	{

		x0 = xd_desk.w / 8;
		x1 = i;
		y1 = i;

		if (type == TEXT_WIND)
		{
			wi = &textwindows[i];
			txt_font = def_font; /* need not be in the loop */	
			y0 = 20;
			y1 += i;
			w1 = 12;
			w3 = i + i % 2;
			h1 = 7;
		}
		else
		{
			wi = &dirwindows[i];
			dir_font = def_font; /* need not be in the loop */
			x0 *= 2;
			x1 += i;
			y0 = 16;
			w1 = 8;
			w3 = x1;
			h1 = 9;
		}

		wi->x = x0 + x1 * xd_fnt_w + xd_desk.x;
		wi->y = y0 + y1 * xd_fnt_h + xd_desk.y;
		wi->w = xd_desk.w * w1 / (xd_fnt_w * 16) + w3; 	/* in char cell units */
		wi->h = xd_desk.h  * h1 / (xd_fnt_h * 16);			/* in char cell units */
	}
}


/* 
 * Determine maximum permitted window work area size, mostly depending on
 * screen area. Size is rounded to multiples of character cell sizes. If 
 * 'themenu' is given, it is assumed that a text window size is set; 
 * otherwise it is for a dir window.
 * Window size is rounded to multiples of cell size (font or icon size).
 */

static void wd_type_sizes(RECT *dest, int w_flags, OBJECT * themenu, int fw, int fh)
{
	RECT work;

	xw_calc(WC_WORK, w_flags, &xd_desk, &work, themenu);

	dest->x = xd_desk.x;
	dest->y = xd_desk.y;
	dest->w = work.w - (work.w % fw);
	dest->h = work.h - (work.h % fh);
}


/*
 * Combine calculation of maximum window work area sizes for text and 
 * directory windows. See also wd_cellsize().
 * Font and icon sizes have to be defined before this routine is called.
 */

void wd_sizes(void)
{
	int dcw, dch;


	wd_cellsize(NULL, &dcw, &dch, FALSE);
	wd_type_sizes(&dmax, DFLAGS, NULL, dcw, dch);
	wd_type_sizes(&tmax, TFLAGS, viewmenu, txt_font.cw, txt_font.ch);

	can_iconify = aes_wfunc & 128; 
}


void wd_default(void)
{
	wd_deselect_all();				/* deselect everything */
	wd_del_all();					/* delete all windows */
	menu_icheck(menu, MSHOWICN, options.mode);	
	menu_icheck(menu, MAARNG, options.aarr);
	wd_set_sort(options.sort);
	wd_set_fields(options.fields);
	wd_defsize(TEXT_WIND); 			/* default window sizes and font size */
	wd_defsize(DIR_WIND);			/* default window sizes and font size */
	wd_sizes();						/* limit window size (min/max) */
}


#if !__USE_MACROS

/*
 * Initialize windows data
 */

void wd_init(void)
{
	wd_default();
}

#endif


/* 
 * Functies voor het veranderen van het window font.
 * Note: this is only for dir windows and text windows 
 * Return TRUE if OK selected.
 */

boolean wd_type_setfont(int button)
{
	int 
		title,		/* index of dialog title string */
		i;			/* counter */

	RECT 
		work;

	WINFO 
		*wd;
 
	XDFONT 
		*the_font;

	TYP_WINDOW 
		*tw;


	if ( button == WOVIEWER )
	{
		the_font = &txt_font;
		wd = &textwindows[0];
		title = DTVFONT;
	}
	else
	{
		the_font = &dir_font;
		wd = &dirwindows[0];
		title = DTDFONT;
		obj_hide(wdfont[WDFEFF]); /* unhidden in fnt_dialog */
	}

	if ( fnt_dialog(title, the_font, FALSE) )
	{
		for (i = 0; i < MAXWINDOWS; i++)
		{
			tw = wd->typ_window;

			if ( wd->used && tw )
			{
				wd_type_nofull((WINDOW *)tw);
				xw_getwork((WINDOW *)tw, &work); 
				calc_rc(tw, &work); 

				/* no need to set sliders here, see wd_drawall */
			}
			wd++;
		}

#if _MORE_AV
		/* Tell av-clients that directory font has changed */

		va_fontreply( VA_FONTCHANGED, 0 );
#endif
		return TRUE;
	}

	return FALSE;
}


/*
 * Note: in the routines below it is assumed that if window
 * type is NOT TXT_WINDOW than it is DIR_WINDOW, because there is no
 * third type used in this context (i.e. DIR_WINDOW not explicitely checked). 
 * If a third window type be required, if-then-else constructs
 * should be replaced by switch(...){...}
 */

/* 
 * Calculate number of lines and columns, total and visible, in a window.
 * 'work' is input parameter denoting the size of the rectangle
 * the calculation should apply to.
 */

void calc_rc(TYP_WINDOW *w, RECT *work)
{
	int cw, ch;


	wd_cellsize(w, &cw, &ch, TRUE);

	/* Note: do not set w->columns here */

	w->rows     = (work->h + ch - 1) / ch;
	w->nrows    =  work->h / ch; 
	w->ncolumns =  work->w / cw;
	w->scolumns =  work->w / xd_fnt_w;

	if (xw_type(w) == DIR_WIND)
	{
		w->xw_work.w = work->w;
		w->xw_work.h = work->h;
		calc_nlines((DIR_WINDOW *)w);
	}
}


/*
 * Aux. function for rounding window position and size;
 * parameter m is the modulus for rounding
 */

int wd_round(int x, int m)
{
	x += m / 2;
	x -= (x % m);
	return x;
}


/*
 * Similar, for rounding x and y position of a window area
 * Note: rounding of position is problematic in y direction 
 * because of inconvenient menu and screen heights, and becomes effective
 * only about 5 text lines from the top.
 * In earlier versions of the code, screen_info.fntw and .fnt_h were used
 * instead of fixed modulus of 8 pixels, but this caused very jumpy
 * real-time movement of windows in Magic and XaAES.
 */

void wd_xyround(RECT *r)
{
	r->x = wd_round(r->x, 8); 
	r->y = wd_round(r->y, (r->y > 4 * xd_fnt_h) ? 8 : 2);
}


/* 
 * Funktie die uit opgegeven grootte de werkelijke grootte van het
 * window berekent.
 * If iswork == TRUE, input->w and input->h are assumed to be equal to 
 * work area (but -not- input->x and input->y).
 * Size and position of a window may be slightly modified in order to
 * always make it multiples of system character size; otherwise there may be
 * problems with patterns.
 * This routine also limits window size so that it does not
 * exceed the screen, or does not become too small.
 */

void wd_wsize(TYP_WINDOW *w, RECT *input, RECT *output, boolean iswork)
{
#if _MINT_

	if(wd_isiconified(w))
	{
		/*
		 * Things are simpler for an iconified window. It can not be resized
		 * and so all parameters remain as they were when the window was
		 * made iconified. Just the position is rounded to multiples of 8
		 */

		*output = *input;
		wd_xyround(output);

		output->w = icwsize.w;
		output->h = icwsize.h;
	}
	else

#endif

	{
		/* Normal window size */

		RECT 
			work, 
			*dtmax = &dmax;		/* maximum window work area size */

		int 
			fw,					/* window unit cell width */ 
			fh,					/* window unit cell height */ 
			mw,					/* minimum window size */
			mh,					/* minimum window size */
			d = DELTA,			/* increment of line distance */ 
			wflags = DFLAGS;

		OBJECT 
			*menu = NULL;

		if ( xw_type((WINDOW *)w) == TEXT_WIND )
		{
			menu = viewmenu;
			d = 0;
			wflags = TFLAGS;
			dtmax = &tmax;
		}

		/* Calculate work area from overall size */

		xw_calc(WC_WORK, wflags, input, &work, menu);

		/* Note: this is relevant only if the window is fulled */

		if (iswork)
		{
			work.w = input->w;
			work.h = input->h;
		}

		/* 
		 * Round-up position and dimensions of the work area.
		 * Note: rounding of position is problematic in y direction 
		 * because of inconvenient menu and screen heights 
		 */

		wd_cellsize(w, &fw, &fh, FALSE); /* false ? */

		mw = (WMINW * xd_fnt_w) / fw + 1; 
		mh = (WMINH * xd_fnt_h) / fh + 1;

		work.w = wd_round(work.w, fw);
		work.h = wd_round(work.h, fh) + d;	

		/* 
		 * Attempt some rounding of y position when possible...
		 * (when not close to the menu bar)
		 */

		wd_xyround(&work);

		/* There are limits on permitted window size */

		wd_type_sizes(dtmax, wflags, w->xw_menu, fw, fh);

		work.w = minmax(mw * fw, work.w, dtmax->w);
		work.h = minmax(mh * fh, work.h, dtmax->h);

		/* Recalculate overall size from modified work size */

		xw_calc(WC_BORDER, wflags, &work, output, menu);
		calc_rc((TYP_WINDOW *)w, &work); /* number of lines and columns */
	}
}


/* 
 * Calculate window size for directory or text windows.
 * Special considerations when the window is fulled.
 */

void wd_calcsize(WINFO *w, RECT *size)
{
	TYP_WINDOW
		*tw = (TYP_WINDOW *)(w->typ_window);

/* it seems that this is not needed
	OBJECT 
		*menu = NULL;
*/

	RECT 
		*dtmax = &dmax,
		def = xd_desk, 
		border;

	int 
		wflags = DFLAGS,
		wtype= xw_type((WINDOW *)(tw));

	int 
		ch,
		cw;

	boolean
		iswork = FALSE;


	if ( wtype == TEXT_WIND )
	{
		dtmax = &tmax;
/* not needed ?
		menu = tw->xw_menu;
*/
		wflags = TFLAGS;
	}

	/* Find position of the window on screen */

	def.x = w->x + xd_desk.x;
	def.y = w->y + xd_desk.y;

	/* If the window is fulled, calculate its new size, etc. */

	if (w->flags.fulled == 1)
	{
		/* Adjust size of a fulled window- no more than needed */

		long ll = tw->nlines;

		iswork = TRUE;

		/* 
		 * Calculation will depend on whether this is a shift-full,
		 * and also on whether auto-arrange is set
		 */

		wd_sizes();
		wd_cellsize(tw, &cw, &ch, TRUE); /* false ??? */

		if (w->flags.fullfull)
		{
			/* Full to the whole screen */
/* not exactly
			def.w =xd_screen.w - (tw->xw_size.w - tw->xw_work.w);
			def.h = xd_screen.h - (tw->xw_size.h - tw->xw_work.h);
*/
			def.w = dtmax->w;
			def.h = dtmax->h;
		}
		else
		{
			/* Full only as much as needed */

			def.w = tw->columns * cw;

			/* 
			 * Add some empty lines to fulled directory directory windows
			 * This will make draging of items into windows more comfortable. 
			 * Add one line in text mode, or one row in icons mode.
			 */

			if (wtype == DIR_WIND)
			{
				ll += 1;	

				if (options.mode != TEXTMODE)
					def.w += XOFFSET;
			}

			/* Window overall size must not exceed max integer or so */

			def.h = (int)lmin(32700, ll * ch);
		}

		wd_wsize(tw, &def, size, iswork);

		/* Limit window positon to screen size (excluding menu), or slightly smaller */

		size->x = max(0, min(size->x, xd_desk.w - size->w - 4)); 
		size->y = max(xd_desk.y, min(size->y, xd_desk.h - size->h));
	}
	else
	{
		/* 
		 * Window is not fulled;
		 * Window work area size in WINFO is in screen-font character units; 
		 * therefore a conversion is needed here (def = window work area);
		 */

		def.w = w->w * xd_fnt_w;	/* hoogte en breedte van het werkgebied. */
		def.h = w->h * xd_fnt_h;

		/* 
		 * Calculate window width and height. As 'def' above corresponds
		 * to w->xw_work, window menu height should be ignored 
		 * in xw_calc(), therefore NULL parameter below
		 */

		xw_calc(WC_BORDER, wflags, &def, &border, NULL);

		border.x = def.x;
		border.y = def.y;

		wd_wsize(tw, &border, size, FALSE); 
	}
}


/* 
 * Merge of redraw functions for text and dir windows 
 */

void wd_type_redraw(WINDOW *w, RECT *r1)
{
	RECT 
		r2, 
		in, 			/* intersection rectangle */
		work;			/* window work area */

	long
		rows = ((TYP_WINDOW *)w)->rows,
		py = ((TYP_WINDOW *)w)->py,
		i;

	OBJECT 
		*obj = NULL;	/* window root object in icon mode */

	XDFONT
		*thefont = &txt_font;


	if (!clip_desk(r1)) 
		return;

	xw_getwork(w, &work); /* note: work may not be equal to w->xw_work */

	if ( wd_isiconified((TYP_WINDOW *)w) )
		icw_draw( w );
	else
	{
		if ( xw_type(w) == DIR_WIND )
		{
			if (options.mode != TEXTMODE)
			{
				int nc = ((DIR_WINDOW *)w)->ncolumns;

				if ( work.w % ICON_W != XOFFSET)
					nc++;

				if ((obj = make_tree
				(
					(DIR_WINDOW *)w,
					((DIR_WINDOW *)w)->px,
					nc,
					(int)py, 
					rows, 
					FALSE, 
					&work
				)) == NULL) 
					return;
			}

			thefont = &dir_font;
		}

		clearline = FALSE; /* don't redraw window background twice */

		xd_begupdate();
		xd_mouse_off();
		set_txt_default(thefont);

		xw_getfirst(w, &r2);

		while (r2.w != 0 && r2.h != 0)
		{
			if (xd_rcintersect(r1, &r2, &in))
			{
				xd_clip_on(&in);

				if ( xw_type(w) == TEXT_WIND )
				{
					for (i = 0; i < rows; i++)
						txt_prtline((TXT_WINDOW *)w, py + i, &in, &work);
				}
				else
				{					
					if (options.mode == TEXTMODE)
					{
						pclear(&in);

						for(i = 0; i < rows; i++ )
							dir_prtcolumns((DIR_WINDOW *)w, py + i, &in, &work);
					}
					else
					{
						/* dir_prtcolumns() can draw icons, but this is faster here */

						draw_tree(obj, &in);
					}
				}

				xd_clip_off();
			}

			xw_getnext(w, &r2);
		}

		clearline = TRUE;

		xd_mouse_on();
		xd_endupdate();
		free(obj);
	}	
}


/* 
 * Either redraw a window or send a message to AES to redraw it 
 */

void wd_type_draw(TYP_WINDOW *w, boolean message) 
{
	RECT area;

	xw_getsize((WINDOW *)w, &area);

	if (message)
		xw_send_redraw((WINDOW *)w, WM_REDRAW, &area);
	else
		wd_type_redraw((WINDOW *)w, &area);
}


/* 
 * Draw a window but set sliders first
 */

void wd_type_sldraw(WINDOW *w)
{
	if(wd_dirortext(w))
	{
		RECT size;

		wd_calcsize(((TYP_WINDOW *)w)->winfo, &size);

		if(size.w != w->xw_size.w || size.h != w->xw_size.h)
			xw_setsize(w, &size); 

		set_sliders((TYP_WINDOW *)w);
	}

	wd_type_draw((TYP_WINDOW *)w, TRUE);
}


/*
 * Redraw all windows
 */

void wd_drawall(void)
{
	WINDOW *w = xw_first();

	while (w)
	{
		/* 
		 * Note: windows below dialogs do not get redrawn nicely
		 * if wd_type_draw() is called with message=FALSE
		 * (as in earlier versions of this file)
		 */

		wd_type_sldraw(w);
		w = xw_next(w);
	}
}


/* 
 * Window topped handler for dir and text  windows 
 */

void wd_type_topped (WINDOW *w)
{
	autoloc_off();
	xw_set(w, WF_TOP);
}


void wd_type_bottomed(WINDOW *w)
{
	autoloc_off();
	xw_set(w, WF_BOTTOM);
}


/*
 * Set window title. For the sake of size optimization, 
 * also set window sliders, because in previous versions that routine was always 
 * (except in one place once) called immediately after setting the title.
 */

void wd_type_title(TYP_WINDOW *w)
{
	int 
		d = 3,		/* Required width increment, single-TOS value */ 
		columns;

	char 
		*fulltitle = NULL;

#if _MINT_

	/* Don't do anything in an iconified window */

	if(wd_isiconified(w))
		return;

	/* 
	 * aes_hor3d is usually either 0 or 2 pixels;
	 * line below increases d by about 1 character 
	 * if there is nonzero aes_hor3d
	 */

	if(xd_aes4_0)
		d = 5 + aes_hor3d / 2;
#endif

	/* Calculate available title width (in characters) */

	columns = min( w->scolumns - d, (int)sizeof(w->title) );

	if(columns > 0)
	{
		if(xw_type(w) == TEXT_WIND)
			fulltitle = strdup(((TXT_WINDOW *)w)->path);
		else
			fulltitle = fn_make_path(((DIR_WINDOW *)w)->path, ((DIR_WINDOW *)w)->fspec);	
	}

	/* Set window */

	if(fulltitle)
	{
		cramped_name(fulltitle, w->title, (size_t)columns);
		free(fulltitle);
		xw_set((WINDOW *)w, WF_NAME, w->title);
	}
	
	set_sliders(w);
}


/*
 * Window fuller handler.
 * Two fulled states are recognized: normal and fullscreen.
 * The second one is obtained by pressing the shift key while
 * clicking on the fuller window gadget.
 * Subsequent clicks on the fuller gadget will toggle the window
 * between the fulled and the normal state.
 */

void wd_type_fulled(WINDOW *w, int mbshift)
{
	WINFO 
		*winfo = ((TYP_WINDOW *)w)->winfo; 

#if _MINT_

	int
		ox = ((TYP_WINDOW *)w)->px,
		oc = ((TYP_WINDOW *)w)->dcolumns;

	long
		oy = ((TYP_WINDOW *)w)->py;
#endif

	boolean
		f = (( mbshift & ( K_RSHIFT | K_LSHIFT ) ) != 0 );


	if ( (winfo->flags.fulled == 0) || (f && (winfo->flags.fullfull == 0)) )
	{
		winfo->flags.fulled = 1;

		if (f)
			winfo->flags.fullfull = 1;
	}
	else
		wd_setnormal(winfo); /* this resets the iconified window too but it doesn't matter */

	/* Change window size depending on flags state */

	wd_adapt(w);

	/* Set window title (and sliders), depending on window size */

	wd_type_title((TYP_WINDOW *)w);

	/* Attempt to handle stupid smart redraws */

#if _MINT_
	if
	(
		mint && 
		!geneva &&
		winfo->flags.fulled &&
		(
			ox != ((TYP_WINDOW *)w)->px || 
			oy != ((TYP_WINDOW *)w)->py || 
			oc != ((TYP_WINDOW *)w)->dcolumns
		)
	)
		wd_type_redraw(w, &(w->xw_size));
#endif

}


/*
 * Reset a fulled directory or text window to nonfulled state
 * without changing its size.
 */

void wd_type_nofull(WINDOW *w)
{
	if(wd_dirortext(w))
	{
		WINFO *wi;

 		wi = ((TYP_WINDOW *)w)->winfo;
		wi->flags.fulled = 0;
		wi->flags.fullfull = 0;
		wd_set_defsize(wi);
	}
}


/*
 * Handle scrolling window gadgets 
 */

void wd_type_arrowed(WINDOW *w, int arrows)
{
	switch (arrows)
	{
		case WA_UPLINE:
		case WA_DNLINE:
		case WA_LFLINE:
		case WA_RTLINE:
		{
			w_scroll((TYP_WINDOW *)w, arrows);
			break;
		}
		case WA_UPPAGE:
		{
			w_pageup((TYP_WINDOW *)w); 
			break;
		}
		case WA_DNPAGE:
		{
			w_pagedown((TYP_WINDOW *)w); 
			break;
		}
		case WA_LFPAGE:
		{
			w_pageleft((TYP_WINDOW *)w); 
			break;
		}
		case WA_RTPAGE:
		{
			w_pageright((TYP_WINDOW *)w); 
			break;
		}
	}
}


void wd_type_hslider(WINDOW *w, int newpos)
{
	long h = (long)( ((TYP_WINDOW *)w)->columns - ((TYP_WINDOW *)w)->ncolumns);
	w_page((TYP_WINDOW *)w, (int)calc_slpos(newpos, h), ((TYP_WINDOW *)w)->py);
}


void wd_type_vslider(WINDOW *w, int newpos)
{
	long h = (long) (((TYP_WINDOW *)w)->nlines - ((TYP_WINDOW *)w)->nrows);
	w_page((TYP_WINDOW *)w, ((TYP_WINDOW *)w)->px, calc_slpos(newpos, h));
}


/*
 * Calculate window size to be kept in the WINFO structure.
 * Position is in pixel units but size is in character-cell units! 
 * (for iconified windows size is in character-cell-width units, because
 * some AESes create iconified windows in sizes that are not multiples
 * of character height, so a smaller unit is needed there)
 */

void wd_set_defsize(WINFO *w) 
{
	WINDOW *ww = (WINDOW *)w->typ_window;

	w->x = ww->xw_size.x - xd_desk.x;
	w->y = ww->xw_size.y - xd_desk.y;

	if(w->flags.iconified)
	{
		w->w = ww->xw_size.w;
		w->h = ww->xw_size.h / xd_fnt_w; /* beware: fnt_w, not fnt_h ! */
	}
	else
	{
		w->w = ww->xw_work.w;
		w->h = ww->xw_work.h / xd_fnt_h;
	}

	w->w /= xd_fnt_w;
}


/*
 * If overscan hardware hack exists and it is turned OFF there is a problem
 * with redrawing of a moved window, if initial position of the window
 * was such that the lower portion of the window was off the screen.
 * It seems that any attempt, message etc. to redraw the window
 * does not do that completely (the "sizer" widget is not redrawn)
 * unless its size is actually changed.
 * So, a brute hack was made here to amend this:
 * Window size is briefly increased by one pixel below, then returned
 * to normal. "Overscan off" state is irregular for many programs anyway,
 * and this would probably be used only rarely.
 */

void wd_forcesize(WINDOW *w, RECT *size, boolean cond)
{
	RECT s = *size;

	if ( cond )
	{
		s.w += 1;	
		s.h += 1;
		xw_setsize(w, &s);
		s.w -= 1;
		s.h -= 1;
	}

	xw_setsize(w, &s); 	/* set new size in pixels */
}


/* 
 * Handle moving of a window.
 * "fulled" flag is reset if a window is moved
 */

void wd_type_moved(WINDOW *w, RECT *newpos)
{
	WINFO
		*wd = ((TYP_WINDOW *) w)->winfo; 

	RECT
		size;

	/* Permit size/position of window only as multiples of character size */

	wd_wsize((TYP_WINDOW *)w, newpos, &size, FALSE);

	wd->flags.fulled = 0;
	wd->flags.fullfull = 0;

#if _OVSCAN
	/* see function wd_forcesize() above */
	wd_forcesize(w, &size, over != -1L && ovrstat == 0);
#else
	wd_forcesize(w, &size, FALSE);
#endif
	wd_set_defsize(wd);		/* calculate new size for WINFO */
}


/*
 * Handle resizing of a window. In some cases it may be necessary to
 * redraw the window here; otherwise, redraw message is received and 
 * window redrawn elsewhere
 */

void wd_type_sized(WINDOW *w, RECT *newsize)
{
#if _MINT_
	long
		oy;				/* old vertical slider position */
#endif

	RECT 
		oldsize;		/* old window size */

	int 
#if _MINT_
		ox,				/* old horizontal slider position */
#endif
		dc = 0,			/* number of directory columns */
		oc = 0;			/* old number of directory columns */

	boolean
		draw = FALSE;	/* true when window has to be redrawn */


	/* Save size, slider positions and number of columns before resizing */

	oldsize = w->xw_size;
#if _MINT_
	ox = ((TYP_WINDOW *)w)->px;
	oy = ((TYP_WINDOW *)w)->py;
#endif
	oc = ((TYP_WINDOW *)w)->dcolumns;

	wd_type_moved(w, newsize);

	wd_type_title((TYP_WINDOW *)w); /* sets window title AND sliders */

	if ( xw_type(w) == DIR_WIND )
	{
		if(((DIR_WINDOW *)w)->par_itm > 0) /* selected item in parent dir. */
			((DIR_WINDOW *)w)->par_itm = -((DIR_WINDOW *)w)->par_itm;

		dc = ((DIR_WINDOW *)w)->dcolumns; /* new number of columns */

		if
		(
			dc != oc &&	/* number of item columns has changed */ 
			w->xw_size.x > 0 && /* this helps, but why ? */
			w->xw_size.w < oldsize.w &&
			w->xw_size.h <= oldsize.h
		)
			draw = TRUE;
	}

	/* 
	 * This should fix some redraw problems with real-time resizing.
	 * However, real-time resizing can not be detected in XaAES (this appears
	 * to be a bug in XaAES?) and the inquiry is therefore somewhat simplified- 
	 * but this may result in double redraws. It is now assumed that realtime 
	 * resizing exists only in those AESes that need mint. 
	 * Later: this is an unsolved problem related to wheter an AES employs
	 * a "smart redraw" mechanism- i.e. whether it draws the complete window
	 * or just the new portions. wd_type_draw() below is needed only if
	 * "smart_redraw()" is on. Currently this affects only XaAES and Magic
	 */

#if _MINT_
	if
	(
		(((TYP_WINDOW *)w)->px != ox || (((TYP_WINDOW *)w)->py != oy) || dc != oc) && 
		(w->xw_size.w > oldsize.w || w->xw_size.h > oldsize.h) &&
		mint && !geneva /* but this may cause double redraws sometimes */
	)
		draw = TRUE; 
#endif

	if(draw)
		wd_type_redraw(w, &(w->xw_size));
}


/********************************************************************
 *																	*
 * Funkties voor het zetten van de window sliders.					*
 *																	*
 ********************************************************************/


/* 
 * Calculate size and position of a window slider
 * (later beware of int/long difference in hor/vert applicaton)
 */

void calc_wslider
(
	long wlines, 	/* in-     w->ncolumns or w->nrows; visible width */
	long nlines,	/* in-     w->columns or w->nlines; total width */
	long *wpxy,		/* in/out- w->px or w->py; current position */
	int *p,			/* out-    calculated slider size */
	int *pos		/* out-    calculated slider position */
)
{
	long h = nlines - wlines;

	*wpxy = lminmax(0, h, *wpxy);

	if ( h > 0 )
	{
		*p = calc_slmill(wlines, nlines);
		*pos = calc_slmill(*wpxy, h);
	}
	else
	{
		*p = 1000;
		*pos = 0;
	}
}


/*
 * Calculate and set size and position of horizontal window slider;
 * note: this is somewhat slower than may be necessary, 
 * because both size and position are always set;
 */

void set_hslsize_pos(TYP_WINDOW *w)
{
	int 
		p,		/* calculated slider size     */ 
		pos;	/* calculated slider position */

	long 
		ww = w->columns,
		wpx = (long)w->px;

	calc_wslider( (long)w->ncolumns, ww, &wpx, &p, &pos );

	w->px = (int)wpx;

	/* both size and position of the slider are always set; */

	xw_set((WINDOW *) w, WF_HSLSIZE, p);
	xw_set((WINDOW *) w, WF_HSLIDE, pos);
}


/*
 * Calculate and set size and position of vertical window slider.
 * Note that some variables are long here, different 
 * than in set_hslsize_pos()
 */

void set_vslsize_pos(TYP_WINDOW *w)
{
	int
		p,		/* slider size     */ 
		pos;	/* slider position */

	calc_wslider( w->nrows, w->nlines, &(w->py), &p, &pos );

 	xw_set((WINDOW *)w, WF_VSLSIZE, p);
	xw_set((WINDOW *)w, WF_VSLIDE, pos);
}


/*
 * Set both window sliders (sizes and positions)
 */

void set_sliders(TYP_WINDOW *w)
{
	set_hslsize_pos(w);
	set_vslsize_pos(w);
}


/*
 * Page window. If autolocator is turned on, enable
 * action even if positions have not changed.
 */

void w_page(TYP_WINDOW *w, int newpx, long newpy)
{
	boolean doit = FALSE;

	if ( newpx != w->px )
	{
		w->px = newpx;
		set_hslsize_pos((TYP_WINDOW *)w); 
		doit = TRUE;
	}

	if ( newpy != w->py )
	{
		w->py = newpy;
		set_vslsize_pos((TYP_WINDOW *)w);
		doit = TRUE;
	} 

	if (autoloc || doit)
		wd_type_draw(w, FALSE);
}


/*
 * Window page scrolling routines
 */

void w_pageup(TYP_WINDOW *w)
{
	long newy = w->py - w->nrows;

	if (newy < 0)
		newy = 0;

	w_page(w, w->px, newy);
}


void w_pagedown(TYP_WINDOW *w)
{
	long newy = w->py + w->nrows;						

	if ((newy + w->nrows) > w->nlines)	
		newy = lmax(w->nlines - (long)w->nrows, 0);

	w_page (w, w->px, newy);
}


void w_pageleft(TYP_WINDOW *w)
{
	int newx = w->px - w->ncolumns;

	if (newx < 0)
		newx = 0;

	w_page(w, newx, w->py);
}


void w_pageright(TYP_WINDOW *w)
{
	int 
		nc = w->ncolumns,
		lwidth = w->columns,
		newx;

	newx = w->px + nc;

	if ((newx + nc) > lwidth)
		newx = max(lwidth - nc, 0);

	w_page ( w, newx, w->py);
}


/*
 * More window scrolling routines ;
 * for text window: ch= txt_font.ch;
 * for dir window: ch is either dir_font.ch + DELTA or ICON_H
 */

/*
 * Zoek de eerste regel van een window, die zichtbaar is binnen
 * een rechthoek.
 *
 * Parameters:
 *
 * wx,wy	- x,y coordinaat werkgebied window
 * area		- rechthoek
 * prev		- pointer naar een boolean, die aangeeft of een of twee
 *			  regels opnieuw getekend moeten worden.
 *
 * ch  		- char height (or char height + DELTA or icon height)
 * cw		- char width
 * 
 */


/*
 * Find the first or the last column or line in a window
 * If last=1 find the last one; if last=0 find the first one.
 */

static long find_firstlast(int wy, int ay, int ah, boolean *prev, int ch, int last)
{
	long line;

	line = ay - wy;

	*prev = FALSE;

	if(last)
	{
		line += ah;

		if((line % ch) != 0)
			*prev = TRUE;

		line--;
	}

	return (long)(line / ch);
}


/*
 * Functie om een text/dir window een regel te scrollen.
 *
 * Parameters:
 *
 * w		- Pointer naar window
 * type		- geeft richting van scrollen aan
 */

void w_scroll(TYP_WINDOW *w, int type) 
{
	RECT 
		work, 
		r, 
		in, 
		src, 	/* screen area bein copied from */
		dest;	/* screen area being copied to */

	long 
		line;

	int 
		last,
		dl = -1,
		column,
		nc,
		wx, 
		wy, 
		cw,
		ch,
		wtype = xw_type((WINDOW *)w);

	boolean 
		vert = FALSE,
		prev; 


	/* Change position in the window, i.e. w->px, w->py */

	switch (type)
	{
		case WA_UPLINE:
		{
			if (w->py <= 0)
				return;
			w->py -= 1;
			dl = 1;
			vert = TRUE;
			break;
		}
		case WA_DNLINE:
		{
			if ((w->py + w->nrows) >= w->nlines)
				return;
			w->py += 1;
			vert = TRUE;
			break;
		}
		case WA_LFLINE:
		{
			if ((w->px <= 0))
				return;
			w->px -= 1;
			dl = 0;
			break;
		}
		case WA_RTLINE:
		{
			if ((w->px + w->ncolumns) >= w->columns) 
				return;
			w->px += 1;
			break;
		}
		default:
		{
			return;
		}
	}

	xw_getwork((WINDOW *) w, &work);

	/* Is this in the visible screen at all ? */

	if(!clip_desk(&work))
		return;

	/* Determine increments for scrolling- in char cell or icon cell units */

	wd_cellsize(w, &cw, &ch, TRUE);

	wx = work.x;
	wy = work.y;

	/* Start updating... */

	xd_begupdate();

	/*  Calculate and set window slider(s) size(s) and position(s) */

	if (vert)
		set_vslsize_pos(w);
	else
		set_hslsize_pos(w);

	xd_mouse_off();

	set_txt_default(( wtype == TEXT_WIND ) ? &txt_font : &dir_font);

	/* Traverse all rectangles */

	xw_getfirst((WINDOW *) w, &r);

	while (r.w != 0 && r.h != 0)
	{
		if (xd_rcintersect(&r, &work, &in)) 
		{
			xd_clip_on(&in);

			src = in;
			dest = in;

			if (vert)
			{
				if (type == WA_UPLINE)
				{
					dest.y += ch;
					last = 0;
				}
				else
				{
					src.y += ch;
					last = 1;
				}

				line = find_firstlast(wy, in.y, in.h, &prev, ch, last);					

				line += w->py;
				dest.h -= ch;
				src.h -= ch;
			}
			else
			{
				nc = 1;

				if (type == WA_LFLINE)
				{
					dest.x += cw;
					last = 0;
				}
				else
				{
					src.x += cw;
					last = 1;
				}

				column = (int)find_firstlast(wx, in.x, in.w, &prev, cw, last);

				column += w->px;
				dest.w -=cw;
				src.w -=cw;

				if ( prev )
					nc++;
			}

			/* 
			 * Part of the window which is not scrolled out
			 * is moved by the right amount in the appropriate direction
			 */

			if ((src.h > 0) && (src.w > 0))
				move_screen(&dest, &src);

			if (vert)	/* up/down */
			{
				again:;

				if ( wtype == TEXT_WIND )
					txt_prtline((TXT_WINDOW *)w, line, &in, &work);
				else
					dir_prtcolumns((DIR_WINDOW *)w, line, &in, &work);

				if (prev)
				{
					line += dl;
					prev = FALSE;
					goto again;
				}
			}
			else	/* left/right */
			{
				if ( prev )	
					column += dl;

				if ( wtype == TEXT_WIND )
					txt_prtcolumn((TXT_WINDOW *)w, column, nc, &in, &work);
				else
					dir_prtcolumn((DIR_WINDOW *)w, column, nc, &in, &work);
			}

			xd_clip_off();
		}

		xw_getnext((WINDOW *)w, &r);
	}

	xd_mouse_on();
	xd_endupdate();
}


/*
 * Adapt window size, e.g. when fulled or with resolution change.
 * Return TRUE if adaptation performed (correct window type)
 * if returned TRUE, routine set_sliders() should be called next.
 */

boolean wd_adapt(WINDOW *w)
{
	RECT size;

	if(wd_dirortext(w))
	{
		wd_calcsize(((TYP_WINDOW *) w)->winfo, &size);
		xw_setsize(w, &size);

		return TRUE;
	}
	
	return FALSE;
}


/* 
 * Keyboard event handler for text & dir windows.
 *
 * Parameters:
 *
 * w		- Pointer naar window
 * scancode	- Scancode van de ingedrukte toets
 * keystate	- Toestand van de SHIFT, CONTROL en ALTERNATE toets
 *
 * Resultaat: 0 - toets kon niet verwerkt worden
 *			  1 - toets kon verwerkt worden
 */

int wd_type_hndlkey(WINDOW *w, int scancode, int keystate)
{
	int 
		act,				/* action code */
		key,				/* code of pressed key */
		wt = xw_type(w),	/* window type */
		result = 1;			/* return value */

	TYP_WINDOW 
		*tyw = (TYP_WINDOW *)w;

	if(!onekey_shorts)
		autoloc = TRUE; /* autolocator is always ON if there are no single-key kbd shortcuts */

	switch (scancode)
	{
		case RETURN:		/* if autoselector is on, open the selected item(s) */
		{
			if ( wt != TEXT_WIND )
			{	
				int n, *list;
	
				if(autoloc && (list = itm_list(w, &n)) != NULL /* no need ? && n */ )
				{
					free(list); /* list is needed just to see if anything selected */
					wd_hndlmenu(MOPEN, keystate);
				}
				else 
					result = 0;
	
				break;
			}

			/* if not in text window, proceed as if CURDOWN */
		}
		case CURDOWN:			/* scroll down one line */
		{
			act = WA_DNLINE;
			goto scroll;
		}
		case CURUP:				/* scroll up one line */
		{
			act = WA_UPLINE;
			goto scroll;
		}
		case CURLEFT:			/* scroll left one column */
		{
			act = WA_LFLINE;	
			goto scroll;
		}
		case CURRIGHT:			/* scroll right one column */
		{
			act = WA_RTLINE;
			scroll:;
			w_scroll(tyw, act);
			break;
		}
		case SPACE:				/* scroll down one page except when autolocating */
		{
			if ( wt == DIR_WIND )
			{
				if(autoloc)
					goto thedefault;
	
				result = 0;
				break;
			}
		}
		case PAGE_DOWN:				/* PgUp/PgDn keys on PC keyboards (Emulators and MILAN) */
		case SHFT_CURDOWN:
		{
			w_pagedown(tyw);
			break;
		}
		case PAGE_UP:				/* PgUp/PgDn keys on PC keyboards (Emulators and MILAN) */
		case SHFT_CURUP:
		{
			w_pageup(tyw);
			break;
		}
		case SHFT_CURLEFT:
		{
			w_pageleft(tyw);
			break;
		}
		case SHFT_CURRIGHT:
		{
			w_pageright(tyw);
			break;
		}
		case HOME:					/* Reset sliders to window top */
		{
			w_page (tyw, 0, 0L); 
			break;
		}
		case SHFT_HOME:				/* Set sliders to window bottom */
		{
			if ( tyw->nrows < tyw->nlines )
				w_page (tyw, 0, (long)(tyw->nlines - tyw->nrows)); 
		
			break;
		}
		case ESCAPE:				/* refresh a window */
		{
			if ( wt == DIR_WIND )
			{
				/* refresh a directory window */
	
				force_mediach(((DIR_WINDOW *) w)->path);
				dir_refresh_wd((DIR_WINDOW *)w);
			}
			else
			{
				/* re-read the file in a text window */
	
				txt_reread((TXT_WINDOW *)w, NULL, tyw->px, tyw->py);
			}
			break;
		}
		case UNDO:
		case INSERT:	/* Toggle autoselector on/off */
		{
			if( wt != DIR_WIND )
				break;
			if(autoloc || (unsigned int)scancode == UNDO)
				autoloc_off();
			else
			{
				autoloc = TRUE;
				aml = 0;
				*automask = 0;
			}
		}
		default:
		{
			thedefault:;
			{
				if ( wt == TEXT_WIND )
					result = 0;
				else 
				{
					/* Handle autoselector and opening of directory windows */
	
					key = scancode & ~XD_SHIFT;
	
					if(!((scancode & XD_SHIFT) && dir_onalt(key, w)))
					{
						/* Create name mask for the autoselector */
	
						if((((TYP_WINDOW *)w)->winfo)->flags.iconified != 0)
							autoloc_off();
	
						/* 
						 * Name mask for the autoselector has to be composed,
						 * taking into account limitations of the filesystem;
						 * last character entered can be deleted by [Backspace]
						 */
	
						if(autoloc)
						{
							size_t lm = x_pathconf(((DIR_WINDOW *)w)->path, DP_NAMEMAX);
	
							if(lm < 0)
								lm = 12; /* override error (?) in x_pathconf */
							else
								lm = lmin(lm, (long)sizeof(LNAME) - 1);
	
							if 
							(
								key == BACKSPC || 					/* backspace ?     */
								(unsigned int)key == INSERT || 		/* insert ?        */
								(key >= ' ' && key <= '~') ||		/* ASCII printable */
								((key & 0xFF) > 127)				/* codes above 127 */
							) 
							{
								/* key is [Backspace] or [Insert] or a printable character */
	
								char *ast, *dot;  	/* pointer to "." in the name */
								int ei = 0; 		/*  index of "*"  */
	
								/* [Insert] and wildcard should have no effect */
	
								if((unsigned int)key == INSERT || (unsigned int)key == UNDO)
									key = 0;
	
								/* If [Backspace], delete last character */
	
								if(key ==  BACKSPC)
								{
									key = 0;
	
									if(aml)
										aml--;
	
									if(aml && automask[aml - 1] == '*')
										aml--;
								}
							
								automask[aml] = 0;
	
								/* Are long names possible or is this a 8+3 FAT filesystem? */
#if _MINT_
								if((x_inq_xfs(((DIR_WINDOW *)w)->path) & FS_LFN) == 0)
#endif
								{
									/* 
									 * Compose a name mask obeying the 8+3 rule 
								 	 * (append either "*.*" or ".*" or "*")
								 	 */
	
									if(key == SPACE)
										key = 0;
	
									key = touppc(key);
									dot = strchr(automask,'.');
	
									if(dot)
									{
										lm = dot - automask + 4;
	
										if(key == '.')
											key = 0;
									}
									else if (key != '.')
									{ 
#if _MINT_
										if(!mint)
#endif
											ei = 10;	/* Index of ".*" */
	
										if(!((aml == 7) && key))
										{
											if(aml == 8)
												key = 0;
											else
											{
#if _MINT_ 
												if(!mint) 
#endif
													ei = 1;	/* index of *.*  */
											}
										}
									}							
								} /* long names possible ? */
	
								/* This should prevent too long namemasks */
	
								if(key && (aml == lm - 1) )
									ei = 11;				/* index of "\0" */
		
								if(aml == lm)
								{
									key = 0;
									ei = 11;	/* index of "\0" */
								}
							
								/* Append the character and wildcards to namemask */
	
								if(key)
									automask[aml++] = key;
	
								automask[aml] = 0;
								strcat(automask, presets[ei]);
	
								/* Consecutive wildcards are not needed */
	
								if((ast = strstr(automask, "**")) != NULL)
									strcpy(ast, ast + 1);
	
								/* Select items in the window */
	
								dir_autoselect((DIR_WINDOW *)w);
							}
							else
								result = 0;
						}
						else
							result = 0;
	
					} /* scancode, dir_onalt(); */
				}		
			}
		
		break;
		}
	}

	return result;
}


CfgNest 
	dir_config, 	/* configuration table for directory windows */
	view_config,	/* Configuration table for text windows */ 
	open_config,	/* configuration table for open windows */
	dir_one,		/* configuration table for one dir window */
	text_one;		/* configuration table for one text window */


/*
 * Configuration table for all windows
 */

CfgEntry window_table[]= 
{
	{CFG_HDR,  "windows" },
	{CFG_BEG},
	{CFG_NEST, "directories",	dir_config	},		/* directory windows */
	{CFG_NEST, "views",			view_config	},		/* text windows */
	{CFG_NEST, "open",			open_config	},		/* open windows (any type */
	{CFG_ENDG},
	{CFG_LAST}
};


/*
 * Load or save all windows information
 */

CfgNest wd_config
{
	*error = handle_cfg(file, window_table, lvl, CFGEMP, io, wd_init, wd_default);
}


/* 
 * Configuration tables for start/end of group for open windows
 */

static const CfgEntry open_start_table[] =
{
	{CFG_HDR, "open" },
	{CFG_BEG},
	{CFG_LAST}
};

static const CfgEntry open_end_table[] =
{
	{CFG_END},
	{CFG_LAST}
};


/*
 * Read-only configuration table for all open windows
 */

static CfgEntry open_table[] =
{
	{CFG_HDR,  "open" },
	{CFG_BEG},
	{CFG_NEST, "dir",	dir_one  },
	{CFG_NEST, "text",	text_one },
	{CFG_END},
	{CFG_LAST}
};


/*
 * Configuration table for temporary saving and reopening windows 
 */

CfgEntry reopen_table[]=
{
	{CFG_NEST, "open", open_config},	/* open windows (any type) */
	{CFG_FINAL}, 				       	/* file completness check  */
	{CFG_LAST}
};


/* 
 * Save or load configuration of open windows.
 * The construction is needed to preserve window stack order:
 * all windows must be saved intermingled on stack order,
 * hence the single loop.
 * during saving, windows may be closed, depending on "wclose"
 * Loading is straightforward on keyword occurrence.
 */

CfgNest open_config
{
	WINDOW *w;

	w = xw_last();
	*error = 0;

	/* Save open windows data only if there are any */

	if (io == CFG_SAVE)
	{
		if(w && (((options.sexit & SAVE_WIN) != 0) || wclose) )
		{
			*error = CfgSave(file, open_start_table, lvl, CFGEMP);

			while ( (*error >= 0) && w )
			{
				that.w = w;

				switch(xw_type(w))
				{
					case DIR_WIND :
					{
 						dir_one(file, lvl + 1, CFG_SAVE, error); 
						break;
					}
					case TEXT_WIND :
					{
						text_one(file, lvl + 1, CFG_SAVE, error);
						break;
					}
					default:
					{
						break;
					}
				}

				if (wclose)
					wd_type_close(w, 1);

				w = xw_prev(w);
			}

			*error = CfgSave(file, open_end_table, lvl, CFGEMP); 
		}
	}
	else
	{
		/* Load configuration of open windows */

		*error = CfgLoad(file, open_table, MAX_KEYLEN, lvl); 
	}
}


/********************************************************************
 *																	*
 * Functies voor het tijdelijk sluiten en weer openen van windows.	*
 *																	*
 ********************************************************************/

/*
 * Save current positions of the open windows into a (file-like) buffer 
 * (a "memory file") and close the windows. This routine also opens 
 * this "file".
 */

boolean wd_tmpcls(void)
{
	int error;

	/* 
	 * Deselect anything that was selected in windows. 
	 * This will also set selection.w to NULL and selection.n to 0
	 */

	wd_deselect_all();

	/* Attempt to open a 'memory file' (set level-1 here) */

	if ((mem_file = x_fmemopen(O_DENYRW | O_RDWR, &error)) != NULL)
	{
		wclose = TRUE;
		error = CfgSave(mem_file, reopen_table, -1, CFGEMP); /* windows closed here */ 
		wclose = FALSE;
	}

	/* 
	 * Close the 'file' only in case of error, otherwise keep it
	 * open for later rereading
	 */

	if (error < 0) 
	{
		if (mem_file != NULL)
			x_fclose(mem_file);

		xform_error(error);
		return FALSE;
	}

	return TRUE;
}


/*
 * Load the positions of windows from a (file-like) memory buffer 
 * ("the memory file") and open them.  This routine closes the "file"
 * after reading
 */

void wd_reopen(void)
{
	int error = EINVFN;

	chklevel = 0;

	/* Rewind the "file", then read from it (use level -1 here) */

	if(mem_file)
	{
		mem_file->read = 0;
		error = CfgLoad(mem_file, reopen_table, MAX_KEYLEN, -1); 

		x_fclose(mem_file);
	}

	xform_error(error); /* will ignore error >= 0 */
}


/********************************************************************
 *																	*
 * Functions for handling items in a window.						*
 *																	*
 ********************************************************************/

/*
 * Find an object in a window. 
 * Attention: Must be used only on directory and desktop windows
 */

int itm_find(WINDOW *w, int x, int y)
{
	if (in_window(w, x, y))
		return (((ITM_WINDOW *)w)->itm_func->itm_find) (w, x, y);
	else
		return -1;
}


boolean itm_state(WINDOW *w, int item)
{
	return (((ITM_WINDOW *)w)->itm_func->itm_state) (w, item);
}


/*
 * Return type of an item in a window. Must be disabled for text window
 * (because this routine may be called for text windows in itm_set_menu)
 */


ITMTYPE itm_type(WINDOW *w, int item)
{
	if ( xw_type(w) == TEXT_WIND )
		return ITM_NOTUSED;

	return (((ITM_WINDOW *) w)->itm_func->itm_type) (w, item);
}


/*
 * Determine the type of an item in a window, but if
 * the item is a link, follow it to the target
 */

ITMTYPE itm_tgttype(WINDOW *w, int item)
{
	if ( xw_type(w) == TEXT_WIND )
		return ITM_NOTUSED;

	return (((ITM_WINDOW *)w)->itm_func->itm_tgttype) (w, item);
}


/*
 * Get the name of an item in a directory window
 */

const char *itm_name(WINDOW *w, int item)
{
	return (((ITM_WINDOW *)w)->itm_func->itm_name)(w, item);
}


/*
 * Get full name (i.e. path + name) of an item in a dir window 
 * item identified by its ordinal in the list.
 */

char *itm_fullname(WINDOW *w, int item)
{
	return (((ITM_WINDOW *)w)->itm_func->itm_fullname) (w, item);
}


/*
 * Get target name of an object which is a link;
 * If the object is not a link, return name of the object.
 * This routine allocates space for the name obtained.
 */

char *itm_tgtname(WINDOW *w, int item)
{
	char 
		*thename,
		*fullname = (((ITM_WINDOW *)w)->itm_func->itm_fullname) (w, item);

#if _MINT_
	if ( itm_islink(w, item) )
	{
		thename = x_fllink(fullname); 	/* allocate new space */
		free(fullname);					/* free old space */
	}
	else
#endif
		thename = fullname;

	return thename;	
}


/*
 * Get iem attributes; on mode=0 follow link, on mode=1 don't
 */

int itm_attrib(WINDOW *w, int item, int mode, XATTR *attrib)
{
	int result = (((ITM_WINDOW *) w)->itm_func->itm_attrib) (w, item, mode, attrib);

	wd_setselection(w);
	return result;
}


/*
 * Is the window object a link? Return TRUE if it is.
 */

boolean itm_islink(WINDOW *w, int item)
{
#if _MINT_
	return (((ITM_WINDOW *) w)->itm_func->itm_islink) (w, item);
#else
	return FALSE;
#endif
}


/*
 * Is the object a link and should it be followed?
 * If so, this routine resets 'link' to FALSE.
 * Use the opportunity to find target object's fullname.
 */

boolean itm_follow(WINDOW *w, int item, boolean *link, char **name, ITMTYPE *type)
{
#if _MINT_
	boolean ll =  itm_islink(w, item);

	if (ll && ((options.cprefs & CF_FOLL) != 0))
	{
		*link = FALSE;
		*type = itm_tgttype(w, item);
		*name = itm_tgtname(w, item);

		if(*type != ITM_NETOB)	/* Don't follow to net objects */
			return TRUE;
	}
	else
#else
	boolean ll = FALSE;
#endif
	{
		*link = ll;
		*name = itm_fullname(w, item);
		*type = itm_type(w, item);
	}

	return FALSE;
}


boolean itm_open(WINDOW *w, int item, int kstate)
{
	return (((ITM_WINDOW *)w)->itm_func->itm_open) (w, item, kstate);
}


/*
 * Funktie voor het selecteren van een item in een window.
 * mode = 0: selecteer selected en deselecteer de rest.
 * mode = 1: inverteer de status van selected.
 * mode = 2: deselecteer selected.
 * mode = 3: selecteer selected.
 * mode = 4: select all  
 */

void itm_select(WINDOW *w, int selected, int mode, boolean draw)
{
	if (xw_exist(w))
		(((ITM_WINDOW *)w)->itm_func->itm_select) (w, selected, mode, draw);

	wd_setselection(w);
}


/* 
 * A shorter form of the above
 */

static void itm_selall(WINDOW *w)
{
	itm_select(w, 0, 4, TRUE);
}


/*
 * Return a list of items selected in a window.
 * This routine will return FALSE, a NULL list pointer, and zero item count
 * if no list has been created.
 * If list allocation fails, an alert will be generated.
 * Note: XWF_SIM (x)flag is set in dir_simw() to mark a simulated window;
 * it is checked here because xw_exist does not recognize simulated windows
 */

static int *itm_list(WINDOW *w, int *n)
{
	if
	(
		w == NULL || 
		(!xw_exist(w) && (w->xw_xflags & XWF_SIM) == 0) || 
		xw_type(w) == TEXT_WIND /* DIR_WIND and DESK_WIND are permitted */
	)
	{
		/* this is neither a simulated nor a real window with items */

		*n = 0;
		return NULL;
	}

	return (((ITM_WINDOW *) w)->itm_func->itm_list)(w, n);
}


/********************************************************************
 *																	*
 * Functies voor het afhandelen van muisklikken in een window.		*
 *																	*
 ********************************************************************/

static boolean itm_copy(WINDOW *sw, int n, int *list, WINDOW *dw,
					 int dobject, int kstate, ICND *icnlist, int x, int y)
{
	if (dw)
	{
		/* Can't copy over/to a file */

		if ((dobject != -1) && (itm_tgttype(dw, dobject) == ITM_FILE))
			dobject = -1;

		/* 
		 * Perform window-type-specific item-copy operation.
		 * Beware that a deselection after the operation can not be done
		 * if the windows have been closed meanwhile, as in single-TOS
		 * when starting a program by copying/dragging objects to it.
		 * If windows have been closed, selection.w is NULL
		 */

		if (((ITM_WINDOW *)dw)->itm_func->itm_copy(dw, dobject, sw, n, list, icnlist, x, y, kstate))
		{
			if( !va_reply && selection.w && ((options.cprefs & CF_KEEPS) == 0) )
				itm_select(sw, -1, 0, TRUE);
		}

		return TRUE;
	}
	else
	{
		alert_printf(1, AILLDEST);
		return FALSE;
	}
}


/* 
 * Drag & drop items from a list onto a window at position x,y,
 * using the drag & drop protocol. This routine does something useful
 * only in the multitasking version; otherwise, it just returns 
 * an error. It should be called only if destination window is not TeraDesk's
 */

static boolean itm_drop
(
	WINDOW *w, 			/* source window */
	int n,				/* number of selected items */ 
	int *list, 			/* list of selected items' indices */
	int kstate, 		/* keyboard state */
	int x, 				/* position of destination window */
	int y				/* positon of destination window  */
)
{
#if _MINT_
	const char 
		*path;		/* full name of the dragged item */

	int 
		*item, 		/* (pointer to) index of a selected item */
		fd,			/* pipe handle */ 
		i, 			/* item counter */
		apid = -1,	/* destination app id */ 
		hdl;		/* destination window handle */
	
	/* Find the owner of the window at x,y */

	if ( (hdl = wind_find(x,y)) > 0)
		wind_get(hdl, WF_OWNER, &apid); 

	/* Drag & drop is possible only in Mint or Magic (what about geneva 6?) */

	if (mint && apid >= 0) 
	{
		char ddsexts[34];
		long nsize = 0;

		/* Create a string with filenames to be sent */

		*global_memory = 0;
		item = list;

		for ( i = 0; i < n; i++ )
		{
			if(*item >= 0)
			{

				path=itm_fullname(w, *item);

				/* Concatenate filename(s) */

				if (!va_add_name(itm_type(w, *item), path))
				{
					free(path);
					return FALSE;
				}

				free (path);
			}

			item++;
		}

		nsize = (long)strlen(global_memory);

		/* Create a drag&drop pipe; if successful, continue */

		fd = ddcreate(apid, ap_id, hdl, x, y, kstate, ddsexts);

		if (fd > 0)
		{
			int reply;

			/* Will the recepient accept data ? */

			reply = ddstry(fd, "ARGS", (char *)empty, nsize);

			/* 
			 * Do whatever is needed, depending on what is on destination.
			 * Note: there is a warning in the docs NOT to do any
			 * wind_update during the operation, so itmlist_op
		 	 * below may do it wrong if wind_update is called somewhere
			 * from them ?
			 */

			switch(reply)
			{
				case DD_OK:
				{
					/* Write the name(s) down the pipe, then close the pipe */
					Fwrite(fd, nsize, global_memory);
					break;
				}
				case DD_PRINTER:
				{
					/* dropped on a printer, so print items */
					itmlist_wop(w, n, list, CMD_PRINT);
					break;
				}
				case DD_TRASH:
				{
					/* dropped on a trashcan, so delete these items */
					itmlist_wop(w, n, list, CMD_DELETE);
					break;
				}
				case DD_CLIPBOARD:	/* app should put it in clipboard */
				case DD_NAK:		/* app refuses the drop */
				case DD_EXT:		/* app doesn't understand this type */
				case DD_LEN:		/* app can't receive so much data */
				default:	
				{
					/* These drag & drop operations are not (yet) supported */
					alert_iprint(APPNOEXT); 
					ddclose(fd);
					return FALSE;
				}
			}

			ddclose(fd);
			itm_select(w, -1, 0, TRUE);	

			return TRUE;
		}
		else
		{
			/* Destination app does not know D&D or won't accept data */

			alert_iprint(APPNODD); 
		}
	}
	else
#endif
		alert_printf(1, AILLDEST); /* Illegal copy destination */

	return FALSE;
}


/* 
 * Routines voor het tekenen van de omhullende van een icoon. 
 */

static void get_minmax(ICND *icns, int n, int *clip)
{
	int
		*cp,
		i,
		j,
		j2,
		icj2,
		icj21;

	ICND
		*icnd = icns;


	for (i = 0; i < n; i++)
	{
		for (j = 0; j < icnd->np; j++)
		{
			if ((i == 0) && (j == 0))
			{
				cp = clip;

				*cp++ = icnd->coords[0];	/* [0] */
				*cp++ = icnd->coords[1];	/* [1] */
				*cp++ = clip[0];			/* [2] */
				*cp   = clip[1];			/* [3] */
			}
			else
			{
				j2 = j * 2;
				icj2 = icnd->coords[j2];
				icj21 = icnd->coords[j2 + 1];

				cp = clip;	/* [0] */

				if (*cp > icj2) /* Do not use min() here */
					*cp = icj2;

				cp++; 		/* [1] */

				*cp = min(*cp, icj21);

				cp++;		/* [2] */

				if ( *cp < icj2)
					*cp = icj2; /* Do not use max() here */

				cp++;		/* [3] */

				*cp = max(*cp, icj21);
			}
		}

		icnd++;
	}
}


/* 
 * Ensure that coordinates are within certain limits;
 * In this particular case there would be no gain in size using minmax()
 * but there would be a loss in speed
 */

static void clip_coords(int *clip, int *nx, int *ny)
{
	int
		h,
		*cp = clip;


	h = xd_desk.x - *cp++;
	if (*nx < h)
		*nx = h;

	h = xd_desk.y - *cp++;
	if (*ny < h)
		*ny = h;

	h = xd_desk.x + xd_desk.w - 1 - *cp++;
	if (*nx > h)
		*nx = h;

	h = xd_desk.y + xd_desk.h - 1 - *cp;
	if (*ny > h)
		*ny = h;
}


static void draw_icns(ICND *icns, int n, int mx, int my, int *clip)
{
	ICND
		*icnd;

	int
		*cp,
		*icp,
		i,
		j,
		c[18],
		x = mx,
		y = my;


	clip_coords(clip, &x, &y);
	set_rect_default();
	xd_mouse_off();

	icnd = icns;

	for (i = 0; i < n; i++)
	{
		cp = c,
		icp = icnd->coords;

		for (j = 0; j < icnd->np; j++)
		{
			*cp++ = *icp++ + x;
			*cp++ = *icp++ + y;
		}

		v_pline(vdi_handle, icnd->np, c);

		icnd++;
	}

	xd_mouse_on();
}


static void desel_old(void)
{
	if (selection.w)
		itm_select(selection.w, -1, 0, TRUE);
}


/*
 * mode = 2 - deselecteren, mode = 3 - selecteren 
 */

static void select_object(WINDOW *w, int object, int mode)
{
	if (object < 0)
		return;

	if (itm_tgttype(w, object) != ITM_FILE)
		itm_select(w, object, mode, TRUE);
}


/*
 * Find the window and the object at position x,y 
 */

static void find_newobj(int x, int y, WINDOW **wd, int *object, boolean *state)
{
	WINDOW *w = xw_find(x, y);

	*wd = w;
	*object = -1;

	if (w != NULL)
	{
		if ((xw_type(w) == DIR_WIND) || (xw_type(w) == DESK_WIND))
		{
			if ((*object = itm_find(w, x, y)) >= 0)
				*state = itm_state(w, *object);
		}
	}
}


/*
 * Copy objects from an object (window) to another
 */

boolean itm_move
(
	WINDOW *src_wd,	/* source window */ 
	int src_object,	/* source object */ 
	int old_x,		/* initial position */ 
	int old_y,		/* initial position */ 
	int avkstate 	/* key state only in av protocol */
)
{
	int 
		x = old_x, 
		y = old_y,
		ox, 
		oy, 
		kstate, 
		*list,
		*listi, 
		n, 
		nv = 0, 
		i,
		cur_object = src_object, 
		new_object,
		clip[4] = {0, 0, 0, 0};

	WINDOW 
		*cur_wd = src_wd, 
		*new_wd;

	boolean 
		result = FALSE,
		cur_state = TRUE, 
		new_state, 
		mreleased;

	ICND 
		*icnlist = NULL;

	SEL_INFO
		keepsel = selection;	/* to restore corrupted "selection" */


	if ( src_object >= 0 && itm_type(src_wd, src_object) == ITM_PREVDIR )
	{
		if (!va_reply)
		{
			/* Wait for a button click */
			while
				(xe_button_state());
		}

		return result;
	}

	/* Create a list of selected objects in the source window */

	if ( (list = itm_list(src_wd, &n)) == NULL )
		return result;

	/* Don't do the following if AV-protocol command is executed */

	if (!va_reply)
	{
		listi = list;

		for (i = 0; i < n; i++)
		{
			if (itm_type(src_wd, *listi) == ITM_PREVDIR)
				itm_select(src_wd, *listi, 2, TRUE);

			listi++;
		}

		free(list); 

		if ((icnlist = ((ITM_WINDOW *)src_wd)->itm_func->itm_xlist(src_wd, &n, &nv, &list, old_x, old_y)) == NULL)
			return result;
	}

	get_minmax(icnlist, nv, clip); /* if nv == 0 clip is unchanged */
	xd_begmctrl();
	graf_mouse(FLAT_HAND, NULL);
	draw_icns(icnlist, nv, x, y, clip);

	/* Loop until object(s) released at a positon */

	do
	{
		if ( va_reply )
		{
			x = old_x;
			y = old_y;
			mreleased = TRUE; /* position is supplied, so as if released immediately */
			kstate = avkstate;
		}
		else
		{
			ox = x;
			oy = y;
			mreleased = xe_mouse_event(0, &x, &y, &kstate);
		}

		if ( va_reply || (x != ox) || (y != oy))
		{
			/* Safe, becuse if va_reply, nv=0, so undefined ox,oy do not matter */

			draw_icns(icnlist, nv, ox, oy, clip);

			/* Find the window and the object at destination */

			find_newobj(x, y, &new_wd, &new_object, &new_state);

			if ((cur_wd != new_wd) || (cur_object != new_object))
			{
				if (!cur_state && (cur_object >= 0))
					select_object(cur_wd, cur_object, 2);

				cur_wd = new_wd;
				cur_object = new_object;
				cur_state = new_state;

				if ((cur_object >= 0) && (cur_state == FALSE))
					select_object(cur_wd, cur_object, 3);
			}

			if (!mreleased)
				draw_icns(icnlist, nv, x, y, clip);
		}
		else if (mreleased)
			draw_icns(icnlist, nv, x, y, clip);
	}
	while (mreleased == FALSE);

	arrow_mouse();
	xd_endmctrl();

	/* 
	 * During moving of the cursor over objects, current selection may
	 * have changed; now restore it
	 */

	selection = keepsel;

	if (!cur_state && (cur_object >= 0))
		select_object(cur_wd, cur_object, 2);

	if ((cur_wd != src_wd) || (cur_object != src_object))
	{
		if (cur_wd != NULL)
		{
			int cur_type = xw_type(cur_wd);

			switch(cur_type)
			{
				case DIR_WIND:
				case DESK_WIND:
				{
					/* 
					 * Test if destination window is the desktop and if the
					 * destination object is -1 (no object). 
					 */
	
					if ((xw_type(cur_wd) == DESK_WIND) && (cur_object == -1) && (xw_type(src_wd) == DESK_WIND))
						clip_coords(clip, &x, &y);
					result = itm_copy(src_wd, n, list, cur_wd, cur_object, kstate, icnlist, x, y);
					break;
				}
				case ACC_WIND:
				{
					result = va_accdrop(cur_wd, src_wd, list, n, kstate, x, y );
					break;
				}
				case TEXT_WIND:			
				{
					if (n == 1 && isfileprog(itm_tgttype(src_wd, list[0])))
					{
						char *path = itm_fullname(src_wd, list[0]);
						result = txt_reread((TXT_WINDOW *)cur_wd, x_fllink(path), 0, 0L);
						free(path);
						break;
					}
				}
				default:
				{
					alert_printf(1, AILLCOPY);
				}
			}
		}
		else
			result = itm_drop(src_wd, n, list, kstate, x, y); /* MultiTOS drag & drop */
	}

	free(list);
	free(icnlist);
	return result;
}


/*
 * Check whether position (x,y) is inside the work area of window *w);
 */

static boolean in_window(WINDOW *w, int x, int y)
{
	RECT work;

	xw_getwork(w, &work);

	if 
	(   
		(x >= work.x) && (x < (work.x + work.w)) && 
		(y >= work.y) && (y < (work.y + work.h))
	)
		return TRUE;
	else
		return FALSE;
}


/* 
 * Button handling routine. 
 * This is called in desktop, directory and text windows.
 * Note: bstate never used in this routine ? 
 * Parameters:
 *
 * w			- Pointer naar window
 * x			- x positie muis
 * y			- y positie muis
 * n			- aantal muisklikken
 * bstate		- Toestand van de muisknoppen
 * kstate		- Toestand van de SHIFT, CONTROL en ALTERNATE toets
 */

void wd_hndlbutton(WINDOW *w, int x, int y, int n, int bstate, int kstate)
{
	int 
		wt = xw_type(w),
		item, 
		m_state;


	if(wt == DIR_WIND || wt == DESK_WIND)
	{
		/* If a right-mouse-click-extension application is installed... */

		if(xd_rbdclick == 0 && bstate == 2)
		{
			int *list, ni;

			onfile = TRUE;

			list = ((ITM_WINDOW *)w)->itm_func->itm_list(w, &ni);
			app_specstart(AT_RBXT, w, list, ni, kstate);
			free(list); 

			onfile = FALSE;
			return;
		}

		/* Otherwise proceed normally... */

		if (selection.w != w)
			desel_old();

		m_state = xe_button_state();
		item = itm_find(w, x, y);

		if (item >= 0)
		{
			if (n == 2)
			{
				itm_select(w, item, 0, TRUE);

				/* Wait for a button click */

				while 
					(xe_button_state());

				/* 
				 * Note: if itm_open() starts a program in single-tos,
				 * windows will be closed and reopened, and "w" will no
				 * longer will be valid. In this case selection.w will be NULL;
				 * deselect object only if this is not the case
				 */	

				if (itm_open(w, item, kstate))
				{
					w = selection.w;

					if(w)
						itm_select(w, item, 2, TRUE);
				}
			}
			else
			{
				if ((m_state == 0) || (itm_state(w, item) == FALSE))
				{
					itm_select(w, item, (kstate & (K_RSHIFT | K_LSHIFT)) ? 1 : 0, TRUE);
				}

				if ((m_state != 0) && itm_state(w, item))
					itm_move(w, item, x, y, 0);
			}

			wd_setselection(selection.w);
		}
		else if (in_window(w, x, y))
		{
			autoloc_off();

			/* 
			 * Top TeraDesk by clicking on desktop area. Unfortunately,
			 * it is not possible to handle this in the same way for all AESes
			 */

#if _MINT_
			if ( (mint || geneva) && xw_type(w) == DESK_WIND )
			{
				/*  This works only if TeraDesk has open windows ! */

				WINDOW *ww = xw_first();
				boolean nowin = TRUE;

				while (ww)
				{
					if(wd_dirortext(ww))
					{
						wd_type_topped(ww);	
						nowin = FALSE;
						break;
					}

					ww = xw_next(ww);
				}

				/* 
				 * This works without any open windows, but unfortunately
				 * not in all AESses. Will be ok in N.AES, XaAES, Magic 
				 */
 
				if (nowin)
					wd_top_app(ap_id);
			}
#endif

			if (((m_state == 0) || ((kstate & (K_RSHIFT | K_LSHIFT)) == 0)) && (selection.w == w))
				itm_select(w, -1, 0, TRUE);

			if (m_state)
				((ITM_WINDOW *)w)->itm_func->itm_rselect(w, x, y);

			wd_setselection(w); 
		}
	} /* wt ? */
}



/********************************************************************
 *																	*
 * Functions for window iconify/uniconify.							*
 *																	*
 ********************************************************************/


/*
 * Set-up background object for icon(s) in a window;
 * this routine is used for iconified windows and for
 * directory windows in icon mode 
 */

void wd_set_obj0
(
	OBJECT *obj,	/* pointer to object */
	boolean smode,	/* smode=1: G_IBOX, smode=0: G_BOX */
	int row,		
	int lines,		/* how many icon lines will be drawn */
	int yoffset, 	/* icons offset from the top of work area */
	RECT *work		/* window work area dimensions and position*/
)
{
	/* Note: when object type is I_BOX it will not be redrawn */

	init_obj(&obj[0], (smode) ? G_IBOX : G_BOX);
	obj[0].ob_type |= (XD_BCKBOX << 8);

	xd_xuserdef(&obj[0], &wxub, ub_bckbox);

	wxub.ob_flags = 0;	/* so that frame around the object will not be drawn */
	wxub.uv.fill.colour = options.win_colour;
	wxub.uv.fill.pattern = options.win_pattern;

	obj[0].r.x = work->x;
	obj[0].r.y = row + work->y + YOFFSET - yoffset;
	obj[0].r.w = work->w;
	obj[0].r.h = minmax(ICON_H, lines * ICON_H, work->h);
}


/* 
 * Set one icon object.
 * Note similar object-setting code in add_icon() in icon.c
 */

void set_obji( OBJECT *obj, long i, long n, boolean selected, boolean hidden, boolean link, int icon_no, int obj_x, int obj_y, char *name )
{
	OBJECT
		*obji = &obj[i + 1];

	CICONBLK
		*cicnblk = (CICONBLK *) &obj[n + 1];


	init_obj(obji, icons[icon_no].ob_type);

	if(selected)
		obji->ob_state = SELECTED;

	if (hidden && (obj[0].ob_spec.obspec.interiorcol == 0) )
		obji->ob_state |= DISABLED;	/* will this work in all AESes ? */

	if (link)
		obji->ob_state |= CHECKED;

	obji->r.x = obj_x;
	obji->r.y = obj_y;
	obji->ob_spec.ciconblk = &cicnblk[i];

	cicnblk[i] = *icons[icon_no].ob_spec.ciconblk;

	cicnblk[i].monoblk.ib_ptext = name;
	cicnblk[i].monoblk.ib_char &= 0xFF00;
	cicnblk[i].monoblk.ib_char |= 0x20;

	objc_add(obj, 0, (int)i + 1);
}


/* 
 * Draw contents of an iconified window 
 */

static void icw_draw(WINDOW *w)
{

	OBJECT
		*obj;			/* background and icon objects */
	
	const char 
		*wpath = wd_path(w);

	INAME 
		icname;			/* name of icon in desktop.rsc */
	
	int 
		dx, dy,			/* icon offsets in the root object */
		icon_no, 		/* icon identifier in resource files */
		icon_ind;		/* icon identifier in resource files */
	

	/* allocate memory for objects: background + 1 icon */

	if ((obj = malloc_chk( 2 * sizeof(OBJECT) + sizeof(CICONBLK))) == NULL)
		return;

	/* Center the icon in the window work area */

	dx = (w->xw_work.w - ICON_W + XOFFSET) / 2;
	dy = (w->xw_work.h - ICON_H + YOFFSET) / 2;

	/* Set background object. It is needed for background colour / pattern */

	wd_set_obj0(obj, FALSE, 0, 2, YOFFSET, &(w->xw_work));

	/* Decide which icon to use: file, floppy, disk or folder */

	if ( xw_type(w) == TEXT_WIND )
		icon_ind = FIINAME; 			/* file icon */
	else
	{
		if ( wpath && isroot(wpath) )
		{
			if ( *wpath <= 'B' ) 
				icon_ind = FLINAME; 	/* floppy icon */
			else
				icon_ind = HDINAME;		/* hard disk icon */
		}
		else
			icon_ind = FOINAME;			/* folder icon */
	}

	icon_no = rsrc_icon_rscid( icon_ind, icname );
	cramped_name(wpath ,icname, sizeof(INAME));

	/* Set icon centered in work area */

	set_obji( obj, 0L, 1L, FALSE, FALSE, FALSE, icon_no, dx, dy, icname );

	/* (re)draw it */
	
	xd_begupdate();
	draw_icrects(w, obj, &(w->xw_size));
	xd_endupdate();
	free(obj);
}


/* 
 * Routine for text/dir window iconification 
 */

void wd_type_iconify(WINDOW *w, RECT *r)
{
#if _MINT_

	/* Can this be done at all ? */

	if ( can_iconify )
	{
		WINFO *wi = ((TYP_WINDOW *)w)->winfo;
		RECT oldsize;

		/* Change window title to "Tera Desktop" */

		xw_set((WINDOW *)w, WF_NAME, nonwhite(get_freestring(MENUREG)));

		/* Remember size of noniconified window */

		if(!(wi->flags.iconified))
			*(RECT *)(&(wi->ix)) = *(RECT *)(&(wi->x));

		oldsize = w->xw_size;

		/* Call the function to iconify this window. remember iconified size */

		xw_iconify(w, r);

		icwsize = w->xw_size;

		/* 
		 * Set a flag that this is an iconified window;
		 * note: this information is duplicated by xw_iconify
		 * in w->xw_xflags, so that xdialog routines know of
		 * iconified state. flags.iconified is used for saving
		 * in .cfg file
		 */

		wi->flags.iconified = 1;

		/* Convert and remember the new position */

		wd_set_defsize(wi);

		/* Draw contents of iconified window */

		icw_draw(w);

		xw_top(); /* which is the new top window? */

		/* Some additional redraws may be needed with forced iconifying in Geneva */

		if(geneva && r->w == -1)
		{
			WINDOW *ww = xw_first();

			oldsize.h += 4;
			oldsize.w += 4;

			while(ww)
			{
				xw_send_rect(ww, WM_REDRAW, ww->xw_ap_id, &oldsize);
				ww = xw_next(ww);
			}

			redraw_desk(&oldsize);
		}
	}

#endif

}


/* 
 * Routine for text/dir window deiconification 
 */

void wd_type_uniconify(WINDOW *w, RECT *r)
{
#if _MINT_

	RECT
		size;

	WINFO
		*wi = ((TYP_WINDOW *)w)->winfo;


	wd_type_topped(w);

	/* Calculate window size */

	wd_restoresize(wi);

	wi->flags.iconified = 0;
	wd_calcsize(wi, &size);

	r->x = size.x;
	r->y = size.y;

	/* Call uniconify function then resize the window */

	xw_uniconify(w, r);
	wd_type_sized(w, &size);

	/* This seems to be needed only if uniconified with -1, -1, -1, -1 size */

	if(r->w == -1)
		xw_send_redraw(w, WM_REDRAW, &size);
#endif
}


/*
 * Is a window really iconified right now?
 * Rectangle r is used just for convenience;
 * it does not contain any real rectangle data
 */

static boolean wd_isiconified(TYP_WINDOW *w)
{
#if _MINT_

	RECT r;
	
	r.x = 0;

	if(can_iconify && w != NULL)
	{
		if(wd_dirortext((WINDOW *)w))
			r.x = w->winfo->flags.iconified;
		else
			xw_get((WINDOW *)w, WF_ICONIFY, &r);
	}

	return (boolean)(r.x); 

#else

	return FALSE;

#endif
}


/* 
 * Open a window, normal size or iconified, depending on flag state.
 * Note that this routine is only used on directory windows and
 * text windows, NOT av-client windows (which do not have a WINFO pointer)
 */

void wd_iopen ( WINDOW *w, RECT *oldsize, WDFLAGS *oldflags )
{
	WINFO 
		*info = ((TYP_WINDOW *)w)->winfo;

	RECT 
		size;

#if _MINT_
	boolean 
		icf = can_iconify && oldflags->iconified; 
#endif

	if ( avsetw.flag )
	{
		/* size is set through AV-protocol */

		size = avsetw.size;
		avsetw.flag = FALSE;
	}
	else
	{
		/* properly recalculate size */

		info->flags = *oldflags;		/* now set real fulled & iconified state */

#if _MINT_
		if(icf)
		{
			*(RECT *)(&info->x) = *oldsize;

			/* note: icwsize must be set before wd_calcsize() or wd_wsize() */

			icwsize.w = oldsize->w * xd_fnt_w;
			icwsize.h = oldsize->h * xd_fnt_w; /* Beware: fnt_w, not fnt_h! */
		}
#endif
		wd_calcsize(info, &size);

#if _MINT_
		/* In Magic, first iconify, then open ?! */

		if(magx && icf)
			xw_iconify(w, &size);
#endif
	}

	/* Set window title and open the window */

	wd_type_title((TYP_WINDOW *)w);

	xw_open( w, &size );

#if _MINT_

	/* Tell AES to iconify this window; set the flag */

	if ( icf ) 
		wd_type_iconify(w, &size);

#endif

	xw_note_top(w);

	info->used = TRUE;
}



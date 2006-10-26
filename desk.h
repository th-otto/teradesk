/*
 * Teradesk. Copyright (c)       1993, 1994, 2002  W. Klaren.
 *                                     2002, 2003  H. Robbers,
 *                         2003, 2004, 2005, 2006  Dj. Vukovic
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


#include <boolean.h>
#include <stddef.h>

#ifdef MEMDEBUG
/* Note: a specific directory required below: */
#include <d:\tmp\memdebug\memdebug.h>
#endif


#undef O_APPEND

/* 
 * Some convenient macros for manipulating dialog objects
 */

#define obj_hide(x)		x.ob_flags |= HIDETREE
#define obj_unhide(x)	x.ob_flags &= ~HIDETREE  
#define obj_select(x)	x.ob_state |= SELECTED
#define obj_deselect(x)	x.ob_state &= ~SELECTED
#define obj_enable(x)	x.ob_state &= ~DISABLED
#define obj_disable(x)	x.ob_state |= DISABLED

/* 
 * Size of global memory buffer is larger for fuller 
 * AV-protocol support 
 */

#if _MORE_AV
#define GLOBAL_MEM_SIZE sizeof(XLNAME)
#else
#define GLOBAL_MEM_SIZE	sizeof(XLNAME) / 2
#endif

/* Maximum number of Teradesk's windows of one type */

#define MAXWINDOWS 8 

/* Maximum path length (in single-tos mode?) */

#define PATH_MAX 128 /* from stdio.h */

/* Diverse options which are bitflags */


/* Copy and print options; these go into options.cprefs */

#define CF_COPY			0x0001	/* confirm copy              */
#define CF_DEL			0x0002	/* confirm delete            */
#define CF_OVERW		0x0004	/* confirm overwrite         */
#define CF_PRINT		0x0008 	/* confirm print             */
#define CF_TOUCH		0x0010  /* confirm touch (not used)  */
#define CF_KEEPS		0x0020	/* keep selection after copy */
								/* unused 0x0040     */
#define P_GDOS			0x0080	/* use GDOS device for printing; currently NOT used */
								/* unused 0x0100 0x0200 */
#define CF_CTIME		0x0400	/* change date & time */
#define CF_CATTR		0x0800  /* change file attributes */
#define CF_SHOWD		0x1000 	/* always show copyinfo dialog */
#define P_HEADER		0x2000	/* print header and formfeed */
#define CF_FOLL			0x4000	/* follow links */

/* Other diverse dialog options */

#define DIALPOS_MODE	0x0040	/* OFF = mouse, ON = center    */
#define DIAL_MODE   	0x0003  /* 0x0001 = XD_BUFFERED (flying), 0x0002 = XD_WINDOW */

#define TEXTMODE		0		/* display directory as text */
#define ICONMODE		1		/* display directory as icons */

/* These options are for menu items */

#define WD_SORT_NAME	0x00	/* sort directory by file name  */
#define WD_SORT_EXT		0x01	/* sort directory by file type  */
#define WD_SORT_DATE	0x02	/* sort directory by file date  */
#define WD_SORT_LENGTH	0x03	/* sort directory by file size  */
#define WD_NOSORT		0x04	/* don't sort directory         */
#define WD_REVSORT	 	0x10	/* reverse sort order (bitflag) */

/* Option bitflags for elements to be shown in directory windows */

#define WD_SHSIZ 0x0001 /* show file size  */
#define WD_SHDAT 0x0002 /* show file date  */
#define WD_SHTIM 0x0004 /* show file time  */
#define WD_SHATT 0x0008 /* show attributes */
#define WD_SHOWN 0x0010 /* show owner      */

/* Option bitflags for some settable video modes and related information in options.vprefs */

#define VO_BLITTER	0x0001 	/* video option blitter ON  */
#define VO_OVSCAN  	0x0002 	/* video option overscan ON */
							/* unused: 0x0004 0x0008 0x0010 0x0020 0x0040 0x0080 */
#define SAVE_COLORS	0x0100	/* save palette */

/* Saving options; these go into options.sexit */

#define SAVE_CFG	0x0001	/* Save configuration at exit */
#define SAVE_WIN	0x0002	/* Save open windows at exit */

/* Option bitflags for other diverse settings; these go into options.xprefs */

#define S_IGNCASE	0x0001	/* Ignore string case when searching */
#define S_SKIPSUB	0x0002	/* Skip subdirectories when searching */
							/* Unused: 0x0002, 0x0004 */
#define TOS_KEY		0x0020	/* 0 = continue, 1 = wait */
#define TOS_STDERR	0x0200	/* 0 = no redirection, 1 = redirect handle 2 to 1. */

/*
 * It is assumed that the first menu item for which a keyboard shortcut
 * can be defined is MOPEN and the last is MSAVEAS; the interval
 * is defined below by MFIRST and MLAST. 
 * Index of the fist relevant menu title is likewise defined.
 * Beware of dimensioning options.kbshort[64]; it should be larger
 * than NITEM below; currently, about 58 out of 64 are used.
 */

#define MFIRST MOPEN			/* first menu item for which a shortcut can be */
#define MLAST  MSAVEAS			/* last menu item ... */
#define TFIRST TLFILE			/* title under which the first item is */
#define NITEM MLAST - MFIRST	/* number of, -1 */

/* Executable file (program) types */

typedef enum
{
	PGEM = 0, 
	PGTP, 
	PACC, 
	PTOS, 
	PTTP
} ApplType;


/* Configuration options structure */

typedef struct
{	
	/* Desktop */

	int version;				/* cfg. file version */
	int cprefs;					/* copy prefs: CF_COPY|CF_DEL|CF_OVERW|CF_PRINT|CF_TOUCH|CF_CTIME|CF_CATTR|CF_SHOWD|P_HEADER|CF_FOLL */
	int xprefs;					/* more preferences: S_IGNCASE | S_SKIPSUB | TOS_KEY | TOS_STDERR  */
	unsigned int dial_mode;		/* dialog mode (window/flying) */
	unsigned int sexit;			/* save on exit */
	int kbshort[NITEM + 2];		/* keyboard shortcuts */

	/* Sizes */

	int bufsize;				/* copy buffer size  */
	long max_dir;				/* maximum no of entries for 1 hierarchic level */
	int plinelen;				/* printer line length */
	int tabsize;				/* global tab size */
	int cwin;					/* compare match window */

	/* View */

	int mode;					/* text or icon mode */
	int sort;					/* sorting rule */
	int aarr;					/* auto arrange directory items */
	int fields;					/* shown file data elements */
	int attribs;				/* shown attributes of visible directory items */

	/* Video */

	int vprefs;                 /* video preferences  */
	int vrez;                   /* video resolution */
	int dsk_color;				/* desktop colour */
	int win_color;				/* window colour */
	int dsk_pattern;			/* desktop pattern  */
	int win_pattern;			/* window pattern */

} Options;


typedef struct
{
	int item;					/* nummer van icoon */
	int m_x;					/* coordinaten middelpunt t.o.v. muis, alleen bij iconen */
	int m_y;
	int np;						/* number of points in object's contour */
	int coords[18];				/* coordinaten van schaduw */
} ICND;

typedef struct
{
	int phy_handle;
	int vdi_handle;
	RECT dsk;
	int fnt_w;
	int fnt_h;
} SCRINFO;


extern Options options;
extern SCRINFO screen_info;

extern int 
	vdi_handle,		/* workstation handle */ 
	max_w, max_h, 	/* maximum possible window size */
	ap_id,			/* application id of TeraDesk itself */ 
	nfonts;			/* number of available fonts */

extern char 
		*global_memory;

extern const char
		*empty,
		*bslash,
		*adrive,
		*prevdir;

/* Flags to show a specific OS or AES type (detected from cookies) */

#if _MINT_
extern boolean 
	mint,			/* mint or magic present */
	magx,			/* magic present  */
	naes,			/* naes present */
	geneva;			/* geneva present */

extern int have_ssystem;
#endif


char *itoa(int value, char *string, int radix);
char *ltoa(long value, char *string, int radix);

long btst(long x, int bit);
void *malloc_chk(size_t size);
int chk_xd_dialog(OBJECT *tree, int start);
int chk_xd_open(OBJECT *tree, XDINFO *info);
void set_opt(OBJECT *tree, int flags, int opt, int button ); 
void get_opt(OBJECT *tree, int *flags, int opt, int button );
char *strsncpy(char *dst, const char *src, size_t len);	/* secure copy (0 --> len-1) */
int scansh ( int key, int kstate );
int hndlmessage(int *message);
boolean wait_to_quit(void);



/*
 * Teradesk. Copyright (c) 1993, 1994, 2002 W. Klaren.
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

#include "desktop.h"			/* HR 151102: only 1 rsc */

extern
OBJECT *menu,
	   *setprefs,
	   *setprgprefs,
	   *addprgtype,
	   *openfile,
	   *newfolder,
	   *driveinfo,
	   *folderinfo,
	   *fileinfo,
	   *infobox,
	   *addicon,
	   *getcml,
	   *nameconflict,
	   *copyinfo,
	   *print,
	   *setmask,
	   *applikation,
	   *seticntype,
	   *addicntype,
	   *loadmods,
	   *viewmenu,
	   *stabsize,
	   *wdoptions,
	   *wdfont;

extern
char *dirname,
	 *oldname,
	 *newname,
	 *finame,
	 *flname,
	 *disklabel,
	 *drvid,
	 *iconlabel,
	 *cmdline1,
	 *cmdline2,
	 *cpfile,
	 *cpfolder,
	 *filetype,
	 *tabsize,
	 *copybuffer,
	 *applname,
	 *appltype,
	 *applcmdline,
	 *applfkey,
	 *prgname,
	 *icnname,
	 *vtabsize;

extern
char dirnametxt[],		/* HR 021202: The 5 scrolling editable texts. */
     finametxt[],
     flnametxt[],
     oldnametxt[],
     newnametxt[];

void rsc_init(void);
void rsc_title(OBJECT *tree, int object, int title);
int rsc_form_alert(int def_button, int message);
int rsc_aprintf(int def_button, int message,...);
void rsc_ltoftext(OBJECT *tree, int object, long value);

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
#include <stdlib.h>
#include <string.h>
#include <library.h>
#include <mint.h>
#include <xdialog.h>
#include <limits.h>
#include <ctype.h>

#include "resource.h"
#include "desk.h"
#include "error.h"
#include "xfilesys.h"
#include "lists.h"  /* must be before slider.h */
#include "slider.h" 
#include "file.h"
#include "font.h"
#include "config.h"
#include "window.h"
#include "icon.h"
#include "showinfo.h"
#include "copy.h"
#include "viewer.h"
#include "dir.h"
#include "va.h"


static XDINFO 
	sdinfo,					/* information about the search dialog */
	idinfo;					/* information about the info dialog */ 	

static boolean
	searchdopen = FALSE,	/* flag that Search... dialog is open */
	infodopen = FALSE;    	/* flag that Info... dialog is open */ 

extern boolean
	can_touch;

extern const char 
	fas[];			/* file attributes flags */

static const char
	ois[] = {ISWP, ISARCHIV, ISHIDDEN, ISSYSTEM};

/*
 * Search parameters: pattern, string, date range, size range...
 */

long
	find_offset;	/* offset of found string from file start */

int 
	search_nsm = 0;	/* number of found string matches */	

static long
	search_losize,	/* low end of file size range for search, inclusive */
	search_hisize;	/* high end of file size range for search, inclusive */

static int 
	search_lodate,	/* low end of file/folder date for search, inclusive */
	search_hidate;	/* high end of file/folder date for search, inclusive */

static size_t
	search_length;	/* length of the string being searched for */

static char 
	*search_txt,		/* string to search for (directly in dialog field) */
	*search_buf,		/* buffer to store the file to search the string in */
	*search_bufend,		/* pointer to the first byte after the search area */
	**search_finds;		/* pointers to found strings */

static VLNAME 
	search_pattern = {0}; /* store filename pattern for search */

static boolean
	nodirs,					/* TRUE if directory names are not to be searched */
	nofound = TRUE;			/* TRUE if no items founnd */


char *app_find_name(const char *fname, boolean full);


/*
 * Save some bytes in program size...
 */

static int fi_atoi(int obj)
{
	return atoi(fileinfo[obj].ob_spec.tedinfo->te_ptext);
}

/* 
 * Convert DOS time to a string to be displayed in a form. 
 * String format is hhmmss, always six characters long 
 * Terminating 0 is not added. 
 */

static void cv_ttoform(char *tstr, unsigned int time)
{
	unsigned int sec, min, hour, h;

	sec = (time & 0x1F) * 2;
	h = time >> 5;
	min = h & 0x3F;
	hour = (h >> 6) & 0x1F;
	tstr = digit(tstr, hour);
	tstr = digit(tstr, min);
	digit(tstr, sec);
}


/* 
 * Convert DOS date to a string to be displayed in a form.
 * String format id ddmmyy, always six characters long.
 * Terminating 0 is not added. 
 */

static void cv_dtoform(char *tstr, unsigned int date)
{
	unsigned int day, mon, year, h;

	day = date & 0x1F;
	h = date >> 5;
	mon = h & 0xF;
	year = ((h >> 4) & 0x7F) + 80;
	tstr = digit(tstr, day);
	tstr = digit(tstr, mon);
	digit(tstr, year);
}


/*
 * convert a string to DOS date or time.
 *
 * If ct = TRUE:
 * Convert a string (format: hhmmss) to DOS time;
 * Input string is checked: hour can be 0:23, minute 0:59, second 0:59
 *
 * If ct = FALSE:
 * Convert a string (format: ddmmyy) to DOS date; 
 * for brevity, date is only partially checked: 
 * february can be 1:29 in any year, month is 1:12, year is 0:99;
 *
 * returns -1 if invalid string is entered; 
 */

static int cv_formtodt( char *dtstr, boolean ct )
{
	int
		d_h,			/* day or hour */
		m_m,			/* month or minute */
		y_s;			/* year or second */

	char
		*ds = dtstr,	/* pointer to current position */	
		b[3];			/* temporary storage */

	static const char 	/* numbers of days in months */
		md[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if ( *ds == 0 )
		return 0;

	if(strlen(dtstr) == 6)
	{

		b[0] = *ds++;
		b[1] = *ds++;
		b[2] = 0;
		d_h = atoi(b);

		b[0] = *ds++;
		b[1] = *ds++;
		m_m = atoi(b);	

		b[0] = *ds++;
		b[1] = *ds;
		y_s = atoi(b);

		if(ct)
		{
			if ( d_h < 24 && m_m < 60 && y_s < 60 )
				return ((d_h << 11) | (m_m << 5) | (y_s / 2));
		}
		else
		{
			if ( y_s < 80 )
				y_s += 100;

			y_s -= 80;

			if ( m_m > 0 && m_m < 13 && d_h > 0 && d_h <= md[m_m] )
				return (d_h | (m_m << 5) | (y_s << 9));
		}
	}

	return -1;
} 


/* 
 * Handle dialog to set data relevant for file, folder or string search.
 * This routine does not close the dialog.
 */

static int search_dialog(void)
{
	int 
		button = 0;		/* code of pressed button */

	search_txt = searching[SGREP].ob_spec.tedinfo->te_ptext;

	/* Clear all fields */

	*search_txt = 0;				/* string pattern in the dialog */	

	*(searching[SLOSIZE].ob_spec.tedinfo->te_ptext) = 0;
	*(searching[SHISIZE].ob_spec.tedinfo->te_ptext) = 0;
	*(searching[SLODATE].ob_spec.tedinfo->te_ptext) = 0;
	*(searching[SHIDATE].ob_spec.tedinfo->te_ptext) = 0;

	nodirs = TRUE;
	set_opt( searching, options.xprefs, S_IGNCASE, IGNCASE); 
	set_opt( searching, options.xprefs, S_SKIPSUB, SKIPSUB); 

	/* Open the dialog */

	if(chk_xd_open(searching, &sdinfo) >= 0)
	{
		/* Edit data */

		while (button != SOK && button != SCANCEL)
		{
			/* Wait for the appropriate button */

			button = xd_form_do(&sdinfo, ROOT);

			if ( button == SOK )
			{
				/* 
				 * OK, get data out of dialog, but check it.
				 * Directory names will be searched for only if size range is not set
				 * and search string is not set
				 */

				cv_formtofn( search_pattern, searching, SMASK );
				search_losize = atol(searching[SLOSIZE].ob_spec.tedinfo->te_ptext);
				search_hisize = atol(searching[SHISIZE].ob_spec.tedinfo->te_ptext);

				if ( search_hisize == 0L )
				{
					search_hisize = 0x7FFFFFFFL; /* a very large size */

					if (search_losize == 0)
						nodirs = FALSE;
				}				

				search_lodate = cv_formtodt(searching[SLODATE].ob_spec.tedinfo->te_ptext, FALSE);
				search_hidate = cv_formtodt(searching[SHIDATE].ob_spec.tedinfo->te_ptext, TRUE);

				if ( search_hidate == 0 )
					search_hidate = 32671; 	     /* a very late date  */

				search_length = strlen(search_txt);

				if (search_length > 0)
					nodirs = TRUE;

				/* Check if parameters entered have sensible values */

				if 
				(  
					x_checkname(empty, search_pattern) || 
					*search_pattern == 0 ||			 /* must have a pattern */
					search_lodate == -1  ||			 /* valid low date      */ 
					search_hidate == -1  ||			 /* valid high date     */
					search_hidate < search_lodate || /* valid date range    */
					search_hisize < search_losize	 /* valid size range    */ 
				)
				{
					xd_change(&sdinfo, button, NORMAL, 1);
					alert_iprint(MINVSRCH);
					button = 0;
				}
				else
				{
					/* OK, can commence search */

					nofound = TRUE;
					get_opt( searching, &options.xprefs, S_IGNCASE, IGNCASE ); 
					get_opt( searching, &options.xprefs, S_SKIPSUB, SKIPSUB ); 	

					if ( *search_txt != 0 ) /* search for a string */
						obj_unhide(fileinfo[MATCHBOX]);
				}
			} 		/* ok? */
			else
				xd_change(&sdinfo, button, NORMAL, 1);

		} 			/* while */
	}

	return button;
}


/*
 * Find first occurence of a string in the text read from a file into buffer;
 * Currently this is a very primitive and -very- inefficient routine
 * but there's room for improvement (e.g. another basic algorithm, use of 
 * wildcards, etc). Routine returns pointer to the found string, or NULL if 
 * not found. For multiple searches, pointer to buffer has to be updated 
 * between calls, so that each new pointer points after the previous one 
 */

char *find_string
( 
	char *buffer	/* buffer in which the string is searched for */ 
)
{
	int
		(*cmpfunc)(const char *s1, const char *s2, size_t n);

	char 
		*p,			/* pointer to currently examined location              */
		*pend,		/* pointer to last location to examine (near file end) */
		t;			/* uppercase of the first char of the searched string  */	

	p = buffer;									/* start from the beginning */
	pend = search_bufend - search_length + 1;	/* go to (almost) file end */
	t = *search_txt & 0xDF;						/* almost uppercase */

	/* Select the function for string comparison */

	if ( (options.xprefs & S_IGNCASE) != 0 ) 
		cmpfunc = strnicmp;
	else
		cmpfunc = strncmp;

	while ( p < pend )
	{
		/* 
		 * If (uppercase of) the first character of the string does not
		 * match, there is no point in calling the comparison routine.
		 * Certain gain in speed is achieved in this way. Actually,
		 * *p & 0xDF is not always equal to uppercase od *p but as the
		 * same operation is performed for *t, the comparison is valid.
		 * It serves to eliminate a lot of unnecessary comparisons.
		 * Btw. 'ignore case' option does not work correctly for characters
		 * with codes in the upper half of the 256-bytes table. 
		 */

		if
		(
			((*p & 0xDF) == t) && 
			(cmpfunc(p, search_txt, search_length) == 0)
		)
			return p;	/* found */

		p++;
	}

	return NULL;	/* not found */
}


/*
 * Match file/folder data against search criteria 
 * (name, size, date, or containing string),
 * return TRUE if matched.
 * Search criteria are passed through global variables.
 */

boolean searched_found
( 
	char *path,		/* pointer to file/folder path */ 
	char *name, 	/* pointer to file/folder name */
	XATTR *attr		/* attributes of the above     */ 
)
{
	char 
		*fpath = NULL,	/* pointer to full file/folder name */
		*p;				/* local pointer to string found */

	long
		fl;				/* length of the file just read */

	int 
		error = 0;		/* error code */

	boolean 
		found = FALSE;	/* true if match found */


	/* Nothing found yet */

	search_nsm = 0;

	/* if name matched... */

	if ( (found = cmp_wildcard ( name, search_pattern )) == TRUE ) 
	{
		/* ...and date range matched... */

		if ( (found = (attr->mdate >= search_lodate && attr->mdate <= search_hidate ) ) == TRUE )
		{
			/* ...and size range matched... */

			if
			(
				(
					found = 
					(
						( ((attr->mode & S_IFMT) == S_IFDIR) && !nodirs ) || 
			    		( ((attr->mode & S_IFMT) == S_IFREG) && (attr->size >= search_losize && attr->size <= search_hisize) )
					)
				) == TRUE 
			)
			{
				/* Find string, if specified */

				if ( *search_txt != 0 )
				{
					fpath = x_makepath(path, name, &error);
					found = FALSE;

					if ( !error )
					{
						/* 
						 * First read complete file 
						 * (remember to free this buffer when finished)
                         */

						if ( (error = read_txtf( fpath,  &search_buf, &fl)) == 0)
						{
							search_bufend = search_buf + fl;

							/* 
							 * Allocate memory for pointers to found strings
							 * ( max. MFINDS of them ); 
							 * remember to free it when finished! 
							 */

							if ((search_finds = malloc_chk( (MFINDS + 2) * sizeof(char *))) != NULL)
							{
								/* Find and count all instances of searched string */

								p = search_buf;
						
								while ( p && search_nsm < MFINDS )
								{
									p = find_string(p);

									if ( p )
									{
										/* Match found! */

										found = TRUE;
										search_finds[search_nsm] = p;
										search_nsm++;

										/* 
										 * Which behaviour best to select here?
										 * if it is "p++", next search starts from
										 * the next character, e.g. if string
										 * "ABCABC" is searched in "ABCABCABCABC"
										 * 3 instances are found, otherwise
										 * only 2 instances are found- but then
										 * the search is finished sooner 
										 */

										/* p++; alternative */
										p = p + search_length;
									}
								}
							}

							/* 
							 * Free pointers to finds if nothing found 
							 * (otherwise they will be neeed later)
							 * Free the file buffer too.
							 */

							if ( search_nsm == 0 )
							{
								free(search_finds);	
								free(search_buf); 
							}
	
						} /* read textfile OK ? */

					} /* no error ? */

					xform_error(error);
					free(fpath);
  
				} 	/* search string specified ? */
			} 		/* size matched ? */
		}			/* date matched ? */
	}				/* name matched ? */

	return found;
}


/*
 * Closeinfo closes the object info  and object search dialogs
 * if they were open. Now, closeinfo must come after object_info because
 * object_info does not close the dialog after each item.
 */

void closeinfo(void)
{
	if ( infodopen )
	{
		xd_close (&idinfo);
		infodopen = FALSE;
	}
	
	if(searchdopen)
	{
		xd_change(&sdinfo, SOK, NORMAL, 0);
		xd_close(&sdinfo);
		searchdopen = FALSE;
		obj_hide(fileinfo[MATCHBOX]);	
		*search_pattern = 0;
	}
}


/*
 * Display alert: Can't read/set info for...
 */

static int si_error(const char *name, int error)
{
	return xhndl_error(MESHOWIF, error, name);
}


/* 
 * Enter state of file attributes into dialog buttons.
 * This preserves extended attribute bits 
 */

static void set_file_attribs(int attribs)
{
	int i;

	for(i = 0; i < 4; i++)
		set_opt(fileinfo, attribs, (int)fas[i], (int)ois[i]);
}


/* 
 * Get state of file attributes from dialog, change DOS attributes 
 * (preserve other attributte bits) 
 */

static int get_file_attribs(int old_attribs)
{
	int 
		i,
		attribs = (old_attribs & 0xFFD8);

	for (i = 0; i < 4; i++)
		get_opt(fileinfo, &attribs, (int)fas[i], (int)ois[i]);
	
	return attribs;
}


#if _MINT_

/*
 * Note: for the access rights to be correctly set,
 * checkbox-button indexes must agree with the order
 * of the flags specified here
 */ 

static const int rflg[]={S_IRUSR,S_IWUSR,S_IXUSR,S_ISUID,S_IRGRP,S_IWGRP,S_IXGRP,S_ISGID,S_IROTH,S_IWOTH,S_IXOTH,S_ISVTX};

/* 
 * Set item access rights in the dialog checkboxes
 */

static void set_file_rights(int mode)
{
	int i;

	for ( i = 0; i < 12; i++)
		set_opt(fileinfo, mode, rflg[i], OWNR + i);
}


/* 
 * Get access rights from the dialog; return changed rights
 */

static int get_file_rights(int old_mode)
{
	int 
		i,
		mode = old_mode;

	for ( i = 0; i < 12; i++ )
		get_opt(fileinfo, &mode, rflg[i], OWNR + i);

	return mode; 
}
#endif


/*
 * Prepare for display the surroundings of the found string, 
 * if there are any.
 * ism = instance of the string found in a  file.
 */

static void disp_smatch( int ism )
{
	int
		fld,		/* length of display field */
		nc1,		/* number of chars to display before searched string */
		nc2;		/* number of chars to display after searched string */

	char
		*s,			/* part of string to be displayed */
		*disp;		/* form string to display */


	/* Show number of matches */

	rsc_ltoftext(fileinfo, ISMATCH, (long)(ism + 1) );
	rsc_ltoftext(fileinfo, NSMATCH, (long)search_nsm);

	disp = fileinfo[MATCH1].ob_spec.tedinfo->te_ptext;	/* pointer to field */
	fld = (int)strlen(xd_pvalid(&fileinfo[MATCH1])); /* length of */

	/* Completely clear the display field (otherwise it won't work!) */

	memclr( disp, (size_t)fld );
	fld -= 5;

	/* 
	 * Try to copy to display buffer some text just before found string. 
	 * Must not be before beginning of buffer (i.e. file).
	 * Out of available length, 5 characters will be wasted
	 * for the string itself, represented by a "[...]" 
	 */

	s = search_finds[ism] - fld/ 2;

	if ( s < search_buf )
		s = search_buf;				

	nc1 = (int)(search_finds[ism] - s); /* number of characters to display */

	copy_unnull(disp, s, nc1, 0, nc1);

	/*
	 * For display, substitute the string searched for with "[...]"
	 * (which will most often be shorter, so that more of the surrounding
	 * text will be shown
	 */

	strcat(disp, "[...]");				/* substitute for searched text */	

	/* 
	 * Copy to display buffer the text just after the found string. 
	 * This will be terminated  either by field length (i.e. not more than nc2
	 * characters), or by the end of the buffer, whichever comes sooner 
	 */

	s = search_finds[ism] + search_length;
	nc2 = (int)lmin(search_bufend - s, fld - nc1); /* not more than field size */

	copy_unnull(disp + nc1 + 5, s, nc2, 0, nc2);
}


/*
 * Show or set information about one drive, folder, file or link. 
 * Return result code
 */

int object_info
(
	ITMTYPE type,		/* Item type: ITM_FOLDER, ITM_FILE, etc. */ 
	const char *oldn,	/* object path + name */ 
	const char *fname,	/* object name only (why the duplication?) */ 
	XATTR *attr			/* Object's extended attributes */
)
{
	char 
		*time,			/* time string */ 
		*date;			/* date string */ 

#if _MORE_AV
	VLNAME
		avname;			/* name to be reported to an AV client */

#if _MINT_
	VLNAME
		ltgtname;		/* link target fullname */
#endif
#endif	

	VLNAME
		nfname;			/* changed item name taken from the dialog field */	

	long 
		nfolders, 		/* number of subfolders */
		nfiles,			/* number of files   */ 
		nbytes;			/* sum of bytes used */

	int 
		tflall,			/* button text */
		drive,			/* drive id */
		error = 0,		/* error code */
		ism = 0,		/* ordinal of string search match */
		button,			/* button index */ 
#if _MINT_
		mode = 0,		/* copy access rights to temp. storage */
		fs_type,		/* filesystem characteristics flags */
		uid = 0,		/* file owner user id. */
		gid = 0,		/* file owner group id. */
#endif
		attrib = 0, 	/* copy state of attributes to temp. storage */ 
		result = 0,		/* return code of this routine */
		thetitle;		/* dialog title */

	boolean
		changed = FALSE, /* TRUE if objectinfo changed */
		qquit = FALSE;	/* TRUE to exit from dialog */ 

	DISKINFO 
		diskinfo;		/* some disk volume information */

	SNAME 
		dskl;			/* disk label */

#if _EDITLABELS
	TEDINFO
		*lblted = fileinfo[FLLABEL].ob_spec.tedinfo;
#endif

	static const int 
		items1[] = {FLLIBOX, FLFOLBOX, PFBOX, FOPWITH, FOPWTXT, 0},
		items2[] = {FLNAMBOX, ATTRBOX, RIGHTBOX, 0},
		items3[] = {FLLABBOX, FLSPABOX, FLCLSIZ, 0};


	/* In which filesystem does this item reside */

#if _MINT_
	fs_type = x_inq_xfs(oldn);
#endif

	/* Pointers to time and date fields in the dialog */

	time = fileinfo[FLTIME].ob_spec.tedinfo->te_ptext;
	date = fileinfo[FLDATE].ob_spec.tedinfo->te_ptext;

	/* Some fields are set to invisible */

	rsc_hidemany(fileinfo, items1);

	/* Put object data into dialog forms */

	if ( type != ITM_DRIVE )
	{
		attrib = attr->attr; /* copy state of attributes to temp. storage */ 
#if _MINT_
		mode = attr->mode;
		/* If noone has write access, set the file to readonly */

		if ( (mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0 )
			attrib |= FA_READONLY;
		else
			attrib &= ~FA_READONLY;
#endif
		tflall = TFIALL;
	}
	else
		tflall = TFIMORE;

	rsc_title(fileinfo, FLALL, tflall);
	obj_unhide(fileinfo[FLALL]);

#if _MORE_AV
	strsncpy(avname, oldn, sizeof(LNAME));
#endif	

	switch(type)
	{
		case ITM_FOLDER:
		{
			/* Count the numbers of items in the folder */

			error = cnt_items(oldn, &nfolders, &nfiles, &nbytes, 0x11 | options.attribs, FALSE); 
			arrow_mouse();

			if ( error != 0 )
			{
				result =  si_error(fname, error);

				/* Path can be too long in subfolder, allow change */

				if( error == EPTHTL || error == EFNTL)
				{
					error = 0;
					result = 0;
				}
				else
					return result;
			}

			rsc_ltoftext(fileinfo, FLFOLDER, nfolders);
			rsc_ltoftext(fileinfo, FLFILES, nfiles);
			rsc_ltoftext(fileinfo, FLBYTES, nbytes);

			/* 
			 * Folders could not be renamed before TOS 1.4 
			 * (But can they always be renamed in mint? I suppose so)
			 */

#if _MINT_
			if ( mint || (tos_version >= 0x104) )
#else
			if ( tos_version >= 0x104 )
#endif
				fileinfo[FLNAME].ob_flags |= EDITABLE;
			else
				fileinfo[FLNAME].ob_flags &= ~EDITABLE;

			/* Set states of some other fields */

			obj_unhide(fileinfo[FLFOLBOX]);
			rsc_title(fileinfo, FLTITLE, DTFOINF);
			rsc_title(fileinfo, TDTIME, TCRETIM);

			goto setmore;
		}
#if _MINT_
		case ITM_LINK:
		{
			rsc_title(fileinfo, FLTITLE, DTLIINF);
			obj_unhide(fileinfo[FLLIBOX]);
			memclr(ltgtname, sizeof(ltgtname) );
			ltgtname[0] = 0;
			error = x_rdlink(sizeof(ltgtname), ltgtname, oldn);

			if ( error != 0 )
			{
				result = si_error(fname, error);
				return result;
			}

			cv_fntoform(fileinfo, FLTGNAME, ltgtname);
			goto evenmore;
		}
#endif
		case ITM_PROGRAM:
		{
			thetitle = DTPRGINF;

			if (*search_pattern == 0)
			{
				char 
					*pflags = fileinfo[PFLAGS].ob_spec.tedinfo->te_ptext,
					*pprot = fileinfo[PMEM].ob_spec.tedinfo->te_ptext;
				long 
					flg;
				int
					jf;
	
				*pflags = 0;
				*pprot = 0;

				if((flg = x_pflags((char *)oldn)) >= 0)
				{
					/* 
					 * Note: strings must be in the correct sequence
					 * in the resource for the following code to work
					 */

					for ( jf = 0; jf < 3; jf++ )
					{
						if( (flg & (0x1 << jf)) != 0 )
							strcat(pflags, get_freestring(PFFLOAD + jf));
					}

					strcpy(pprot, get_freestring(PPPRIVAT + ((flg & 0x30) >> 4)));

					if( (flg & 0x1000) != 0 )
						strcat(pprot, get_freestring(PPSHARE));
				}
				else
					alert_iprint(TPLFMT);

				obj_unhide(fileinfo[PFBOX]);
			}
			goto settitle;
		}
		case ITM_FILE:
		{
#if _MINT_
			/* Handle some special 'file' objects from drive U */

			switch ( attr->mode & S_IFMT )
			{
				case S_IFCHR:
				{
					thetitle = DTDEVINF;
					break;
				}
				case S_IFIFO:
				{
					thetitle = DTPIPINF;
					break;
				}
				case S_IMEM:
				{
					thetitle = DTMEMINF;
					break;
				}
				default:

				{
#else
			{
#endif
					char *appname = app_find_name(fname, FALSE);

					if(appname && *search_pattern == 0)
					{
		 				obj_unhide(fileinfo[FOPWITH]);
		 				obj_unhide(fileinfo[FOPWTXT]);
						strcpy(fileinfo[FOPWITH].ob_spec.tedinfo->te_ptext, appname);
					}

					thetitle = DTFIINF;
#if _MINT_
					break;
				}
#endif
			}

			settitle:;

			rsc_title(fileinfo, FLTITLE, thetitle);
#if _MINT_
			evenmore:;
#endif
			rsc_title(fileinfo, TDTIME, TACCTIM);
			rsc_ltoftext(fileinfo, FLBYTES, attr->size);

			setmore:;

			strcpy ( nfname, oldn );
			path_to_disp ( nfname );
			cv_fntoform(fileinfo, FLPATH, nfname);	
			cv_fntoform(fileinfo, FLNAME, fname);
			cv_ttoform(time, attr->mtime);
			cv_dtoform(date, attr->mdate);
			rsc_hidemany(fileinfo, items3);
			obj_unhide(fileinfo[FLNAMBOX]);
#if _MINT_
			gid = attr->gid;
			uid = attr->uid;

			if ( (fs_type & FS_UID) != 0 )
			{
				/* user ids and access rights possible */
				set_file_rights(mode);
				itoa(uid, fileinfo[UID].ob_spec.tedinfo->te_ptext, 10);
				itoa(gid, fileinfo[GID].ob_spec.tedinfo->te_ptext, 10);
				obj_unhide(fileinfo[RIGHTBOX]);
				obj_hide(fileinfo[ATTRBOX]);
			}
			else
#endif
			{
				/* dos-type file attributes possible */
				set_file_attribs(attrib);
				obj_hide(fileinfo[RIGHTBOX]);
				obj_unhide(fileinfo[ATTRBOX]);
			}

			if(!can_touch || *search_pattern != 0 || va_reply)
				obj_hide(fileinfo[FLALL]);

			break;
		}
		case ITM_DRIVE:
		{
			rsc_title(fileinfo, FLTITLE, DTDRINF);

#if _EDITLABELS
#if _MINT_
			if(mint)
				fileinfo[FLLABEL].ob_flags |= EDITABLE;
#endif
#endif
			drive = (oldn[0] & 0x5F) - 'A';

			if (check_drive( drive ))
			{
				arrow_mouse();

				if 
				(
					((error = x_getlabel(drive, dskl)) == 0) &&
					((error = x_dfree(&diskinfo, drive + 1)) == 0)
				)
				{
					long 
#if _MINT_
						k = 1,
#endif
						fbytes, tbytes, clsize;

					fbytes = diskinfo.b_free,	/* number of free bytes */
					tbytes = diskinfo.b_total,	/* total number of bytes */
					clsize = diskinfo.b_secsiz * diskinfo.b_clsiz;

#if _EDITLABELS
#if _MINT_
					if((fs_type & FS_UID) != 0)
						rsc_fixtmplt(lblted, lblvalid, lbltmplt);
					else
#endif
						rsc_tostmplt(lblted);
#endif
					cv_fntoform(fileinfo, FLLABEL, dskl);
					rsc_ltoftext(fileinfo, FLCLSIZ, clsize);

					/* 
					 * Make some arrangements for partitions larger
					 * than 2GB, which can not be normally displayed
					 * because sizes do not fit into 32-bit integers.
					 * In such cases display data in kilobytes.
					 * See rsc_ltoftext() for meaning of negative parameters.
					 * This is probably relevant only in non-TOS fs
					 * i.e. mint or magic would have to be present,
					 * and so it can be excluded for the single-TOS version.
					 * This should correctly display partition sizes up to 1TB,
					 * but would be incorrect if cluster size is not a multiple
					 * of 512 bytes. 
					 */
#if _MINT_
					if( tbytes > (LONG_MAX / 2) / clsize )
					{
						clsize /= -512;
						k = 2;
					}
#endif
					tbytes *= clsize;
					fbytes *= clsize;
#if _MINT_
					tbytes /= k;
					fbytes /= k;
#endif
					nbytes = (tbytes - fbytes);	

					rsc_ltoftext(fileinfo, FLBYTES, nbytes);
					rsc_ltoftext(fileinfo, FLFREE, fbytes);
					rsc_ltoftext(fileinfo, FLSPACE, tbytes);

					fileinfo[FLDRIVE].ob_spec.tedinfo->te_ptext[0] = drive + 'A';
				}
			}
			else
				error = EDRIVE;

			if(error != 0)
				result = si_error(oldn, error);

			/* Set visibility states of other fields */

			rsc_hidemany(fileinfo, items2);
			obj_unhide(fileinfo[FLLABBOX]);
			obj_unhide(fileinfo[FLSPABOX]);
			obj_unhide(fileinfo[FLCLSIZ]);

			break;
		}
		default:
		{
			break;
		}
	}

	arrow_mouse();

	if (result != 0)
		return result;

	/* Loop until told to quit */

	while ( !qquit )	
	{
		/* Prepare to display the next string match, if any */

		if ( search_nsm > 0 )
			disp_smatch(ism);

		/* Maybe open (else redraw) the dialog;  */

		if ( !infodopen )
		{
			xd_open( fileinfo, &idinfo );
			infodopen = TRUE;
		}
		else
			xd_drawdeep(&idinfo, ROOT);

		/* Wait for a button pressed, then reset button to normal state */

		button = xd_form_do_draw(&idinfo);

		/* Get the (maybe changed) name from the dialog field */

		if ( type != ITM_DRIVE )
			cv_formtofn(nfname, fileinfo, FLNAME);

		/* 
		 * Check if date and time have sensible values; forbid exit otherwise 
		 * Note: this may be a problem with a write-protected file set
		 * to illegal date or time- it can't be unprotected!
		 * Therefore, it is permitted to keep illegal date/time on
		 * write protected files.
		 * Similar problem exists if a folder has illegal creation time/date.
		 * Such time/date can not be changed from TeraDesk!
		 */

		optime.date = cv_formtodt(fileinfo[FLDATE].ob_spec.tedinfo->te_ptext, FALSE);
		optime.time = cv_formtodt(fileinfo[FLTIME].ob_spec.tedinfo->te_ptext, TRUE);

		/* These will be needed in touch_file()... */

		now.date = Tgetdate();
		now.time = Tgettime();
		

		if ( optime.date == 0 )
			optime.date = now.date;

		if ( optime.time == 0 )
			optime.time = now.time;

		/* Check some data values */

		if 
		( 
			(type != ITM_DRIVE) &&
			(type != ITM_FOLDER) &&
			(button != FLABORT) && 
			(button != FLSKIP) && 
			((attrib & FA_READONLY) == 0 ) && 
			(optime.time == -1 || optime.date == -1 || strlen(nfname) < 1 )
		)
		{
			button = 0;
			alert_iprint(MINVSRCH);
		}

		switch(button)
		{
			case FLOK:
			{
				/* OK has been clicked; make changes */

				if ( type != ITM_DRIVE )
				{
					/* 
					 * Changes are possible only for files and folders;
					 * Information on disk volumes is currently readonly
					 */

					char *newn;
					int error = 0;
					XATTR new_attribs = *attr;
					boolean link = (type == ITM_LINK);

					hourglass_mouse();
					qquit = TRUE;

					if ( search_nsm > 0 )
						find_offset = search_finds[ism] - search_buf;
					else
						find_offset = -1L;

					/* Set states of attributes from the states of dialog buttons */
	
					new_attribs.attr = get_file_attribs(attrib);
#if _MINT_
					if ( (fs_type & FS_UID) != 0 )
					{
						new_attribs.mode = get_file_rights(mode);
						new_attribs.uid = fi_atoi(UID);
						new_attribs.gid = fi_atoi(GID);
					}
#endif
					if ((newn = fn_make_newname(oldn, nfname)) != NULL)
					{
						/* 
						 * Rename the file only if needed (name changed).
						 * If it was successful, try to change attributes.
						 * If the item is write-protected,
						 * error will be generated
						 */

						if ((strcmp(nfname, fname) != 0) && (error == 0))
							error = frename(oldn, newn, &new_attribs);

						if ( error == 0 )
						{
							changed = TRUE;
							icn_fix_ictype(); /* fix desktop icon too */
						}
#if _MINT_
						if ( link && (strcmp(tgname, ltgtname) != NULL) )
						{
							/* It should be checked here whether the target exists */

							char *tgpname = x_pathlink(tgname, newn);

							if ( tgpname )
							{
								if ( !x_netob(tgpname) && (!(x_exist(tgpname, EX_FILE) || x_exist(tgpname, EX_DIR)) ) )
									alert_iprint(TNOTGT);

								free(tgpname);
								error = x_unlink( newn );

								if ( !error )
								{
									error = x_mklink( newn, tgname );
									changed = TRUE;
								} 
							}
							else
								error = ENSMEM;
						}
#endif
						/* Act only if something has changed... */

						if
						(
							(new_attribs.attr != attrib) || 
#if _MINT_
							(new_attribs.mode != mode) ||
							(new_attribs.uid != uid) ||
							(new_attribs.gid != gid) ||
#endif
							(optime.time != attr->mtime) || 
							(optime.date != attr->mdate) 
						)
						{
							/* 
							 * Currently, setting of folder attributes is
							 * not supported in single-TOS (or Magic?)
							 */

							if 
							(
								error == 0 && 
								(
#if _MINT_
									(mint && !magx) || 
#endif
									type != ITM_FOLDER
								) 
							)
							{
								/* 
								 * If the file is set to readonly, it must first
								 * be reset before other changes are made
								 */

								if 
								( 
									(attrib & FA_READONLY) && 
									( 
										( (attrib ^ new_attribs.attr) != FA_READONLY) || 
										(optime.date != attr->mdate) || 
										(optime.time != attr->mtime) 
									) 
								)
									error = EACCDN;
								else
								{
									changed = TRUE;
									error = touch_file(newn, &optime, &new_attribs, link);								
								}
							}
						}		/* changed */

						if (error != 0)
						{
							arrow_mouse();
							result = si_error(fn_get_name(newn), error);
							hourglass_mouse();
						}

						if ((result != XFATAL) && changed)
							wd_set_update(WD_UPD_COPIED, oldn, NULL);
#if _MORE_AV
						strsncpy(avname, newn, sizeof(LNAME));
#endif
						free(newn);
					}

					arrow_mouse();
				}
				else /* for the disk volume */
				{
#if _EDITLABELS
					SNAME ndskl;
#endif
					/* result = XABORT; Better to skip ? */
					result = XSKIP;
#if _EDITLABELS

#if _MINT_
					cv_formtofn(ndskl, fileinfo, FLLABEL);

					/* 
					 * Unfortunately label-handling functions in mint
					 * and magic appears to have different behaviour. In mint,
					 * there should not be the dot in the label on FAT fs, and
					 * also, there should be no blanks. At least the dot
					 * is handled here.
					 */
 
					if(!magx && ((fs_type & FS_UID) == 0) )
					{
						char *r = strchr(ndskl, '.');
						if(r)
							strsncpy(r, r + 1, 4);
					}

					if(strcmp(ndskl, dskl) != 0)
						if(x_putlabel(drive, ndskl) != 0)
							result = XABORT;
#endif

#endif
					qquit = TRUE;
				}

				nofound = FALSE;
				break;
			}
			case FLABORT:
			{
				result = XABORT;
				nofound = FALSE;
				qquit = TRUE;
				break; 		
			}
			case FLSKIP:
			{
				ism++;

				if ( ism >= search_nsm )
				{
					result = XSKIP;
					qquit = TRUE;
				}
				break;
			}
			case FLALL:
			{
				if(type == ITM_DRIVE)
				{
					error = cnt_items(oldn, &nfolders, &nfiles, &nbytes, 0x11 | options.attribs, FALSE);
					arrow_mouse();

					if(error != 0)
					{
						result = si_error(oldn, error);

						/* Path can be too long in a folder, allow change */

						if( error == EPTHTL || error == EFNTL)
						{
							error = 0;
							result = 0;
						}
						else
							return result;
					}

					rsc_ltoftext(fileinfo, FLFOLDER, nfolders);
					rsc_ltoftext(fileinfo, FLFILES, nfiles);

					obj_unhide(fileinfo[FLFOLBOX]);	/* number of folders and files */
					obj_hide(fileinfo[FLALL]);
				}
				else
				{
					opattr = get_file_attribs(attrib);
#if _MINT_
					opmode = get_file_rights(mode);
					opuid = fi_atoi(UID);
					opgid = fi_atoi(GID);
#endif
					result = XALL;
					qquit = TRUE;
				}
			}
			default:
			{
				break;
			}
		} /* switch(button) */

#if _MORE_AV
		if ( va_reply && avname )
			va_add_name(type, avname);
#endif
	}

	/* Free buffers used for string search */

	if ( search_nsm > 0 )
	{
		free( search_buf );			
		free( search_finds );
		search_nsm = 0;
	}

	return result;
}


/* 
 * Show information on a list of drives, folders and/or files 
 */

void item_showinfo
(
	WINDOW *w,		/* pointer to the window in which the objects are */ 
	int n,			/* number of objects to show */ 
	int *list,		/* list of selected object indices */ 
	boolean search 	/* true if search results are displayed */
) 
{
	const char 
		*path,		/* full name of an item */ 
		*name;		/* name of an item */

	long 
		nd, 		/* number of directories */
		nf, 		/* number of files */
		nb;			/* number of bytes */

	XATTR 
		attrib;		/* extended item attributes */

	int 
		*item,		/* dir.index of the current item in the list */ 
#if _MINT_
		whandle,	/* saved top window handle */
		wap_id,		/* saved owner of the top window */
#endif
		i,			/* item counter */ 
		error,	 	/* error code */
		result;		/* return code: XABORT, XSKIP, XALL... */

	ITMTYPE 
		type;		/* type of the current item */


	/* Some settings if search is specified... */

#if _MINT_
	wd_restoretop(0, &whandle, &wap_id); /* remember the top window */
#endif

	error = 0;
	result = 0;
	*search_pattern = 0;
	nofound = FALSE;
	item = list;

	/* Open a dialog to input search parameters */

	if (search)
	{
		if(search_dialog() != SOK )
		{
			xd_close(&sdinfo);
			return;
		}

		searchdopen = TRUE;
	}

	infodopen = FALSE; /* Showinfo dialog is currently closed */ 	

	/* Proceed, for each item in the list */

	for (i = 0; i < n; i++)
	{
		type = itm_type(w, *item);

		if ( isfile(type) ||  (type == ITM_DRIVE) )
		{
			if ((path = itm_fullname(w, *item)) == NULL)
				result = XFATAL;
			else
			{
				if ( search ) 
				{
					if ( (result = cnt_items( path, &nd, &nf, &nb, 0x1 | options.attribs, search ) ) != XSKIP && result != 0)
						break;
				}
				else
				{
#if _MINT_
					if ( itm_islink(w, *item) )
						type = ITM_LINK;
#endif
					if (type == ITM_DRIVE)
						result = object_info(ITM_DRIVE, path, NULL, NULL);
					else
					{
						name = itm_name(w, *item);
						hourglass_mouse();
						error = itm_attrib(w, *item, (type == ITM_LINK ) ? 1 : 0 , &attrib);
						arrow_mouse();

						if (error != 0)
							result = si_error(name, error);
						else
							result = object_info( type, path, name, &attrib );
					}
				}
				free(path);
			}
		}
		else
		{
			alert_cantdo(trash_or_print(type), MNOOPEN);
			result = XSKIP;	/* trash, printer, network */
		}

		if (mustabort(result) || (result == XALL) )
			break;

		item++;
	}

	/* 
	 * Close the 'Info...' and Search dialogs, 
	 * if they were open (tested inside) 
	 */

	closeinfo();

	/* Touch files, if so said */

	if ( result == XALL )
	{
		int ntouch = n - i;

		itmlist_wop(w, ntouch, &list[i], CMD_TOUCH);

		if ( xw_type(w) == DIR_WIND )
			dir_refresh_wd((DIR_WINDOW *)w);

		can_touch = FALSE;
	}
	else
	{
		wd_do_update();

		if (search && nofound)
			alert_iprint(MSRFINI);
	}
	
	/* Restore the top window if this was caused by an AV client */

#if _MINT_
	if(va_reply)
		wd_restoretop(1, &whandle, &wap_id);
#endif

	arrow_mouse (); 
}



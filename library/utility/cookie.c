/*
 * Utility functions for Teradesk. Copyright 1993 - 2002  W. Klaren,
 *                                           2002 - 2009  H. Robbers,
 *                                           2003 - 2009  Dj. Vukovic
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


#include <library.h>
#include <mint/ssystem.h>

#undef p_cookie
#define p_cookie	( * (COOKIE **) 0x5A0L )


/*
 * return cookie value or return -1 if cookie not found.
 * If ssystem function is available (i.e. in Mint 1.15 or more)
 * then use that function.
 * Note: earlier version of this routine erronelosly returned
 * input value of "name" instead of -1 if there was no cookie jar
 */

bool find_cookie(long name, long *value)
{
	COOKIE *cookie;
	long cvalue = 0;

#if _MINT_
	if (have_ssystem)
	{
		if (Ssystem(S_GETCOOKIE, name, (long) &cvalue) != 0)
			return FALSE;
	} else
#endif
	{
		/*
		 * cookie jar ptr is longword aligned, thus
		 * we can use Setexc to fetch its value
		 */
		cookie = (COOKIE *)Setexc(0x5A0 / 4, (void (*)())-1);

		if (cookie == NULL)
			return FALSE;
		while (cookie->name != 0 && cookie->name != name)
			cookie++;

		if (cookie->name == 0)
			return FALSE;
		cvalue = cookie->value;
	}

	if (value)
		*value = cvalue;
	return TRUE;
}


#if 0									/* All following routines are currently not used in Teradesk */

long o_resvalid;

void (*o_resvector) (void);

#if __AHCC__
void __asm__ jmpa6(void)
{
	addq.l	#4, sp			; adjust stack, jmpa6 is called
	jmp 	(a6)
}
#else
void jmpa6(void) 0x4ED6;
#endif


static void cookie_reset(void)
{
	p_cookie = NULL;
	resvector = o_resvector;
	resvalid = o_resvalid;
	jmpa6();
}


/* 
 * r = -1 : fout,
 * r =  0 : geen fout,
 * r =  1 : nieuw cookie geinstalleerd, reset vast,
 * r =  2 : nieuw cookie geinstalleerd, niet reset vast. 
 */

_WORD install_cookie(long name, long value, COOKIE *buffer, long l)
{
	void *stack;
	COOKIE *cookie;
	_WORD r, i, j;

	stack = (void *) Super(NULL);

	if ((cookie = p_cookie) != NULL)
	{
		i = 0;

		while (cookie[i].name != 0L)
			i++;

		if (cookie[i].value > (i + 1))
		{
			cookie[i + 1] = cookie[i];
			cookie[i].name = name;
			cookie[i].value = value;

			r = 0;
		} else
		{
			if (l <= cookie[i].value || buffer == NULL)
			{
				r = -1;
			} else
			{
				_WORD e;				/* reserve 'end' for language */

				e = (_WORD) cookie[i].value - 1;

				for (j = 0; j < e; j++)
					buffer[j] = cookie[j];

				p_cookie = buffer;

				cookie[i].name = name;
				cookie[i].value = value;
				cookie[i + 1].name = 0L;
				cookie[i + 1].value = l;

				r = 1;
			}
		}
	} else
	{
		if ((l < 2) || (buffer == NULL))	/* cookie te klein */
		{
			r = -1;
		} else
		{
			o_resvalid = resvalid;
			o_resvector = resvector;
			resvalid = 0x31415926L;
			resvector = cookie_reset;

			p_cookie = buffer;

			buffer[0].name = name;
			buffer[0].value = value;
			buffer[1].name = 0L;
			buffer[1].value = l;

			r = 1;
		}
	}

	Super(stack);

	return r;
}
#endif

/*--------------------------------------------------------------------
 *	$Id$
 *
 *	Copyright (c) 1991-2016 by P. Wessel, W. H. F. Smith, R. Scharroo, J. Luis and F. Wobbe
 *	See LICENSE.TXT file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation; version 3 or any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/

/*
 * Allow rasterization of PostScript in memory.  No GMT code is used.
 * This code will produce a loadable plugin library with the gsrasterize
 * module that can optionally be called from psconvert.  Since the GS
 * code is GPL we must separate gsrasterize from the GMT code.  We
 * supply two functions in this plugin library:
 *
 * gsrasterize_rip  :	Accept a PS string and return an image
 * gsrasterize_free :	Free the image produced by gsrasterize_rip
 *
 * Author:	Joaquim Luis
 * Date:	17-MAR-2016
 * Version:	5.3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#	include "ierrors.h"
#	include "iapi.h"
#	include <process.h>
#	include <io.h>
#	define dup2 _dup2
#	define fileno _fileno
#	define close _close
#	define stat _stat
#else
#	include "ghostscript/ierrors.h"
#	include "ghostscript/iapi.h"
#	include <unistd.h>
#endif

/* This macro is equal to the one in GMT but is reproduced here because we are NOT linking
   against the GMT lib so that no GPL contamination can pass through */
#	ifdef _WIN32
#		ifdef LIBRARY_EXPORTS
#			define LIBSPEC __declspec(dllimport)
#		else
#			define LIBSPEC __declspec(dllexport)
#		endif /* ifdef LIBRARY_EXPORTS */
#	else /* ifdef _WIN32 */
#		define LIBSPEC
#	endif /* ifdef _WIN32 */

/* By default, we use the standard "extern" declarations. */
#	define EXTERN_MSC_ extern LIBSPEC

#ifndef I_AM_MAIN
EXTERN_MSC_ unsigned char *gsrasterize_rip (char *ps, int dpi, int dim[]);
EXTERN_MSC_ void gsrasterize_free (char *img);
#endif

char *ps_test = "newpath\n"
"100 600 moveto 200 650 lineto 100 700 lineto\n"
"closepath\n"
"gsave\n"
"0.5 setgray\n"
"fill\n"
"grestore\n"
"4 setlinewidth\n"
"stroke\n"
"/Times-Roman findfont\n"
"18 scalefont\n"
"setfont\n"
"newpath\n"
"150 600 moveto\n"
"(Julia-Ghostscript in action) show\n"
"/csquare {\n"
   "newpath\n"
   "0 26 moveto\n"
   "0 1 rlineto\n"
   "1 0 rlineto\n"
   "0 -1 rlineto\n"
   "closepath\n"
   "setrgbcolor\n"
   "fill\n"
"} def\n"
"\n"
"20 20 scale\n"
"\n"
"1 5 translate\n"
"1 0 0 csquare\n"
"\n"
"1 0 translate\n"
"0 1 0 csquare\n"
"\n"
"1 0 translate\n"
"0 0 1 csquare\n"
"showpage";

void gsrasterize_free (char *img) {
	/* Free memory allocated in this file. Needed because the cross boundary dll on Windows */
	if (img) free (img);
}

/* stdio functions */
static int GSDLLCALL gsdll_stdin (void *instance, char *buf, int len) {
	return 0;
}

static int GSDLLCALL gsdll_stdout (void *instance, const char *str, int len) {
	//fwrite(str, 1, len, fp);
	//fflush(fp);
	return len;
}

static int GSDLLCALL gsdll_stderr (void *instance, const char *str, int len) {
	fwrite (str, 1, len, stderr);
	fflush (stderr);
	return len;
}

#define ROWS  0
#define COLS  1
#define BANDS 2

/* The I_AM_MAIN brach is initially intended as a dev/debug feature, where we can build this
   as a stand alone program and send for example a PS file name.
*/

#ifdef I_AM_MAIN
int main (int argc, char *argv[]) {
	int   dpi = 300;
	int   dim[3];			/* Not meant to be used for now */
	int   arg_c = argc;
	char *ps = NULL;
#else
unsigned char *gsrasterize_rip (char *ps, int dpi, int dim[]) {
	int   arg_c = 0;
#endif
	int GScode = 0, GSexitCode, n, fd[2] = {0, 0};
	size_t size;
	char buf[1025], t[16] = {""}, s_dpi[16] = {"-r300x300"}, *ps_t;
	unsigned char *img = NULL;
	void *GSinst = NULL; 
	FILE *fp = NULL;

	char *GSargs[] = {	/* Presumably need to specify dpi at some point */
		"", "-sDEVICE=jpeg", "-dBATCH", "-dNOPAUSE", "-sOutputFile=lixo.jpg", "-q", NULL
	};
	char line[BUFSIZ];
	struct stat F;

#ifdef I_AM_MAIN
	//char line[BUFSIZ];
	//struct stat F;

	if (arg_c > 1) {
		stat (argv[1], &F);
		if ((fp = fopen (argv[1], "r")) == NULL) {
			fprintf(stderr, "Error reading PS %s file. Bye bye\n", argv[1]);
			return -1;
		}
		fprintf (stderr, "PS file size is %d bytes\n", (int)F.st_size);
		ps = (char *)calloc(F.st_size+1, sizeof(char));
		while (fgets (line, BUFSIZ, fp))
			strcat(ps, line);
	}
	else {
		ps = ps_test;
	}
#endif

	if (dpi > 0) sprintf (s_dpi, "-r%dx%d", dpi, dpi); else sprintf (s_dpi, "-r300x300");
	GSargs[6] = s_dpi;

	if ((GScode = gsapi_new_instance (&GSinst, NULL )) < 0 ) {
		fprintf (stderr, "Error: GhostScript gsapi_new_instance failed (error code %d)\n", GScode);
#ifdef I_AM_MAIN
		if (arg_c > 1) free (ps);
		return -1;
#else
	return NULL;
#endif
	}

	gsapi_set_stdio (GSinst, gsdll_stdin, gsdll_stdout, gsdll_stderr);

	if ((GScode = gsapi_init_with_args (GSinst, 6, GSargs)) < 0 ) {
		fprintf (stderr, "Error: GhostScript gsapi_init_with_args failed (error code %d)\n", GScode);
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
#ifdef I_AM_MAIN
		if (arg_c > 1) free (ps);
		return -1;
#else
		return NULL;
#endif
	}

	stat ("c:\\v\\lixo.ps", &F);
	if ((fp = fopen ("c:\\v\\lixo.ps", "r")) == NULL) {
		fprintf(stderr, "Error reading PS %s file. Bye bye\n", "c:\\v\\lixo.ps");
		return -1;
	}
	ps_t = (char *)calloc(F.st_size+1, sizeof(char));
	while (fgets (line, BUFSIZ, fp))
		strcat(ps_t, line);
	if (gsapi_run_string (GSinst, ps_t, 0, &GSexitCode) < 0)
		fprintf (stderr, "Error: GhostScript gsapi_run_string failed (error code %d)\n", GSexitCode);
	free (ps_t);

	if ((n = gsapi_run_string (GSinst, ps, 0, &GSexitCode)) < 0) {
		fprintf (stderr, "Error: GhostScript gsapi_run_string failed (error code %d\t%d)\n", GSexitCode, n);
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
#ifdef I_AM_MAIN
		if (arg_c > 1) free (ps);
		return -1;
#else
		return NULL;
#endif
	}

}

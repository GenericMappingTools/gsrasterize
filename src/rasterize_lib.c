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

EXTERN_MSC_ unsigned char *gsrasterize_rip (char *ps, int dpi, int dim[]);
EXTERN_MSC_ void gsrasterize_free (char *img);

void gsrasterize_free (char *img) {
	/* Free memory allocated in this file. Needed because the cross boundary dll on Windows */
	if (img) free (img);
}

/* stdio functions */
static int GSDLLCALL gsdll_stdin (void *instance, char *buf, int len) {
	return 0;
}

static int GSDLLCALL gsdll_stdout (void *instance, const char *str, int len) {
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

unsigned char *gsrasterize_rip (char *ps, int dpi, int dim[]) {
	int   arg_c = 0;
	int GScode = 0, GSexitCode, n, fd[2] = {0, 0};
	size_t size;
	char buf[1025], t[16] = {""}, s_dpi[16] = {"-r300x300"}, *ps_t;
	unsigned char *img = NULL;
	void *GSinst = NULL; 
	FILE *fp = NULL;

	char *GSargs[] = {	/* Presumably need to specify dpi at some point */
		"", "-sDEVICE=ppmraw", "-dBATCH", "-dNOPAUSE", "-sOutputFile=-", "-q", NULL
	};
	char line[BUFSIZ];
	struct stat F;

	if (dpi > 0) sprintf (s_dpi, "-r%dx%d", dpi, dpi); else sprintf (s_dpi, "-r300x300");
	GSargs[6] = s_dpi;

	if ((GScode = gsapi_new_instance (&GSinst, NULL )) < 0 ) {
		fprintf (stderr, "Error: GhostScript gsapi_new_instance failed (error code %d)\n", GScode);
		return NULL;
	}

	gsapi_set_stdio (GSinst, gsdll_stdin, gsdll_stdout, gsdll_stderr);

	if ((GScode = gsapi_init_with_args (GSinst, 6, GSargs)) < 0 ) {
		fprintf (stderr, "Error: GhostScript gsapi_init_with_args failed (error code %d)\n", GScode);
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
		return NULL;
	}

	/* Establish a pipe to read from stdout */
#ifdef _WIN32
	if (_pipe(fd, 145227600, O_BINARY) == -1) {
		fprintf(stderr, "Error: failed to open the pipe\n");
		gsapi_exit(GSinst);
		gsapi_delete_instance(GSinst);
		return NULL;
	}
#else
	pipe (fd);
#endif

	if (dup2 (fd[1], fileno (stdout)) < 0) {	/* Failed to duplicate pipe */
		fprintf (stderr, "Error: Failed to duplicate pipe\n");
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
		return NULL;
	}
	close (fd[1]); 		/* Close original write end of pipe */

	if ((n = gsapi_run_string (GSinst, ps, 0, &GSexitCode)) < 0) {
		fprintf (stderr, "Error: GhostScript gsapi_run_string failed (error code %d\t%d)\n", GSexitCode, n);
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
		return NULL;
	}

	/* Successful so far so let us read the raw pbm image into memory */
	
	n = read (fd[0], buf, 3U);				/* Consume first header line */
	while (read (fd[0], buf, 1U) && buf[0] != '\n') 	/* OK, by the end of this we are at the end of second header line */
		fprintf(stderr, "%c", buf[0]);
	n = 0;
	while (read(fd[0], buf, 1U) && buf[0] != ' ') 		/* Get string with number of columns from 3rd header line */
		t[n++] = buf[0];
	dim[COLS] = atoi (t);
	n = 0;
	while (read(fd[0], buf, 1U) && buf[0] != '\n') 		/* Get string with number of rows from 3rd header line */
		t[n++] = buf[0];
	t[n] = '\0';						/* Make sure no character is left from previous usage */

	while (read(fd[0], buf, 1U) && buf[0] != '\n')		/* Consume fourth header line */
		fprintf(stderr, "%c", buf[0]);

	dim[ROWS] = atoi (t);
	dim[BANDS] = 3;	/* This might change if we do monochrome at some point */

	size = dim[ROWS] * dim[COLS] * dim[BANDS];	/* Determine number of bytes needed to hold image */
	if ((img = malloc (size)) == NULL) {
		fprintf (stderr, "Error: Unable to allocate space for image [%d bytes]\n", (int)size);
		gsapi_exit (GSinst);
		gsapi_delete_instance (GSinst);
		return NULL;
	}
	/* Read the image */
	if ((n = read (fd[0], img, size)) >= 0) {
		fprintf (stderr, "\nread %d bytes from the pipe:\n", n);
	}
	else
		perror ("read");

	/* Clean up and return image */
	
	gsapi_exit (GSinst);
	gsapi_delete_instance (GSinst);
	return img;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main (int argc, char *argv[]) {
	int   dim[3];
	unsigned char *img = NULL;
	char *ps = NULL;
	char line[BUFSIZ];
	void *GSinst = NULL; 
	FILE *fp = NULL;
	struct stat F;

	stat (argv[1], &F);
	if ((fp = fopen (argv[1], "r")) == NULL) {
		fprintf(stderr, "Error reading PS %s file. Bye bye\n", argv[1]);
		return -1;
	}
	fprintf (stderr, "PS file size is %d bytes\n", (int)F.st_size);
	ps = (char *)calloc(F.st_size+1, sizeof(char));
	while (fgets (line, BUFSIZ, fp))
		strcat(ps, line);

	img = gsrasterize_rip(ps, 300, dim);
	free(ps);

	return 0;
}

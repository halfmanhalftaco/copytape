
/*
 * COPYTAPE.C
 *
 * This program duplicates magnetic tapes, preserving the
 * blocking structure and placement of tape marks.
 *
 * This program was updated at
 *
 *	U.S. Army Artificial Intelligence Center
 *	HQDA (Attn: DACS-DMA)
 *	Pentagon
 *	Washington, DC  20310-0200
 *
 *	Phone: (202) 694-6900
 *
 **************************************************
 *
 *	THIS PROGRAM IS IN THE PUBLIC DOMAIN
 *
 **************************************************
 *
 * July 1986		David S. Hayes
 *		Made data file format human-readable.
 *
 * April 1985		David S. Hayes
 *		Original Version.
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/file.h>

extern int      errno;

#define BUFLEN		262144	/* max tape block size */
#define TAPE_MARK	-100	/* return record length if we read a
				 * tape mark */
#define END_OF_TAPE	-101	/* 2 consecutive tape marks */
#define FORMAT_ERROR	-102	/* data file munged */

int             totape = 0,	/* treat destination as a tape drive */
                fromtape = 0;	/* treat source as a tape drive */

int             verbose = 0;	/* tell what we're up to */

char           *source = "stdin",
               *dest = "stdout";

char            tapebuf[BUFLEN];

main(argc, argv)
    int             argc;
    char           *argv[];
{
    int             from = 0,
                    to = 1;
    int             len;	/* number of bytes in record */
    int             skip = 0;	/* number of files to skip before
				 * copying */
    unsigned int    limit = 0xffffffff;
    int             i;
    struct mtget    status;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
	switch (argv[i][1]) {
	  case 's':		/* skip option */
	    skip = atoi(&argv[i][2]);
	    break;

	  case 'l':
	    limit = atoi(&argv[i][2]);
	    break;

	  case 'f':		/* from tape option */
	    fromtape = 1;
	    break;

	  case 't':		/* to tape option */
	    totape = 1;
	    break;

	  case 'v':		/* be wordy */
	    verbose = 1;
	    break;

	  default:
	    fprintf(stderr, "usage: copytape [-f] [-t] [-lnn] [-snn] [-v] from to\n");
	    exit(-1);
	}
    }

    if (i < argc) {
	from = open(argv[i], O_RDONLY);
	source = argv[i];
	if (from == -1) {
	    perror("copytape: input open failed");
	    exit(-1);
	}
	i++;;
    }
    if (i < argc) {
	to = open(argv[i], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	dest = argv[i];
	if (to == -1) {
	    perror("copytape: output open failed");
	    exit(-1);
	}
	i++;
    }
    if (i < argc)
	perror("copytape: extra arguments ignored");

    /*
     * Determine if source and/or destination is a tape device. Try to
     * issue a magtape ioctl to it.  If it doesn't error, then it was a
     * magtape. 
     */

    errno = 0;
    ioctl(from, MTIOCGET, &status);
    fromtape |= errno == 0;
    errno = 0;
    ioctl(to, MTIOCGET, &status);
    totape |= errno == 0;
    errno = 0;

    if (verbose) {
	fprintf(stderr, "copytape: from %s (%s)\n",
		source, fromtape ? "tape" : "data");
	fprintf(stderr, "          to %s (%s)\n",
		dest, totape ? "tape" : "data");
    }

    /*
     * Skip number of files, specified by -snnn, given on the command
     * line. This is used to copy second and subsequent files on the
     * tape. 
     */

    if (verbose && skip) {
	fprintf(stderr, "copytape: skipping %d input files\n", skip);
    }
    for (i = 0; i < skip; i++) {
	do {
	    len = input(from);
	} while (len > 0);
	if (len == FORMAT_ERROR) {
	    perror(stderr, "copytape: format error on skip");
	    exit(-1);
	};
	if (len == END_OF_TAPE) {
	    fprintf(stderr, "copytape: only %d files in input\n", i);
	    exit(-1);
	};
    };

    /*
     * Do the copy. 
     */

    len = 0;
    while (limit && !(len == END_OF_TAPE || len == FORMAT_ERROR)) {
	do {
	    do {
		len = input(from);
		if (len == FORMAT_ERROR)
		    perror("copytape: data format error - block ignored");
	    } while (len == FORMAT_ERROR);

	    output(to, len);

	    if (verbose) {
		switch (len) {
		  case TAPE_MARK:
		    fprintf(stderr, "  copied MRK\n");
		    break;

		  case END_OF_TAPE:
		    fprintf(stderr, "  copied EOT\n");
		    break;

		  default:
		    fprintf(stderr, "  copied %d bytes\n", len);
		};
	    };
	} while (len > 0);
	limit--;
    }
    exit(0);
}


/*
 * Input up to 256K from a file or tape. If input file is a tape, then
 * do markcount stuff.  Input record length will be supplied by the
 * operating system. 
 */

input(fd)
    int             fd;
{
    static          markcount = 0;	/* number of consecutive tape
					 * marks */
    int             len,
                    l2,
                    c;
    char            header[40];

    if (fromtape) {
	len = read(fd, tapebuf, BUFLEN);
	switch (len) {
	  case -1:
	    perror("copytape: can't read input");
	    return END_OF_TAPE;

	  case 0:
	    if (++markcount == 2)
		return END_OF_TAPE;
	    else
		return TAPE_MARK;

	  default:
	    markcount = 0;		/* reset tape mark count */
	    return len;
	};
    }
    /* Input is really a data file. */
    l2 = read(fd, header, 5);
    if (l2 != 5 || strncmp(header, "CPTP:", 5) != 0)
	return FORMAT_ERROR;

    l2 = read(fd, header, 4);
    if (strncmp(header, "BLK ", 4) == 0) {
	l2 = read(fd, header, 7);
	if (l2 != 7)
	    return FORMAT_ERROR;
	header[6] = '\0';
	len = atoi(header);
	l2 = read(fd, tapebuf, len);
	if (l2 != len)
	    return FORMAT_ERROR;
	read(fd, header, 1);	/* skip trailing newline */
    } else if (strncmp(header, "MRK\n", 4) == 0)
	return TAPE_MARK;
    else if (strncmp(header, "EOT\n", 4) == 0)
	return END_OF_TAPE;
    else
	return FORMAT_ERROR;

    return len;
}


/*
 * Copy a buffer out to a file or tape. 
 *
 * If output is a tape, write the record.  A length of zero indicates that
 * a tapemark should be written. 
 *
 * If not a tape, write len to the output file, then the buffer.  
 */

output(fd, len)
    int             fd,
                    len;
{
    struct mtop     op;
    char            header[20];

    if (totape && (len == TAPE_MARK || len == END_OF_TAPE)) {
	op.mt_op = MTWEOF;
	op.mt_count = 1;
	ioctl(fd, MTIOCTOP, &op);
	return;
    }
    if (!totape) {
	switch (len) {
	  case TAPE_MARK:
	    write(fd, "CPTP:MRK\n", 9);
	    break;

	  case END_OF_TAPE:
	    write(fd, "CPTP:EOT\n", 9);
	    break;

	  case FORMAT_ERROR:
	    break;

	  default:
	    sprintf(header, "CPTP:BLK %06d\n", len);
	    write(fd, header, strlen(header));
	    write(fd, tapebuf, len);
	    write(fd, "\n", 1);
	}
    } else
	write(fd, tapebuf, len);
}


#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "kbad.h"

main(int argc, char *argv[])
{
    int fd;
    int i;

    if ((fd = open("/devices/pseudo/kbad@0:kbad", O_RDWR)) < 0) {
	    perror("open failed");
	    exit(1);
    }

    if (ioctl(fd, BAD_KBAD, 0) == -1) {
	    perror("ioctl failed");
	    exit(1);
    }
    exit(0);
}



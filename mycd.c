#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <stdio.h>
#include <linux/cdrom.h>

static char *device = "/dev/cdrom";

int main(int argc, char *argv[])
{
	int cmd, fh;
	if (argc!=2 || (strcmp(argv[1],"start") && strcmp(argv[1], "stop")))
	{
		fprintf(stderr, "Useage: mycd [ start | stop ]\n");
		exit(-1);
	}
	if (strcmp(argv[1], "start")==0) cmd = CDROMSTART;
	else cmd = CDROMSTOP;
	if ((fh = open(device, O_RDONLY)) >= 0)
	{
/*		ioctl(fh, cmd);*/
		ioctl(fh, CDROMRESUME);
		close(fh);
	}
	return 0;
}


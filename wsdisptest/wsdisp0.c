#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <dev/wscons/wsconsio.h>

#define WSDISPDEV "/dev/ttyE0"

int main() {

	int i;
	int mode;
	int wd, x, y;
	uint32_t *vram, *lp,*p;
	struct wsdisplayio_fbinfo fbinfo;
	
	if ((wd=open(WSDISPDEV, O_RDWR))<0)
		err(1,"open");

	mode =  WSDISPLAYIO_MODE_DUMBFB;
	if(ioctl(wd, WSDISPLAYIO_SMODE, &mode))
		err(1,"ioctl SMODE");

	if (ioctl(wd, WSDISPLAYIO_GET_FBINFO,&fbinfo)==-1) {
		perror("ioctl FBINFO");
	} else {

		if ((vram=(uint32_t *)mmap(0, fbinfo.fbi_fbsize,
			PROT_READ|PROT_WRITE, MAP_SHARED,wd,0))==MAP_FAILED) {
			perror("ioctl FBINFO");
		} else {
			lp=vram+fbinfo.fbi_fboffset/(fbinfo.fbi_bitsperpixel/8);

			for (y=0; y<fbinfo.fbi_height; y++) {
				for (x=0,p=lp; x<fbinfo.fbi_width;x++,p++)
					*p=0xffffff;
				lp+=fbinfo.fbi_stride/(fbinfo.fbi_bitsperpixel/8);
			}
			sleep(10);
		}
	}
	mode =  WSDISPLAYIO_MODE_EMUL;
	ioctl(wd, WSDISPLAYIO_SMODE, &mode);
	close(wd);
}


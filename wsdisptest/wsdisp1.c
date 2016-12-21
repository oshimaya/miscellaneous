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

struct wsdisplayio_fbinfo fbinfo;
int bypp;
int wd;
uint32_t *vram=(uint32_t *)-1;

int setBitmapmode() {
	int x =  WSDISPLAYIO_MODE_DUMBFB;
	return ioctl(wd, WSDISPLAYIO_SMODE, &x);
}

int setTextmode() {
	int x =  WSDISPLAYIO_MODE_EMUL;
	return ioctl(wd, WSDISPLAYIO_SMODE, &x);
}

void terminate(int code, char *msg) {
	int o_errno = errno;
	setTextmode();
	close(wd);
	errno=o_errno;
	if ( vram != (uint32_t *)-1 )
		munmap(vram, fbinfo.fbi_fbsize);
	if  ( code != 0 )
		perror(msg);
	exit(code);
}

int checkfbinfo(){
	/*
         * 手抜きなので RGBダイレクトカラーのみ
         */
	if (fbinfo.fbi_pixeltype != WSFB_RGB)
		return -1;

	/*
         * 手抜きなので 32bitカラーのみ。16/24bitの環境の人すいません
         */
	if (fbinfo.fbi_bitsperpixel != 32)
		return -2;
	return 0;
}

void getFBinfo() {

	if (ioctl(wd, WSDISPLAYIO_GET_FBINFO,&fbinfo)==-1)
		terminate(1, "ioctl FBINFO");
	switch(checkfbinfo(fbinfo)){
	case 0:
		break;
	case -1:
		errno=EOPNOTSUPP;
		terminate(1, "FBinfo type");
		break;
	case -2:
		errno=EINVAL;
		terminate(1, "FBinfo depth");
		break;
	default:
		errno=EINVAL;
		terminate(1, "FBinfo");
		break;
	}
	bypp = fbinfo.fbi_bitsperpixel/8;
}


uint32_t pixrgb(int r, int g, int b) {

	/*
         * 入力階調は8bitなのでマスクする
         */
	r &= 0xff;
	g &= 0xff;
	b &= 0xff;

	/*
         * 表示bit数が8に満たない場合(このプログラムでは未対応だけど)
         * それだけshiftさせて階調を落とす
         */
	r >>= 8-fbinfo.fbi_subtype.fbi_rgbmasks.red_size;
	g >>= 8-fbinfo.fbi_subtype.fbi_rgbmasks.green_size;
	b >>= 8-fbinfo.fbi_subtype.fbi_rgbmasks.blue_size;

	/*
         * それぞれoffsetにしたがって書き込む32bitデータに結合する
         */
	return (r<<fbinfo.fbi_subtype.fbi_rgbmasks.red_offset)|
	    (g<<fbinfo.fbi_subtype.fbi_rgbmasks.green_offset)|
	    (b<<fbinfo.fbi_subtype.fbi_rgbmasks.blue_offset);
}

/*
 * 点を1つ表示
 */
void pset(int x, int y, int r, int g, int b) {
	/*
         * 表示領域外なら何もせずにリターン
         */
	if (x>=fbinfo.fbi_width || x<0 || y>=fbinfo.fbi_height || y<0)
		return;

	uint32_t *sp =  vram + fbinfo.fbi_fboffset/bypp;

	sp = sp + x+y*fbinfo.fbi_stride/bypp;
	*sp = pixrgb(r, g,b);
}

/*
 * (x0,y0)-(x1,y1)の線分を描画
 * ブレゼンハムのアルゴリズム
 */
void line( int x0, int y0, int x1, int y1, int r, int g, int  b)
{
	int dx,dy,sx,sy,i;

	dx = abs(x1-x0);	/* 水平変位 */
	dy = abs(y1-y0);	/* 垂直変位 */

	sx = (x1>x0)?1:-1;	/* 水平方向1回辺りの移動量(方向) */
	sy = (y1>y0)?1:-1;	/* 垂直方向1回辺りの移動量(方向) */

	if (dx > dy){
		/* 90度以上 x方向ループ */
		int up = -dx;
		for (i=0;i<=dx;i++){
			pset(x0,y0,r,g,b);
			x0 += sx;
			up += dy*2;
			if  ( up >=0 ) {
				y0 +=  sy;
				up -= dx*2;
			}
		}
	} else {
		/* 90度未満 y方向ループ */
		int up = -dy;
		for (i=0;i<=dy;i++){
			pset(x0,y0,r,g,b);
			y0 += sy;
			up += dx*2;
			if  ( up >=0 ) {
				x0 +=  sx;
				up -= dy*2;
			}
		}
	}
}

/*
 * 全画面を色で埋める
 */
void filltest() {
	int x,y;
	uint32_t *sp = vram + fbinfo.fbi_fboffset/bypp;
	uint32_t *lp,*p;

	for (y=0,lp=vram; y<fbinfo.fbi_height;y++,lp+=fbinfo.fbi_stride/bypp)
		for (x=0,p=lp; x<fbinfo.fbi_width;x++,p++)
			*p=((y)%256)<<16|(x/4)%256<<8|x%256;
}
/*
 * 全画面を白で埋める
 */
void fillwhite() {
	int x,y;
	uint32_t *sp = vram + fbinfo.fbi_fboffset/bypp;
	uint32_t *lp,*p;

	for (y=0,lp=vram; y<fbinfo.fbi_height;y++,lp+=fbinfo.fbi_stride/bypp)
		for (x=0,p=lp; x<fbinfo.fbi_width;x++,p++)
			*p=0xffffff;
}

/*
 * 全画面をクリア(黒で埋める)
 */

void cls() {
	int x,y;
	uint32_t *sp = vram + fbinfo.fbi_fboffset/bypp;
	uint32_t *lp,*p;

	for (y=0,lp=vram; y<fbinfo.fbi_height;y++,lp+=fbinfo.fbi_stride/bypp)
		for (x=0,p=lp; x<fbinfo.fbi_width;x++,p++)
			*p=0;
}

uint32_t *getVramAddr() {
	return (uint32_t *)mmap(0, fbinfo.fbi_fbsize,
			PROT_READ|PROT_WRITE, MAP_SHARED,wd,0);
}

int main() {

	int i;
	
	if ((wd=open(WSDISPDEV, O_RDWR))<0)
		err(1,"open");

	if (setBitmapmode(wd)==-1)
		err(1,"setmodeBitmap");

	getFBinfo();

	if ((vram = getVramAddr()) == (uint32_t *)-1)
		terminate(1,"getVram");

	filltest();

	sleep(5);

	cls();

	for ( i=0;i<1024;i+=8) {
		line(i,0,1024-i,1024,i,0,(i/2)%256);
		line(0,i,1024,1024-i,256-(i%256),i%256,(i/2)%256);
	}

	sleep(5);

	fillwhite();
	for ( i=0;i<1024;i+=8) {
		line(i,0,1024-i,1024,i,0,(i/2)%256);
		line(0,i,1024,1024-i,256-(i%256),i%256,(i/2)%256);
	}

	sleep(10);
	terminate(0,"");
}


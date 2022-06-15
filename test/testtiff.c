#include "common.h"

static int rgb[4096 * 4096 * 3];
static char bmp[4096 * 4096 * 3];

int main(int argc, char* argv[])
{
	FILE* p;
	libphoto_output_t attr;
	char buf[256];
	int size;
	void* tiff = tiffCreate(argv[1]);
	while(0 == tiffDecode(tiff,&attr,rgb))
	{
		size = bmpEncode(rgb,attr.width,attr.height,bmp);
		sprintf(buf,"%d.bmp",attr.index);
		p = fopen(buf,"wb");
		fwrite(bmp,1,size,p);
		fclose(p);
	}
	tiffDestroy(tiff);
	return 0;
}

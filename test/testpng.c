#include "common.h"

static char bmp[1024 * 1024 * 16] = {0};
extern int bmpEncode(int* rgb,int width,int height,char* bmp);

int main(int argc,char* argv[])
{
	libphoto_output_t info;
	void* png = pngCreate(argv[1]);
	if(0 == pngDecode(png,&info))
	{
		FILE* p;
		int size;
		size = bmpEncode(info.rgb,info.width,info.height,bmp);
		p = fopen(argv[2],"wb");
		fwrite(bmp,1,size,p);
		fclose(p);
		printf("PNG decode success!\r\n");
	}
	pngDestroy(png);
	return 0;
}


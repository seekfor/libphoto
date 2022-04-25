#include <common.h>


static char buf[4096 * 4096 * 4];

int main(int argc,char* argv[])
{
#ifdef CFG_BMP_SUPPORT
	int ret;
	int size;
	FILE* p;
	libphoto_output_t attr;
	void* bmp = bmpCreate(argv[1]);
	if(!bmp)
	{
		return -1;
	}
	ret = bmpDecode(bmp,&attr);
	if(ret == 0)
	{
		size = bmpEncode(attr.rgb,attr.width,attr.height,buf);
		p = fopen("bmp2bmp.bmp","wb");
		fwrite(buf,1,size,p);
		fclose(p);
	}
	bmpDestroy(bmp);
#endif
	return 0;
}

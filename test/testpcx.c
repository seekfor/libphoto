#include <common.h>


static char buf[4096 * 4096 * 4];

int main(int argc,char* argv[])
{
#ifdef CFG_PCX_SUPPORT
	int ret;
	int size;
	FILE* p;
	libphoto_output_t attr;
	void* pcx = pcxCreate(argv[1]);
	if(!pcx)
	{
		return -1;
	}
	ret = pcxDecode(pcx,&attr);
	if(ret == 0)
	{
		size = bmpEncode(attr.rgb,attr.width,attr.height,buf);
		p = fopen("pcx2bmp.bmp","wb");
		fwrite(buf,1,size,p);
		fclose(p);
		size = pcxEncode(attr.rgb,attr.width,attr.height,buf);
		p = fopen("bmp2pcx.pcx","wb");
		fwrite(buf,1,size,p);
		fclose(p);

	}

	pcxDestroy(pcx);
#endif
	return 0;
}

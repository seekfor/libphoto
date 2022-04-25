#include "common.h"


static char bmp[4096 * 4096 * 3];

int main(int argc, char* argv[])
{
#ifdef CFG_GIF_SUPPORT
	FILE* p;
	libphoto_output_t attr;
	char buf[256];
	int size;
	void* gif = libgifCreate(argv[1]);
	if(!gif)
	{
		return -1;
	}	
	while(0 == libgifDecode(gif,&attr))
	{
		size = bmpEncode(attr.rgb,attr.width,attr.height,bmp);
		sprintf(buf,"gif2bmp%d.bmp",attr.index);
		p = fopen(buf,"wb");
		fwrite(bmp,1,size,p);
		fclose(p);
	}
	libgifDestroy(gif);
#endif
	return 0;
}


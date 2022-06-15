#ifndef __COMMON_H__
#define __COMMON_H__

#define CFG_BMP_SUPPORT
#define CFG_GIF_SUPPORT
#define CFG_PCX_SUPPORT
#define CFG_TIFF_SUPPORT

#define RGB(r,g,b) ((unsigned int)b << 16|((unsigned int)g << 8)|((unsigned int)r))
#define RGB_R(cr) ((unsigned char)(cr))
#define RGB_G(cr) ((unsigned char)((cr) >> 8))
#define RGB_B(cr) ((unsigned char)((cr) >> 16))

#define BI_RGB		0x00
#define BI_RLE8		0x01
#define BI_RLE4		0x02

#include <string.h>
#include <stdio.h>
#include <malloc.h>


typedef struct
{  
   int  biSize; 
   int  biWidth; 
   int  biHeight; 
   short int biPlanes; 
   short int  biBitCount; 
   int biCompression; 
   int biSizeImage; 
   int biXPelsPerMeter; 
   int biYPelsPerMeter; 
   int biClrUsed; 
   int biClrImportant; 
}liphoto_bih_t;

typedef struct
{
	int index;
	int width;
	int height;
	int bpp;
	int period;
	int interlace;
	int* rgb;
}libphoto_output_t;

#define SETDWORD(ptr,item)\
{\
	*ptr++=(unsigned int)(item & 0xff);\
	*ptr++=(unsigned int)(item >> 8) & 0xff;\
	*ptr++=(unsigned int)(item >> 16) & 0xff;\
	*ptr++=(unsigned int)(item >> 24) & 0xff;\
}
#define SETWORD(ptr,item)\
{\
	*ptr++=(item & 0xff);\
	*ptr++=(item >> 8) & 0xff;\
}
#define GETDWORD(ptr) (ptr += 4,((unsigned int)(*(ptr - 4)))|((unsigned int)(*(ptr - 3)) << 8)|((unsigned int)(*(ptr - 2)) << 16)|((unsigned int)(*(ptr - 1)) << 24))
#define GETWORD(ptr) (ptr += 2,(*(ptr - 2))|((unsigned short)(*(ptr - 1)) << 8))


#ifdef CFG_BMP_SUPPORT
	#include "libmp.h"
#endif

#ifdef CFG_GIF_SUPPORT
	#include "libgif.h"
#endif

#ifdef CFG_PCX_SUPPORT
	#include "libpcx.h"
#endif

#ifdef CFG_TIFF_SUPPORT
	#include "libtiff.h"
#endif


#endif


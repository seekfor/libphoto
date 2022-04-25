#include "common.h"

#ifdef CFG_BMP_SUPPORT

typedef struct
{
	FILE* fp;
	unsigned char gct[256 * 4];
	int bmpstart;
	int filesize;
	int bmpsize;
	int w;
	int h;
	int bpp;
	int compression;
	int topdown;
	int* rgb;
	int* ref;
}libmp_t;

static int GETCOLOR(unsigned char* gct,unsigned char pxl)
{
	unsigned char* pal = gct + pxl * 4;
	return RGB(pal[0],pal[1],pal[2]);
}

static void TOPDOWN(libmp_t* bmp)
{
	int* top,*bottom,color;
	int i,j;
	int* ref = bmp->ref;
	if(!ref)
	{
		return;
	}
	for(i = 0;i < bmp->h;i++)
	{
		top = ref + i * bmp->w;
		bottom = bmp->rgb + (bmp->h - i - 1) * bmp->w;
		for(j = 0;j < bmp->w;j++)
		{
			color = *top;
			*top = *bottom;
			*bottom = color;
			top++;
			bottom++;
		}
	}
	top = bmp->rgb;
	bottom = ref;
	memcpy(top,bottom,bmp->w * bmp->h * 4);
}



static void bmp_rle4_decode(libmp_t* bmp,unsigned char* buf,int len)
{
	unsigned char* ptr = buf;
	int* video_ptr = bmp->rgb;
	int* video_end = bmp->rgb + bmp->w * bmp->h;
	unsigned char first,second;
	int i,j,k;
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i* bmp->w;
		for(j = 0;j < bmp->w && (video_ptr < video_end);)
		{
			first = *ptr++;
			second = *ptr++;
			if(first)
			{
				for(k = 0;k < (first >> 1) && (video_ptr < video_end - 2);k++)
				{
					*video_ptr++ = GETCOLOR(bmp->gct,second >> 4);
					*video_ptr++ = GETCOLOR(bmp->gct,second & 0x0f);
					j+=2;
				}
				if((first & 0x01) && (video_ptr < video_end))
				{
					*video_ptr++ = GETCOLOR(bmp->gct,second >> 4);
					j++;
				}
			}
			else
			{
				switch(second)
				{
				case 0x00:
					len = (int)((size_t)video_ptr - (size_t)bmp->rgb) % bmp->w;
					if(len)
					{
						video_ptr += bmp->w - len;
					}
					j += len;
					break;
				case 0x01:
					return;
				case 0x02:
					first = *ptr++;
					second = *ptr++;
					video_ptr += second * bmp->w + first;
					j=first;
					break;
				default:
					first =((second + 1) >> 1) & 0x01;
					for(k = 0;k < (second >>1) && (video_ptr < video_end - 2);k++)
					{
						*video_ptr++ = GETCOLOR(bmp->gct,((*ptr)>>4));
						*video_ptr++ = GETCOLOR(bmp->gct,((*ptr) & 0x0f));
						ptr++;
						j += 2;
					}
					if((second & 0x01) && (video_ptr < video_end))
					{
						*video_ptr++ = GETCOLOR(bmp->gct,((*ptr)>>4));
						j++;
						ptr++;
					}
					ptr += first;
					break;
				}
			}
		}
	}
}

static void bmp_rle8_decode(libmp_t* bmp,unsigned char* buf,int len)
{
	unsigned char* ptr = buf;
	int* video_ptr = bmp->rgb;
	int* video_end = video_ptr + bmp->w * bmp->h;
	unsigned char first,second;
	int i,j,k;	
	for(i = 0;i < bmp->h;i++)
	{
		for(j = 0;(j < bmp->w) && (video_ptr < video_end);)
		{
			first = *ptr++;
			second = *ptr++;
			if(first)
			{
				for(k = 0;k < first && (video_ptr < video_end);k++,j++)
				{
					*video_ptr ++= GETCOLOR(bmp->gct,second);
				}
			}
			else
			{
				switch(second)
				{
				case 0x00:
					len = (int)((size_t)video_ptr - (size_t)bmp->rgb) % bmp->w;
					if(len)
					{
						video_ptr += bmp->w - len;
					}
					j += len;
					break;
				case 0x01:
					return;
				case 0x02:
					first = *ptr++;
					second = *ptr++;
					video_ptr += second * bmp->w + first;
					j = first;
					break;
				default:
					for(k = 0; k < second && (video_ptr < video_end);k++,j++)
					{
						*video_ptr++ = GETCOLOR(bmp->gct,*ptr++);
					}
					ptr += second & 0x01;
					break;
				}
			}

		}
	}
}


static void bmp_rgb1_decode(libmp_t* bmp,unsigned char* ptr,int len)
{
	int* video_ptr;
	int color;
	int i,j,k;
	unsigned char MASK[]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
	unsigned char skip;
	len = (bmp->w + 7) >> 3;
	skip = len & 0x03;
	if(skip) 
	{
		skip = 4 - skip;
	}
	if(bmp->w & 0x07)
	{
		len--;
	}
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i * bmp->w;
		for(j = 0;j < len;j++)
		{
			for(k = 0;k < 8;k++)
			{
				color = *ptr & MASK[k]?1:0;
				color = GETCOLOR(bmp->gct,color);
				*video_ptr++ = color;
			}
			ptr++; 
		}
		for(k = 0;k < (bmp->w & 0x07);k++)
		{
			color = *ptr & MASK[k]?1:0;
			color = GETCOLOR(bmp->gct,color);
			*video_ptr++ =color;
		}
		if(k) 
		{
			ptr++;
		}
		ptr += skip;
	}
}

static void bmp_rgb4_decode(libmp_t* bmp,unsigned char* ptr,int len)
{
	int* video_ptr;
	int color;
	int i,j;
	unsigned char skip;
	len = ((bmp->w + 1) >> 1);
	skip = len & 0x03;
	if(skip)
	{
		skip = 4 - skip;
	}
	if(bmp->w & 0x01)
	{
		len--;
	}
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i * bmp->w;
		for(j = 0;j < len;j++)
		{
			color = GETCOLOR(bmp->gct,(*ptr) >> 4);
			*video_ptr++ = color;
			color = GETCOLOR(bmp->gct,(*ptr) & 0x0f);
			*video_ptr++ = color;
			ptr++;
		}
		if(bmp->w & 0x01)
		{
			color = GETCOLOR(bmp->gct,((*ptr++) >> 4));
			*video_ptr++ = color;
		}
		ptr += skip;
	}
}


static void bmp_rgb8_decode(libmp_t* bmp,unsigned char* ptr,int len)
{
	int* video_ptr;
	int i,j;
	unsigned char skip;
	skip = (bmp->w & 0x03);
	if(skip) 
	{
		skip = 4 - skip;
	}
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i * bmp->w;
		for(j = 0;j < bmp->w;j++,ptr++)
		{
			*video_ptr++ = GETCOLOR(bmp->gct,*ptr);
		}
		ptr += skip;
	}
}



static void bmp_rgb16_decode(libmp_t* bmp,unsigned char* ptr,int len)
{
	int *video_ptr;
	int index;
	unsigned char r,g,b;
	int i,j;
	unsigned char skip;
	skip = (bmp->w * 2) & 0x03;
	if(skip) 
	{
		skip = 4 - skip;
	}
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i * bmp->w;
		for(j = 0;j < bmp->w;j++)
		{
			index = *ptr++;
			index += (*ptr++) << 8;
			r = (index >> 10) & 0x1f;
			g = (index >> 5) & 0x1f;
			b = index & 0x1f;
			r <<= 3;
			g <<= 3;
			b <<= 3;
			*video_ptr++ = RGB(r,g,b);
		}
		ptr += skip;
	}
}

static void bmp_rgb24_decode(libmp_t* bmp,unsigned char* ptr,int len)
{
	int* video_ptr;
	unsigned char r,g,b;
	unsigned int i,j;
	unsigned char skip;
	skip = (bmp->w * 3) & 0x03;
	if(skip) 
	{
		skip = 4 - skip;
	}
	for(i = 0;i < bmp->h;i++)
	{
		video_ptr = bmp->rgb + i * bmp->w;
		for(j = 0;j < bmp->w;j++)
		{
			b = *ptr++;
			g = *ptr++;
			r = *ptr++;
			*video_ptr++ = RGB(r,g,b);
		}
		ptr += skip;
	}
}


void* bmpCreate(char* filename)
{
	int i;
	libmp_t* bmp;
	char* src;
	int colors;
	int topdown;
	unsigned char buf[256 * 4];
	unsigned char* oldptr = buf,*ptr = buf;
	FILE* fp = fopen(filename,"rb");
	if(!fp)
	{
		return NULL;
	}
	if((54 != fread(buf,1,54,fp)) || (buf[0] != 'B') || (buf[1] != 'M') )
	{
		fclose(fp);
		return NULL;
	}
	bmp = (libmp_t*)malloc(sizeof(libmp_t));
	if(!bmp)
	{
		fclose(fp);
		return NULL;
	}
	bmp->fp = fp;
	ptr = buf + 2;
	bmp->filesize = GETDWORD(ptr);
	ptr += 12;
	bmp->w = GETDWORD(ptr);
	bmp->h = GETDWORD(ptr);
	if(bmp->h < 0)
	{
		topdown = 1;
		bmp->h = -bmp->h;
	}
	else
	{
		topdown = 0;
	}
	bmp->topdown = topdown;
	ptr += 2;
	bmp->bpp = GETWORD(ptr);
	bmp->compression = GETDWORD(ptr);
	bmp->bmpsize = GETDWORD(ptr);
	ptr += 8;
	colors = GETDWORD(ptr);
	if(!colors)
	{
		colors = 1u << bmp->bpp;
	}
	if(!bmp->bmpsize)
	{
		bmp->bmpsize = bmp->filesize - 54 - ((bmp->bpp < 16)?colors * 4:0);
	}
	bmp->rgb = (int*)malloc(bmp->w * bmp->h * 4);
	if(!bmp->rgb)
	{
		free(bmp);
		fclose(fp);
		return NULL;
	}
	bmp->ref = (int*)malloc(bmp->w * bmp->h * 4 + bmp->bmpsize);
	if(!bmp->ref)
	{
		free(bmp->rgb);
		free(bmp);
		fclose(fp);
		return NULL;
	}
	if(bmp->bpp < 16)
	{
		ptr = buf;
		fread(buf,1,colors * 4,fp);
		for(i = 0;i < colors;i++)
		{
			bmp->gct[i * 4 + 3] = *(ptr + 3);/*A*/
			bmp->gct[i * 4 + 2] = *(ptr + 0);/*B*/
			bmp->gct[i * 4 + 1] = *(ptr + 1);/*G*/
			bmp->gct[i * 4 + 0] = *(ptr + 2);/*R*/
			ptr += 4;
		}
	}
	bmp->bmpstart = ftell(fp);
	return bmp;
}

int bmpDecode(void* hdl,libphoto_output_t* info)
{
	unsigned char* src;
	int bmpsize;
	libmp_t* bmp = (libmp_t*)hdl;
	fseek(bmp->fp,bmp->bmpstart,SEEK_SET);
	src = (unsigned char*)bmp->ref;
	bmpsize = bmp->bmpsize;
	if(bmpsize != fread(src,1,bmpsize,bmp->fp))
	{
		return -1;
	}
	switch(bmp->compression)
	{
		case BI_RLE8:
			bmp_rle8_decode(bmp,src,bmpsize);
			break;
		case BI_RLE4:
			bmp_rle4_decode(bmp,src,bmpsize);
			break;
		case BI_RGB:
			switch(bmp->bpp)
			{
				case 1:
					bmp_rgb1_decode(bmp,src,bmpsize);
					break;
				case 4:
					bmp_rgb4_decode(bmp,src,bmpsize);
					break;
				case 8:
					bmp_rgb8_decode(bmp,src,bmpsize);
					break;
				case 16:
					bmp_rgb16_decode(bmp,src,bmpsize);
					break;
				case 24:
					bmp_rgb24_decode(bmp,src,bmpsize);
					break;
				default:
					return-1;
			}
			break;
	}
	TOPDOWN(bmp);
	if(info)
	{
		info->index = 0;
		info->width = bmp->w;
		info->height = bmp->h;
		info->bpp = bmp->bpp;
		info->period = 0;
		info->rgb = bmp->rgb;
	}
	return 0;
}

int bmpDestroy(void* hdl)
{
	libmp_t* bmp = (libmp_t*)hdl;
	if(bmp->rgb)
	{
		free(bmp->rgb);
	}
	free(hdl);
	return 0;
}


static int bmp_header_encode(char* images,int bpp,int width,int height,int topdown)
{
	char* ptr = images;
	unsigned int i;
	*ptr++ ='B';
	*ptr++ ='M';
	ptr += 4;
	SETDWORD(ptr,0);
	i = 54 + (bpp <= 8?((1u << bpp)<<2):0);
	SETDWORD(ptr,i);
	SETDWORD(ptr,0x28);
	SETDWORD(ptr,width);
	if(topdown)
	{
		SETDWORD(ptr,-height);
	}
	else
	{
		SETDWORD(ptr,height);
	}
	SETWORD(ptr,1);
	SETWORD(ptr,bpp);
	SETDWORD(ptr,0);
	SETDWORD(ptr,0);
	SETDWORD(ptr,3780);
	SETDWORD(ptr,3780);
	SETDWORD(ptr,0);
	SETDWORD(ptr,0);
	return 14 + 0x28;
}

static int bmp_rgb24_encode(int* rgb,int width,int height,char* bmp)
{
	int i,j,k = 0;
	int line;
	int* ptr = rgb;
	int color;
	unsigned char skip;
	line = width * 3;
	skip = line & 0x03;
	if(skip) 
	{
		skip = 4 - skip;
	}
	for(i = 0;i < height;i++)
	{
		for(j = 0;j < width;j++)
		{
			color = *ptr++;
			bmp[k++] = ((color >> 16) & 0xff);
			bmp[k++] = ((color >> 8) & 0xff);
			bmp[k++] = (color & 0xff);
		}
		for(j = 0;j < skip;j++)
		{
			bmp[k++] = 0x00;
		}
	}
	return k;
}

static void bmp_update(char* bmp,int len)
{
	char* ptr = &bmp[2];
	int size;
	SETDWORD(ptr,len);
	ptr = &bmp[34];
	size = len - 0x28 -0x12;
	SETDWORD(ptr,size);
}


int bmpEncode(int* rgb,int width,int height,char* bmp)
{
	int len;
	int skip;
	int oft;
	len = width * 3;
	skip = (len & 0x03)?(4 - (len & 0x03)) * height:0;
	oft = bmp_header_encode(bmp,24,width,height,1);
	len = oft + bmp_rgb24_encode(rgb,width,height,bmp + oft);
	bmp_update(bmp,len);
	return len;
}

#endif




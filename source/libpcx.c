#include "common.h"

#ifdef CFG_PCX_SUPPORT


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
	int bytes_per_line;
	int* rgb;
	unsigned char* ref;
	unsigned char* R;
	unsigned char* G;
	unsigned char* B;
}libpcx_t;


static int GETCOLOR(unsigned char* gct,unsigned char pxl)
{
	unsigned char* pal = gct + pxl * 4;
	return RGB(pal[0],pal[1],pal[2]);
}

static unsigned char pcx_rle_decode(unsigned char* ptr,unsigned char* count,unsigned char* val)
{
	unsigned char item = *ptr;
	*count = 1;
	if((item & 0xc0) == 0xc0)
	{
		*count = item & 0x3f;
		*val = *(ptr + 1);
		item = 2;
	}
	else
	{
		*val = item;
		item = 1;
	}
	return item;
}

static void pcx_rgb1_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j,k;
	int* video_ptr;
	unsigned int color;
	unsigned char *R = pcx->R;
	unsigned char  MASK[] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

	for(j = 0;j < pcx->h;j++)
	{
		for(i = 0;i < pcx->bytes_per_line;i++)
		{
			R[i] = *buffer++;
		}
		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < pcx->w >> 3;i++)
		{
			for(k = 0;k < 8 && ((i * 8 + k) < pcx->w);k++)
			{
				color = (R[i] & MASK[k])?RGB(255,255,255):RGB(0,0,0);
				*video_ptr++ =color;
			}
		}
		if(pcx->w & 0x07)
		{
			for(k = 0; k < (pcx->w & 0x07);k++)
			{
				color = (R[i] & MASK[k])?RGB(255,255,255):RGB(0,0,0);
				*video_ptr++ =color;
			}
		}

	}
}

static void pcx_rgb4_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j;
	int* video_ptr;
	int color;
	unsigned char* R = pcx->R;
	unsigned char* ptr = buffer;

	for(j = 0;j < pcx->h;j++)
	{
		for(i = 0;i < pcx->bytes_per_line;i++)
		{
			R[i] = *ptr++;
		}
		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < (pcx->w >> 1);i++)
		{
			color = GETCOLOR(pcx->gct,(R[i]>>4));
			*video_ptr++ =color;
			color = GETCOLOR(pcx->gct,(R[i]&0x0f));
			*video_ptr++ =color;
		}
		if(pcx->w & 0x01)
		{
			color = GETCOLOR(pcx->gct,(R[i]>>4));
			*video_ptr++ = color;
		}
	}
}

static void pcx_rgb8_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j;
	int* video_ptr;
	unsigned char* ptr;
	unsigned char* R = pcx->R;
	ptr = buffer + len - 768;
	for(i = 0;i < 256;i++)
	{
		pcx->gct[i * 4 + 0] = *ptr++;
		pcx->gct[i * 4 + 1] = *ptr++;
		pcx->gct[i * 4 + 2] = *ptr++;
		pcx->gct[i * 4 + 3] = 0;
	}
	ptr = buffer;
	video_ptr = pcx->rgb;
	for(i = 0;i < pcx->h;i++)
	{
		for(j = 0;j < pcx->bytes_per_line;j++)
		{
			R[j] = *ptr++;
		}
		for(j = 0;j < pcx->w;j++)
		{
			*video_ptr++ = GETCOLOR(pcx->gct,R[j]);
		}
	}
}

static void pcx_rgb24_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j;
	int* video_ptr;
	unsigned char* ptr;
	unsigned char *R,*G,*B;
	ptr = buffer;
	R = pcx->R;
	G = pcx->G;
	B = pcx->B;
	for(i = 0;i < pcx->h;i++)
	{
		video_ptr = pcx->rgb + i * pcx->w;
		for(j = 0;j < pcx->bytes_per_line;j++)
		{
			R[j] = *ptr++;
		}
		for(j = 0;j < pcx->bytes_per_line;j++)
		{
			G[j]=*ptr++;
		}
		for(j = 0;j < pcx->bytes_per_line;j++)
		{
			B[j]=*ptr++;
		}
		for(j = 0;j < pcx->w;j++)
		{
			*video_ptr++ = RGB(R[j],G[j],B[j]);
		}
	}
}


static void pcx_rle1_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j,k;
	int* video_ptr;
	int color;
	unsigned char* R = pcx->R;
	unsigned char count,val;
	unsigned char MASK[] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

	for(j = 0;j < pcx->h;j++)
	{
		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count-- && (i < pcx->bytes_per_line))
			{
				R[i++] = val;
			}
		}
		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < pcx->w >> 3;i++)
		{
			for(k = 0;(k < 8) && (i * 8 + k < pcx->w);k++)
			{
				color = (R[i] & MASK[k])?RGB(255,255,255):RGB(0,0,0);
				*video_ptr++ =color;
			}
		}
		if(pcx->w & 0x07)
		{
			for(k = 0;k < (pcx->w & 0x07);k++)
			{
				color = (R[i] & MASK[k])?RGB(255,255,255):RGB(0,0,0);
				*video_ptr++ =color;
			}
		}

	}
}


static void pcx_rle4_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	int i,j;
	int* video_ptr;
	int color;
	unsigned char* R = pcx->R;
	unsigned char count,val;
	unsigned char* ptr = buffer;

	for(j = 0;j < pcx->h;j++)
	{
		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count--&& (i < pcx->bytes_per_line))
			{
				R[i++] = val;
			}
		}

		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < (pcx->w >> 1);i++)
		{
			color = GETCOLOR(pcx->gct,(R[i]>>4));
			*video_ptr++ = color;
			color = GETCOLOR(pcx->gct,(R[i]&0x0f));
			*video_ptr++ = color;
		}
		if(pcx->w & 0x01)
		{
			color = GETCOLOR(pcx->gct,(R[i]>>4));
			*video_ptr++ =color;
		}

	}
}

static void pcx_rle8_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	unsigned char* R = pcx->R;
	unsigned char count,val;
	int* video_ptr;
	int color;
	unsigned char* ptr;
	int i,j;
	ptr = buffer + len - 768;
	for(i = 0;i < 256;i++)
	{
		pcx->gct[i * 4 + 0] = *ptr++;
		pcx->gct[i * 4 + 1] = *ptr++;
		pcx->gct[i * 4 + 2] = *ptr++;
		pcx->gct[i * 4 + 3] = 0;
	}
	for(j = 0;j < pcx->h;j++)
	{
		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count-- && (i < pcx->bytes_per_line))
			{
				R[i++] = val;
			}
		}

		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < pcx->w;i++)
		{
			color = GETCOLOR(pcx->gct,R[i]);
			*video_ptr++ = color;
		}
	}
}

static void pcx_rle24_decode(libpcx_t* pcx,unsigned char* buffer,int len)
{
	unsigned char *R,*G,*B;
	unsigned char count,val;
	int* video_ptr;
	int color;
	int i,j;
	R = pcx->R;
	G = pcx->G;
	B = pcx->B;

	for(j = 0;j < pcx->h;j++)
	{
		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count-- && (i < pcx->bytes_per_line))
			{
				R[i++] = val;
			}
		}

		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count-- && (i < pcx->bytes_per_line) )
			{
				G[i++] = val;
			}
		}
		

		i = 0;
		while(i < pcx->bytes_per_line)
		{
			buffer += pcx_rle_decode(buffer,&count,&val);
			while(count-- && (i < pcx->bytes_per_line))
			{
				B[i++] = val;
			}
		}

		video_ptr = pcx->rgb + j * pcx->w;
		for(i = 0;i < pcx->w;i++)
		{
			color = RGB(R[i],G[i],B[i]);
			*video_ptr++ = color;
		}
	}
}


void* pcxCreate(char* filename)
{
	libpcx_t* pcx;
	int i;
	FILE* fp;
	int filesize;
	unsigned int xmin,ymin,xmax,ymax;
	unsigned char buf[128];
	unsigned char* ptr = buf;
	fp = fopen(filename,"rb");
	if(!fp)
	{
		return NULL;
	}
	fseek(fp,0,SEEK_END);
	filesize = ftell(fp);
	fseek(fp,0,SEEK_SET);
	if((128 != fread(buf,1,128,fp)) || (buf[0] != 0x0a) || (buf[1] < 1) )
	{
		fclose(fp);
		return NULL;
	}
	ptr += 2;
	pcx = (libpcx_t*)malloc(sizeof(libpcx_t));
	if(!pcx)
	{
		fclose(fp);
		return NULL;
	}
	pcx->fp = fp;
	pcx->bmpstart = ftell(fp);
	pcx->filesize = filesize;
	pcx->bmpsize = filesize - 128;

	pcx->compression = *ptr++;
	pcx->bpp = *ptr++;
	xmin = *ptr++;
	xmin += (*ptr ++) << 8;
	ymin = *ptr++;
	ymin += (*ptr ++) << 8;
	xmax = *ptr++;
	xmax += (*ptr ++) << 8;
	ymax = *ptr++;
	ymax += (*ptr ++) << 8;

	pcx->w = xmax - xmin + 1;
	pcx->h = ymax - ymin + 1;
	ptr += 4;
	for(i = 0;i < 16;i++)
	{
		pcx->gct[i * 4 + 0] = *ptr++;
		pcx->gct[i * 4 + 1] = *ptr++;
		pcx->gct[i * 4 + 2] = *ptr++;
		pcx->gct[i * 4 + 3] = 0;
	}
	ptr++;
	i = *ptr++;
	switch(pcx->bpp)
	{
		case 1:
			if(i != 1) 
			{
				i = 0;
			}
			break;
		case 4:
			if(i != 1)
			{
				i = 0;
			}
			break;
		case 8:
			if((i != 1) && (i != 3)) 
			{
				i = 0;
			}
			break;
		default:
			i = 0;
			break;
	}
	if(i == 0)
	{
		fclose(fp);
		free(pcx);
		return NULL;
	}
	pcx->bpp *= i;
	pcx->bytes_per_line = *ptr ++;
	pcx->bytes_per_line += (*ptr++) << 8;
	pcx->ref = (unsigned char*)malloc(pcx->bytes_per_line * 3 + pcx->bmpsize);
	if(!pcx->ref)
	{
		free(pcx);
		fclose(fp);
		return NULL;
	}
	pcx->R = pcx->ref + pcx->bmpsize;
	pcx->G = pcx->R + pcx->bytes_per_line;
	pcx->B = pcx->G + pcx->bytes_per_line;;
	pcx->rgb = (int*)malloc(pcx->w * pcx->h * 4);
	if(!pcx->rgb)
	{
		free(pcx->ref);
		free(pcx);
		fclose(fp);
		return NULL;
	}
	return pcx;
}
	
int pcxDecode(void* hdl,libphoto_output_t* info)
{
	libpcx_t* pcx = (libpcx_t*)hdl;
	fseek(pcx->fp,pcx->bmpstart,SEEK_SET);
	if(pcx->bmpsize != fread(pcx->ref,1,pcx->bmpsize,pcx->fp))
	{
		return -1;
	}	
	switch(pcx->bpp)
	{
		case 1:
			if(pcx->compression)
			{
				pcx_rle1_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			else
			{
				pcx_rgb1_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			break;
		case 4:
			if(pcx->compression)
			{
				pcx_rle4_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			else
			{
				pcx_rgb4_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			break;
		case 8:
			if(pcx->compression)
			{
				pcx_rle8_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			else
			{
				pcx_rgb8_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			break;
		case 24:
			if(pcx->compression)
			{
				pcx_rle24_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			else
			{
				pcx_rgb24_decode(pcx,pcx->ref,pcx->bmpsize);
			}
			break;
		default:
			return -1;
	}
	if(info)
	{
		info->index = 0;
		info->width = pcx->w;
		info->height = pcx->h;
		info->bpp = pcx->bpp;
		info->period = 0;
		info->rgb = pcx->rgb;
	}
	return 0;
}

int pcxDestroy(void* hdl)
{
	libpcx_t* pcx = (libpcx_t*)hdl;
	free(pcx->ref);
	free(pcx->rgb);
	free(pcx);
	return 0;
}



static int pcx_rle_encode(unsigned char* source,unsigned char* dest,int len)
{
	int j = 0;
	int ret = 0;
	unsigned char run = 0;
	unsigned char current;
	unsigned char* end = source + len;
	while(source < end)
	{
		current = *source++;
		run = 1;
		while((*source == current) && (run < 0x3f))
		{
			run++;
			source++;
		}
		dest[j++] = 0xc0 + run;
		dest[j++] = current;
		ret += 2;
	}
	return ret;
}

static int pcx_rle24_encode(int* buffer,int w,int h,unsigned char* pcx)
{
	int len;
	unsigned char *R,*G,*B;
	unsigned char *encoded;
	int bytes_per_line = w;
	int i,j,k;
	int images_len = 0;
	int* ptr = buffer;
	if(w & 0x01)
	{
		bytes_per_line ++;
	}
	R = (unsigned char*)malloc(bytes_per_line * 6);
	if(!R)
	{
		return 0;
	}
	G = R + bytes_per_line;
	B = G + bytes_per_line;
	encoded = B + bytes_per_line;
	
	for(j = 0;j < h;j++)
	{
		for(i = 0;i < w;i++,ptr++)
		{
			R[i] = RGB_R(*ptr);
			G[i] = RGB_G(*ptr);
			B[i] = RGB_B(*ptr);
		}
		len = pcx_rle_encode(R,encoded,bytes_per_line);
		for(k = 0;k < len;k++)
		{
			pcx[images_len++] = encoded[k];
		}

		len = pcx_rle_encode(G,encoded,bytes_per_line);
		for(k = 0;k < len;k++)
		{
			pcx[images_len++] = encoded[k];
		}

		len = pcx_rle_encode(B,encoded,bytes_per_line);
		for(k = 0;k < len;k++)
		{
			pcx[images_len++] = encoded[k];
		}
	}
	free(R);
	return images_len;	
}




static int pcx_encode_header(int w,int h,char* pcx)
{
	int i;
	int planes = 3;
	int bpp = 8;
	unsigned char* ptr = (unsigned char*)pcx;
	int bytes_per_line = w;
	if(w & 0x01)
	{
		bytes_per_line ++;
	}

	memset(ptr,0,128);
	*ptr++ = 0x0a;
	*ptr++ = 0x05;
	*ptr++ = 1;
	*ptr++ = bpp;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = w - 1;
	*ptr++ = (w -1) >> 8;
	*ptr++ = h - 1;
	*ptr++ = (h -1)>>8;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	*ptr++ = 0x00;
	for(i = 0;i < 16;i++)
	{
		*ptr++ = 0x00;
		*ptr++ = 0x00;
		*ptr++ = 0x00;
	}
	*ptr++ = 0x00;
	*ptr++ = planes;
	*ptr++ = bytes_per_line;
	*ptr++ = bytes_per_line >> 8;
	*ptr++ = 0x01;
	*ptr++ = 0x00;
	return 128;	
}

static int pcx_encode_data(int* rgb,int w,int h,char* pcx)
{
	return pcx_rle24_encode(rgb,w,h,pcx);
}


int pcxEncode(int* rgb,int width,int height,char* pcx)
{
	pcx_encode_header(width,height,pcx);
	return pcx_encode_data(rgb,width,height,pcx + 128) + 128;
}






#endif



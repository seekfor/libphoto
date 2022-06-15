#include "common.h"

#ifdef CFG_GIF_SUPPORT

typedef struct
{
	unsigned char kw[CFG_MAX_STRING_SIZE];
	int kwsize;
}libgif_lzw_table_t;

typedef struct
{
	libgif_lzw_table_t tbl[4096];
	int tblsize;
	int codesize;
	int clearcode;
	int endcode;
}libgif_lzw_t;


typedef struct
{
	unsigned int rgb[256];
}libgif_colortable_t;

typedef struct
{
	int version;
	int width;
	int height;
	int bpp;
	int gct;
	int flags;
	int background;
	int ratio;
	int codesize;
	unsigned int start;
}libgif_info_t;

typedef struct
{
	int index;
	int transcolor;
	int delaytime;
	int dispmode;
	int left;
	int top;
	int width;
	int height;
}libgif_frame_attr_t;



typedef struct
{
	FILE* fp;
	libgif_info_t info;
	libgif_lzw_t lzw;
	libgif_colortable_t gct;
	libgif_colortable_t lct;
	libgif_frame_attr_t frame;
	libgif_colortable_t* pct;
	unsigned char* decbuf;
	unsigned char* blkbuf;
	int* rgbuf;	
}libgif_t;

typedef struct
{
	unsigned char* start;
	unsigned char* buf;
	unsigned char* end;
	int codesize;
	int rd;
	int mode;
	unsigned int val;
}libgif_bitstream_t;

void bits_init(libgif_bitstream_t* bt,unsigned char* buf,unsigned int len,unsigned char codesize,int mode)
{
	bt->start = buf;
	bt->buf = buf;
	bt->codesize = codesize;
	bt->rd = 0;
	bt->end = buf + len;
	bt->mode = mode;
	if(mode)
	{
		bt->val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		bt->buf += 4;
	}


}

unsigned int nextbits(libgif_bitstream_t* bt,unsigned int peek)
{
	unsigned int ret = 0;
	unsigned int k;
	unsigned int low = 0,med = 0,hi = 0;
	unsigned char MASK[]=
	{
		0x00,0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f,0xff
	};
	int bits;
	int lowbit;
	int rd = bt->rd;
	bits = bt->codesize;
	if(bt->mode == 0)
	{
		if(!bt->buf)
		{
			return 0xffffffff;
		}
		lowbit = 8 - rd;
		if(lowbit >= bits)
		{
			low = (bt->buf[0] >> rd) & MASK[bits];
			k = rd + bits;
		}
		else
		{
			low = (bt->buf[0] >> rd) & MASK[lowbit];
			bits -= lowbit;
			bt->buf ++;

			if(bits <= 8)
			{
				med = bt->buf[0] & MASK[bits];
				k = bits;
			}
			else
			{
				bits -= 8;
				med = bt->buf[0];
				bt->buf ++;
				hi = bt->buf[0] & MASK[bits];
				k = bits;
			}
		}
		if(k >= 8)
		{
			k -= 8;
			bt->buf ++;
		}
		bt->rd = k;
		ret = low | (med << lowbit) | (hi << (8 + lowbit));
	}
	else
	{
		ret = (bt->val << rd) >> (32 - bits);
		rd += bits;
		if(bt->buf)
		{
			while(rd >= 8)
			{
				bt->val <<= 8;
				bt->val |= bt->buf[0];

				rd -= 8;
				bt->buf ++;
			}
		}
		bt->rd = rd;
	}
	if(bt->buf >= bt->end)
	{
		bt->buf = (unsigned char*)0;
	}
	return ret;
}


static void libgif_lzw_clear(libgif_t* gif,libgif_bitstream_t* bt)
{
	int i;
	int start = 1 << gif->info.codesize;
	for(i = start + 2;i < gif->lzw.tblsize; i ++)
	{
		gif->lzw.tbl[i].kwsize = 0;
	}
	for(i = 0; i < start + 2; i ++)
	{
		gif->lzw.tbl[i].kw[0] = i;
		gif->lzw.tbl[i].kwsize = 1;
	}
	gif->lzw.codesize = gif->info.codesize + 1;
	bt->codesize = gif->lzw.codesize;;
	gif->lzw.tblsize = start + 2;
}

static int libgif_lzw_add_table(libgif_t* gif,libgif_bitstream_t* bt,char* kw,int kwsize)
{
	int i;
	libgif_lzw_table_t* t = gif->lzw.tbl + gif->lzw.tblsize;
	int start = 1 << (gif->info.codesize );
	int diff = gif->fp?0:1;
	if(gif->lzw.tblsize >= 4095)
	{
		return 0;
	}
	t->kwsize = kwsize;
	memcpy(t->kw,kw,kwsize);
	gif->lzw.tblsize ++;
	if(gif->lzw.tblsize >= ((1 << gif->lzw.codesize) - diff) )
	{
		if(gif->lzw.codesize >= 12)
		{
			gif->lzw.codesize = 8;
		}
		gif->lzw.codesize++;
		bt->codesize = gif->lzw.codesize;
	}
	return gif->lzw.tblsize;
}

static int libgif_lzw_get_table(libgif_t* gif,int idx,char* kw)
{
	libgif_lzw_table_t* t;
	t = &gif->lzw.tbl[idx];
	if(t->kwsize)
	{
		if(kw)
		{
			memcpy(kw,t->kw,t->kwsize);
		}
		return t->kwsize;
	}
	return 0;
}

static int libgif_lzw_init(libgif_t* gif,int codesize)
{
	gif->lzw.codesize = codesize + 1;
	gif->lzw.clearcode = 1 << codesize;
	gif->lzw.endcode = gif->lzw.clearcode + 1;
	return 0;
}

static short int libgif_alloc_code(libgif_bitstream_t* bt)
{
	return nextbits(bt,1);
}

static int libgif_lzw_decode(libgif_t* gif,char* buf,int size,int mode)
{
	unsigned char* out = gif->decbuf;
	short int pw = -1,cw;
	char kw[8192];
	char nkw[8192];
	int kwsize;
 	int i;
	unsigned short clear = gif->lzw.clearcode;
	unsigned short end = gif->lzw.endcode;
	libgif_bitstream_t bt;
	bits_init(&bt,(unsigned char*)buf,size,gif->lzw.codesize,mode);
	libgif_lzw_clear(gif,&bt);
	do
	{
		cw = libgif_alloc_code(&bt);
	}while(cw == clear);
	*out++ = cw & 0xff;
 	while(cw != end)
	{
		pw = cw;
		cw = libgif_alloc_code(&bt);
		if(cw == clear)
		{
			libgif_lzw_clear(gif,&bt);
			cw = libgif_alloc_code(&bt);
			*out++ = cw & 0xff;
			continue;
		}
		else if(cw == end)
		{
			break;
		}
		else if(cw == -1)
		{
			break;
		}
		kwsize = libgif_lzw_get_table(gif,cw,kw);
		if(kwsize)
		{
			for(i = 0; i < kwsize;i ++)
			{
				*out++ = kw[i];
			}
			kwsize = libgif_lzw_get_table(gif,pw,nkw);
			nkw[kwsize] = kw[0];
			libgif_lzw_add_table(gif,&bt,nkw,kwsize + 1);
		}
		else
		{
			kwsize = libgif_lzw_get_table(gif,pw,nkw);
			nkw[kwsize] = nkw[0];
			for(i = 0; i < kwsize + 1; i ++)
			{
				*out++ = nkw[i];
			}
			libgif_lzw_add_table(gif,&bt,nkw,kwsize + 1);
		}
	}
	return 0;
}

static int libgif_is_giformat(FILE* p)
{
	unsigned char sig[6] = {0};
	fread(sig,1,6,p);
	if(!memcmp(sig,"GIF87a",6))
	{
		return 0x87a;
	}
	else if(!memcmp(sig,"GIF89a",6))
	{
		return 0x89a;
	}
	return 0;
}

static int libgif_logic_screen_descriptor(libgif_t* gif,int gifver)
{
	unsigned char buf[8] = {0};
	libgif_info_t* info = &gif->info;
	info->version = gifver;
	fread(buf,1,7,gif->fp);
	info->width = buf[0] | (buf[1] << 8);
	info->height = buf[2] | (buf[3] << 8);
	info->flags = buf[4];
	info->background = buf[5];
	info->ratio = buf[6];
	info->bpp = ((buf[4] >> 4) & 0x07) + 1;
	info->gct = 1 << ((buf[4] & 0x07) + 1);
	gif->decbuf = (unsigned char*)malloc(info->width * info->height);
	gif->blkbuf = (unsigned char*)malloc(info->width * info->height * 4);
	gif->rgbuf = (int*)malloc(info->width * info->height * sizeof(int));
	return buf[4];
}

static int libgif_global_color_table(libgif_t* gif)
{
	int i;
	unsigned char buf[4];
	libgif_colortable_t* gct = &gif->gct;
	int numcolor = 1 << ((gif->info.flags & 0x07) + 1);
	for(i = 0; i < numcolor; i ++)
	{
		if(3 != fread(buf,1,3,gif->fp))
		{
			return -1;
		}
		gct->rgb[i] = buf[0] | (buf[1] << 8) | (buf[2] << 16);
	}
	return 0;
}

static int libgif_header_init(libgif_t* gif,int ver)
{
	libgif_info_t* info = &gif->info;
	int mask = libgif_logic_screen_descriptor(gif,ver);
	if(mask & 0x80)
	{
		libgif_global_color_table(gif);
	}
	info->start = ftell(gif->fp);
	return 0;
}

void* gifCreate(char* filename)
{
	FILE* p;
	int gifver;
	libgif_t* gif;
	p = fopen(filename,"rb");
	if(p == NULL)
	{
		return NULL;
	}
	gifver = libgif_is_giformat(p);
	if(gifver == 0)
	{
		fclose(p);
		return NULL;
	}
	gif = (libgif_t*)malloc(sizeof(libgif_t));
	if(!gif)
	{
		return NULL;
	}
	memset(gif,0,sizeof(libgif_t));
	gif->fp = p;
	libgif_header_init(gif,gifver);
	return (void*)gif;
}

static int libgif_decode_gce(libgif_t* gif)
{
	unsigned char size;
	unsigned char buf[256] = {0};
	libgif_frame_attr_t* frm = &gif->frame;
	frm->dispmode = 0;
	frm->delaytime = 0;
	frm->transcolor = -1;
	do
	{
		if(1 != fread(&size,1,1,gif->fp))
		{
			return -1;
		}
		if(size != fread(buf,1,size,gif->fp))
		{
			return -1;
		}
		if(size >= 4)
		{
			frm->dispmode = (buf[0] >> 2) & 0x07;
			frm->delaytime = buf[1] | (buf[2] << 8);
			if(buf[0] & 0x01)
			{
				frm->transcolor = buf[3];
			}
		}
	}while(size);
	return 0;
}

static int libgif_decode_ael(libgif_t* gif)
{
	unsigned char size;
	unsigned char buf[256] = {0};
	do
	{
		if(1 != fread(&size,1,1,gif->fp))
		{
			return -1;
		}
		if(size != fread(buf,1,size,gif->fp))
		{
			return -1;
		}
	}while(size);
	return 0;
}


static int libgif_decode_89a(libgif_t* gif)
{
	unsigned char ch;
	if(1 != fread(&ch,1,1,gif->fp))
	{
		return -1;
	}
	switch(ch)
	{
	case 0xf9:/*Graphics Control Extension*/
		if(0 != libgif_decode_gce(gif))
		{
			return -1;
		}
		break;
	case 0xff:/*Application Externsion Label*/
		if(0 != libgif_decode_ael(gif))
		{
			return -1;
		}
		break;
	default:
		if(0 != libgif_decode_ael(gif))
		{
			return -1;
		}
		break;
	}
	return 0;
}

static int libgif_decode_block(libgif_t* gif,char* buf)
{
	unsigned char size = 0;
	if(1 != fread(&size,1,1,gif->fp))
	{
		return 0;
	}
	if(!size)
	{
		return 0;
	}
	if(size != fread(buf,1,size,gif->fp))
	{
		return 0;
	}
	return size;
}

static int libgif_decode_picture(libgif_t* gif,int* rgb)
{
	int i;
	int ret;
	char* start = (char*)gif->blkbuf;
	int size = 0;
	unsigned char buf[12];
	unsigned int xoffset,yoffset,w,h,mask;
	libgif_frame_attr_t* frm = &gif->frame;
	int numcolor;
	libgif_colortable_t* lct = &gif->lct;
	if(9 != fread(buf,1,9,gif->fp))
	{
		return -1;
	}
	xoffset = buf[0] | (buf[1] << 8);
	yoffset = buf[2] | (buf[3] << 8);
	w = buf[4] | (buf[5] << 8);
	h = buf[6] | (buf[7] << 8);
	frm->left = xoffset;
	frm->top = yoffset;
	frm->width = w;
	frm->height = h;
	mask = buf[8];
	gif->pct = &gif->gct;
	if(mask & 0x80)
	{
		gif->pct = &gif->lct;
		numcolor = 1 << ((mask & 0x07) + 1);
		for(i = 0; i < numcolor; i ++)
		{
			if(3 != fread(buf,1,3,gif->fp))
			{
				return -1;
			}
			lct->rgb[i] = buf[0] | (buf[1] << 8) | (buf[2] << 16);
		}
	}
	if(1 != fread(buf,1,1,gif->fp))
	{
		return -1;
	}
	gif->info.codesize = buf[0];
	libgif_lzw_init(gif,buf[0]);
	do
	{
		ret = libgif_decode_block(gif,start);
		start += ret;
		size += ret;
	}while(ret > 0);

	libgif_lzw_decode(gif,(char*)gif->blkbuf,size,0);


	return 0;
}

void* gifDecodeTiff(char* buf,int size,int width,int height,char** outbuf)
{
	libgif_t* gif;
	gif = (libgif_t*)malloc(sizeof(libgif_t));
	if(gif)
	{
		memset(gif,0,sizeof(libgif_t));
		gif->decbuf = (unsigned char*)malloc(width * height * 4);
		gif->info.codesize = 8;
		libgif_lzw_init(gif,8);
		libgif_lzw_decode(gif,buf,size,1);
		if(outbuf)
		{
			*outbuf = (char*)gif->decbuf;
		}
	}	
	return gif;
}

int gifDecode(void* hdl,libphoto_output_t* attr)
{
	int ret = -1;
	int end = 0;
	unsigned char fmt;
	libgif_t* gif = (libgif_t*)hdl;
	while(!end)
	{
		if(1 != fread(&fmt,1,1,gif->fp))
		{
			return -1;
		}
		switch(fmt)
		{
		case 0x21:
			if(0 != libgif_decode_89a(gif))
			{
				return -1;
			}
			break;
		case 0x2c:
			gif->frame.index ++;
			ret = libgif_decode_picture(gif,gif->rgbuf);
			end = 1;
			break;
		case 0x3b:
			end = 1;
			gif->frame.index = 0;
			fseek(gif->fp,gif->info.start,SEEK_SET);
			break;
		}
	};
	if(ret == 0)
	{
		libgif_frame_attr_t* frm = &gif->frame;
		unsigned char* out = gif->decbuf;
		int* pxl;
		int i,j;
		unsigned int fill;
		int dstride = gif->info.width;
		int sstride = frm->width;
		int maxh = frm->height;
		int maxw = frm->width;
		libgif_colortable_t* pct = gif->pct;
		switch(frm->dispmode)
		{
		default:/*reserved*/
		case 0x00:/*no display action*/
		case 0x01:/*display normal*/
			break;
		case 0x02:/*restore background first*/
			fill = pct->rgb[gif->info.background & 0xff];
			/*if the background color is same as transparent color,fixed it's background to WHITE!*/
			if(gif->info.background == frm->transcolor)
			{
				fill = 0xffffffff;
			}
			pxl = gif->rgbuf;
			for(i = 0; i < gif->info.height;i ++)
			{
				for(j = 0; j < gif->info.width; j ++,pxl ++)
				{
					*pxl = fill;
				}
			}
			break;
		case 0x03:/*restore prev frame first*/
			break;
		}
		pxl = gif->rgbuf + frm->top * gif->info.width + frm->left;
		for(i = 0; i < maxh; i ++)
		{
			for(j = 0; j < maxw; j ++)
			{
				if(out[j] != frm->transcolor)
				{
					pxl[j] = pct->rgb[out[j]];
				}
			}
			pxl += dstride;
			out += sstride;
		}
		if(attr)
		{
			attr->index = gif->frame.index - 1;
			attr->bpp = gif->info.bpp;
			attr->width = gif->info.width;
			attr->height = gif->info.height;
			attr->period = gif->frame.delaytime * 10;
			attr->rgb = gif->rgbuf;
		}
	}
	return ret;
}

int gifDestroy(void* hdl)
{
	libgif_t* gif = (libgif_t*)hdl;
	if(gif->fp)
	{
		fclose(gif->fp);
	}
	if(gif->decbuf)
	{
		free(gif->decbuf);
	}
	if(gif->blkbuf)
	{
		free(gif->blkbuf);
	}
	if(gif->rgbuf)
	{
		free(gif->rgbuf);
	}
	free(gif);
	return 0;
}
#endif


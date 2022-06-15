#include "common.h"

#define IFD_TAG_NEWSUBFILETYPE	0x00fe
#define	IFD_TAG_SUBFILETYPE		0x00ff

#define IFD_TAG_IMAGEWIDTH		0x0100
#define IFD_TAG_IMAGEHEIGHT		0x0101
#define IFD_TAG_BITSPERSAMPLE	0x0102
#define IFD_TAG_COMPRESS		0x0103
	#define IFD_COMPRESS_NONE		1
	#define IFD_COMPRESS_CCITT		2
	#define IFD_COMPRESS_GROUP3FAX	3
	#define IFD_COMPRESS_GROUP4FAX	4
	#define IFD_COMPRESS_LZW		5
	#define IFD_COMPRESS_JPEG		6
	#define IFD_COMPRESS_PACKBITS	32773

#define IFD_TAG_PHOTOATTR			0x0106
	#define IFD_PHOTOATTR_WHITEISZERO	0
	#define IFD_PHOTOATTR_BLACKISZERO	1
	#define IFD_PHOTOATTR_RGB			2
	#define IFD_PHOTOATTR_PALETTE		3
	#define IFD_PHOTOATTR_TRANPARENCY	4



#define IFD_TAG_STRIPOFFSETS	0x0111
#define IFD_TAG_SAMPLESPERPIXEL	0x0115
#define IFD_TAG_ROWSPERSTRIP	0x0116
#define IFD_TAG_STRIPBYTECOUNT	0x0117

#define IFD_TAG_XRES			0x011a
#define IFD_TAG_YRES			0x011b
#define IFD_TAG_PLANAR			0x011c
#define IFD_TAG_RESUNIT			0x0128
#define IFD_TAG_PREDICTOR		0x013d
#define IFD_TAG_COLORMAP		0x0140

#define IFD_TAG_COPYRIGHT		0x8298
#define IFD_TAG_DATETIME		0x0132
#define IFD_TAG_EXTRASAMPLES	0x0152



#define IFD_TYPE_BYTE		1
#define IFD_TYPE_ASCII		2
#define IFD_TYPE_WORD		3
#define IFD_TYPE_DWORD		4
#define IFD_TYPE_QWORD		5
#define IFD_TYPE_SBYTE		6
#define IFD_TYPE_UNDEF		7
#define IFD_TYPE_SWORD		8
#define IFD_TYPE_SDWORD		9
#define IFD_TYPE_SQWORD		10
#define IFD_TYPE_FLOAT		11
#define IFD_TYPE_DOUBLE		12

typedef struct
{
	int magic;
	FILE* fp;

	int isyuv;
	int isplanar;

	int compression;
	int sps;
	int predictor;

	int xres;
	int yres;
	int unit;


	int framestart[1024];
	int numframes;

	int*	rgb;
	char*	blk;
	int blksize;

	libphoto_output_t info;
}libtiff_t;

static int libtiff_is_little_endia(int magic)
{
	return magic == 0x002a4949;
}

static int libtiff_read_byte(libtiff_t* t)
{
	unsigned char val;
	fread(&val,1,1,t->fp);
	return val;
}

static int libtiff_read_word(libtiff_t* t)
{
	unsigned short val;
	if(2 != fread(&val,1,2,t->fp))
	{
		return 0x0000;
	}
	if(!libtiff_is_little_endia(t->magic))
	{
		val = ((val & 0xff) << 8) | (val >> 8);
	}
	return val;
}

static int libtiff_read_dword(libtiff_t* t)
{
	unsigned int val;
	if(4 != fread(&val,1,4,t->fp))
	{
		return 0x0000;
	}
	if(!libtiff_is_little_endia(t->magic))
	{
		int a,b,c,d;
		a = val & 0xff;
		b = (val >> 8) & 0xff;
		c = (val >> 16) & 0xff;
		d = (val >> 24) & 0xff;

		val = (a << 24) | (b << 16) | (c << 8) | d;
	}
	return val;
}

static double libtiff_read_qword(libtiff_t* t)
{
	unsigned int f,d;
	f = libtiff_read_dword(t);
	d = libtiff_read_dword(t);

	return f * 1.0f / d;
}



static int libtiff_read_string(libtiff_t* t,char* buf)
{
	unsigned char val = 0xff;
	int size = 0;
	while(val != 0x00)
	{
		if(1 != fread(&val,1,1,t->fp))
		{
			break;
		}
		*buf = val;
		buf ++;
		size ++;
	};
	*buf = 0;
	return size;
}

static int libtiff_read(libtiff_t* t,int type,int len,int* buf)
{
	int val;
	int i;
	int oft;
	val = libtiff_read_dword(t);
	oft = ftell(t->fp);
	switch(type)
	{
	case IFD_TYPE_BYTE:
	case IFD_TYPE_SBYTE:
		if(len <= 4)
		{
			for(i = 0; i < len; i ++)
			{
				buf[i] = val & 0xff;
				val >>= 8;
			}
		}
		else
		{
			fseek(t->fp,val,SEEK_SET);
			fread(buf,1,len,t->fp);
			fseek(t->fp,oft,SEEK_SET);
		}
		break;
	case IFD_TYPE_WORD:
	case IFD_TYPE_SWORD:
		if(len <= 2)
		{
			for(i = 0; i < len; i ++)
			{
				buf[i] = val & 0xffff;
				val >>= 16;
			}
		}
		else
		{
			fseek(t->fp,val,SEEK_SET);
			for(i = 0; i < len; i ++)
			{
				buf[i] = libtiff_read_word(t);
			}
			fseek(t->fp,oft,SEEK_SET);
		}
		break;
	case IFD_TYPE_DWORD:
	case IFD_TYPE_SDWORD:
		if(len <= 1)
		{
			buf[0] = val;
		}
		else
		{
			fseek(t->fp,val,SEEK_SET);
			for(i = 0; i < len; i ++)
			{
				buf[i] = libtiff_read_dword(t);
			}
			fseek(t->fp,oft,SEEK_SET);

		}
		break;
	case IFD_TYPE_QWORD:
	case IFD_TYPE_SQWORD:
			fseek(t->fp,val,SEEK_SET);
			for(i = 0; i < len; i ++)
			{
				buf[i] = (int)libtiff_read_qword(t);
			}
			fseek(t->fp,oft,SEEK_SET);

	default:
		break;
	}
	return 0;
}

static int libtiff_skip(libtiff_t* t,int size)
{
	fseek(t->fp,size,SEEK_CUR);
	return 0;
}



static int libtiff_decode_ifd(libtiff_t* t)
{
	int ret = -1;
	int count;
	int bpp[4];
	unsigned short tag,type;
	int len;
	int offset = 8;
	int bytecounts = 0;
	int rps;
	if(t->numframes < 1024)
	{
		t->framestart[t->numframes] = ftell(t->fp);
		++t->numframes;
	}
	count = libtiff_read_word(t);
	t->sps = 3;
	while(count --)
	{
		tag = libtiff_read_word(t);
		type = libtiff_read_word(t);
		len = libtiff_read_dword(t);
		switch(tag)
		{
		case IFD_TAG_IMAGEWIDTH:
			libtiff_read(t,type,len,&t->info.width);
			break;
		case IFD_TAG_IMAGEHEIGHT:
			libtiff_read(t,type,len,&t->info.height);
			break;
		case IFD_TAG_BITSPERSAMPLE:
			libtiff_read(t,type,len,bpp);
			t->info.bpp = bpp[0];
			break;
		case IFD_TAG_COMPRESS:
			libtiff_read(t,type,len,&t->compression);
			break;
		case IFD_TAG_PHOTOATTR:
			libtiff_read(t,type,len,&t->isyuv);
			break;
		case IFD_TAG_STRIPOFFSETS:
			libtiff_read(t,type,len,&offset);
			break;
		case IFD_TAG_STRIPBYTECOUNT:
			libtiff_read(t,type,len,&bytecounts);
			if(t->blksize < bytecounts)
			{
				t->blk = (char*)malloc(bytecounts);
				t->blksize = bytecounts;
			}
			break;
		case IFD_TAG_SAMPLESPERPIXEL:
			libtiff_read(t,type,len,&t->sps);
			break;
		case IFD_TAG_ROWSPERSTRIP:
			libtiff_read(t,type,len,&rps);
			break;
		case IFD_TAG_XRES:
			libtiff_read(t,type,len,&t->xres);
			break;
		case IFD_TAG_YRES:
			libtiff_read(t,type,len,&t->yres);
			break;
		case IFD_TAG_PLANAR:
			libtiff_read(t,type,len,&t->isplanar);
			break;
		case IFD_TAG_RESUNIT:
			libtiff_read(t,type,len,&t->unit);
			break;
		case IFD_TAG_PREDICTOR:
			libtiff_read(t,type,len,&t->predictor);
			break;
		default:
			libtiff_skip(t,4);
			break;
		}
		ret = 0;
	}
	if(bytecounts)
	{
		int i,j,k;
		void* gif = NULL;
		int* rgb = t->rgb;
		unsigned char* obuf = NULL;
		int oldpos = ftell(t->fp);
		fseek(t->fp,offset,SEEK_SET);
		fread(t->blk,1,t->blksize,t->fp);
		fseek(t->fp,oldpos,SEEK_SET);

		switch(t->compression)
		{
		case IFD_COMPRESS_NONE:
			obuf = (unsigned char*)t->blk;
			break;
		case IFD_COMPRESS_LZW:
			gif = gifDecodeTiff(t->blk,t->blksize,t->info.width,t->info.height,(char**)&obuf);
			break;
		default:
			break;
		}
		for(i = 0; rgb && obuf && (i < t->info.height);i ++)
		{
			for(j = 0; j < t->info.width; j++,rgb++,obuf += t->sps)
			{
				if(t->predictor && j)
				{
					for(k = 0; k < t->sps; k ++)
					{
						obuf[k] += obuf[k - t->sps];
					}
				}
				*rgb = (obuf[2] << 16) | (obuf[1] << 8) | obuf[0];
			}
		}

		if(gif)
		{
			gifDestroy(gif);
		}

	}
	return ret;
}


void* tiffCreate(char* filename)
{
	libtiff_t* tiff;
	int magic,offset;
	FILE* fp = fopen(filename,"rb");
	if(!fp)
	{
		return NULL;
	}
	if(4 != fread(&magic,1,4,fp))
	{
		fclose(fp);
		return NULL;
	}
	if((magic != 0x002a4949) && (magic != 0x2a004d4d) )
	{
		fclose(fp);
		return NULL;
	}
	tiff = (libtiff_t*)malloc(sizeof(libtiff_t));
	if(!tiff)
	{
		fclose(fp);
		return NULL;
	}
	memset(tiff,0,sizeof(libtiff_t));
	tiff->magic = magic;
	tiff->fp = fp;
	offset = libtiff_read_dword(tiff);
	fseek(fp,offset,SEEK_SET);
	if((ftell(fp) != offset))
	{
		fclose(fp);
		free(tiff);
		return NULL;
	}
	return (void*)tiff;
}


int tiffDecode(void* hdl,libphoto_output_t* attr,int* rgb)
{
	int ret;
	libtiff_t* t = (libtiff_t*)hdl;
	t->rgb = rgb;
	ret = libtiff_decode_ifd(t);
	if(attr)
	{
		*attr = t->info;
		++t->info.index;
	}
	return ret;
}

int tiffDestroy(void* hdl)
{
	libtiff_t* tiff = (libtiff_t*)hdl;
	fclose(tiff->fp);
	if(tiff->blk)
	{
		free(tiff->blk);
	}
	free(tiff);
	return 0;
}


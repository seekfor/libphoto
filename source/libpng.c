#include "common.h"

#ifdef CFG_PNG_SUPPORT

unsigned int bitstreamSwap32(unsigned int val,int bits)
{
	unsigned int ret = 0;
	int i;
	for(i = 0; i < bits; i ++)
	{
		if(val & (1 << i))
		{
			ret |= 1 << (bits - 1 - i);
		}
	}
	return ret;
}

unsigned char bitstreamSwap8(unsigned char val,int bits)
{
	unsigned int ret = 0;
	int i;
	for(i = 0; i < bits; i ++)
	{
		if(val & (1 << i))
		{
			ret |= 1 << (bits - 1 - i);
		}
	}
	return ret;
}

void bitstreamInit(bitstream_t* bt,FILE* fp,unsigned char* buf,int size)
{
	memset(bt,0,sizeof(bitstream_t));
	if(fp)
	{
		buf = bt->pool;
		bt->fp = fp;
		size = fread(bt->pool,1,4096,fp);
	}
	bt->buf = buf + 4;
	bt->bufval = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	bt->size = size - 4;
	bt->rd = 0;
}

int bitstreamReadValue(bitstream_t* bt,int bits,int peek)
{
	int len;
	unsigned int ret;
	ret = (bt->bufval >> bt->rd) & ((1 << bits) - 1);
	if(peek)
	{
		if(bt->fp)
		{
			if(bt->size < 64)
			{
				memcpy(bt->pool,bt->buf,bt->size);
				len = fread(bt->pool + bt->size,1,4096 - bt->size,bt->fp);
				if(len > 0)
				{
					bt->size += len;
				}
				bt->buf = bt->pool;
			}
		}
		bt->rd += bits;
		while(bt->rd >= 8)
		{
			if(bt->size > 0)
			{
				bt->bufval >>= 8;
				bt->bufval |= (bt->buf[0] << 24);
				bt->buf ++;
				bt->rd -= 8;
				bt->size --;
			}
			else
			{
				break;
			}
		}
	}
	return ret;
}

int bitstreamReadBits(bitstream_t* bt,int bits,int peek)
{
	unsigned int ret = bitstreamReadValue(bt,bits,peek);
	return bitstreamSwap32(ret,bits);
}

void bitstreamSkip(bitstream_t* bt,int bits)
{
	bitstreamReadValue(bt,bits,1);
}

void bitstreamAlign(bitstream_t* bt)
{
	if(bt->rd & 0x07)
	{
		bitstreamSkip(bt,8 - (bt->rd & 0x07));
	}
}

int bitstreamMore(bitstream_t* bt)
{
	return bt->size || (bt->rd < 8);
}



void huffmanInitLITable(huffman_table_t* t)
{
	int i;
	t->num = 288;
	for(i = 0; i < 144; i ++)
	{
		t->vlc[i].value = i;
		t->vlc[i].numbits = 8;
		t->vlc[i].code = 0x30 + i;
	}
	for(;i < 256; i ++)
	{
		t->vlc[i].value = i;
		t->vlc[i].numbits = 9;
		t->vlc[i].code = 0x190 + i - 144;
	}
	for(; i < 280; i ++)
	{
		t->vlc[i].value = i;
		t->vlc[i].numbits = 7;
		t->vlc[i].code = i - 256;
	}
	for(;i < 288; i ++)
	{
		t->vlc[i].value = i;
		t->vlc[i].numbits = 8;
		t->vlc[i].code = 0xc0 + i - 280;

	}

}

void huffmanInitDISTable(huffman_table_t* t)
{
	int i;
	for(i = 0; i < 32; i ++)
	{
		t->vlc[i].value = i;
		t->vlc[i].numbits = 5;
		t->vlc[i].code = i;
	}
}

int huffmanNormlize(int* cls,int num,huffman_table_t* huffman)
{
	int i,j;
	int idx;
	int code;
	int blcount[16] = {0};
	int nextcode[16] = {0};
	for(i = 1; i < 16; i ++)
	{
		blcount[i] = 0;
		for(j = 0; j < num;j ++)
		{
			if(cls[j] == i)
			{
				blcount[i] ++;
			}
		}
	}
	code = 0;
	blcount[0] = 0;
	for(i = 1; i < 16; i ++)
	{
		code = (code + blcount[i - 1]) << 1;
		nextcode[i] = code;
	}
	huffman->num = num;
	memset(huffman->vlc,0,sizeof(huffman->vlc));
	for(i = 0; i < num;i ++)
	{
		idx = huffman->vlc[i].numbits = cls[i];
		if(idx)
		{
			huffman->vlc[i].value = i;
			huffman->vlc[i].code = nextcode[idx];
			nextcode[idx] ++;
		}
	}
	return 0;
}

huffman_item_t* huffmanRead(bitstream_t* bt,huffman_table_t* t)
{
	int i;
	int code;
	huffman_item_t* item = &t->vlc[0];
	for(i = 0; i < t->num; i ++,item ++)
	{
		if(!item->numbits)
		{
			continue;
		}
		code = bitstreamReadBits(bt,item->numbits,0);
		if(code == item->code)
		{
			bitstreamSkip(bt,item->numbits);
			return item;
		}
	}
	return (huffman_item_t*)0;
}




#define MAX_WIN_SIZE	(32 * 1024)

static unsigned char win[MAX_WIN_SIZE];
static unsigned char* pwin = &win[MAX_WIN_SIZE];
static int wsize = 0;

static int lz77_add_dict(char* buf,int size)
{
	int skip;
	if((wsize + size) > MAX_WIN_SIZE)
	{
		skip = wsize + size - MAX_WIN_SIZE;
		if(skip)
		{
			memcpy(win,pwin + skip,wsize - skip);
		}
		memcpy(win + wsize - skip,buf,size);
		wsize = MAX_WIN_SIZE;
		pwin = win;
	}
	else
	{
		if(wsize)
		{
			memcpy(pwin - size,pwin,wsize);
		}
		memcpy(pwin - size + wsize ,buf,size);
		pwin -= size;
		wsize += size;
	}
	return wsize;
}

static void lz77_get_dict(char* buf,int d,int l)
{
	unsigned char* item;
	item = (win + MAX_WIN_SIZE) - d;
	if(d >= l)
	{
		memcpy(buf,item,l);
	}
	else
	{
		while(l >= d)
		{
			memcpy(buf,item,d);
			l -= d;
			buf += d;
		}
		if(l)
		{
			memcpy(buf,item,l);
		}
	}
}

int lz77Decode(short int* raw,int size,char* out)
{
	short int l;
	short int d;
	char* oldout = out;
	while(size --)
	{
		l = *raw ++;
		d = *raw ++;
		if(l == -1)
		{
			*out = d & 0xff;
			lz77_add_dict(out,1);
			out++;
		}
		else
		{
			lz77_get_dict(out,d,l);
			lz77_add_dict(out,l);
			out += l;
		}
		
	}
	return (size_t)out - (size_t)oldout;
}

int lz77DecodeFile(short int* raw,int size,FILE* out)
{
	short int l;
	short int d;
	int dsize = 0;
	char obuf[512];
	while(size --)
	{
		l = *raw ++;
		d = *raw ++;
		if(l == -1)
		{
			obuf[0] = d & 0xff;
			lz77_add_dict(obuf,1);
			fwrite(obuf,1,1,out);
			dsize ++;
		}	
		else
		{
			lz77_get_dict(obuf,d,l);
			lz77_add_dict(obuf,l);
			fwrite(obuf,1,l,out);
			dsize += l;
		}
		
	}
	return dsize;
}


static int gz_decode_codelen(bitstream_t* bt,huffman_table_t* clen,int num)
{
	int i;
	int idx;
	int values[] = 
	{
		16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
	};
	int cls[24] = {0};
	for(i = 0; i < num;i ++)
	{
		idx = values[i];
		cls[idx] = bitstreamReadValue(bt,3,1);
	}
	huffmanNormlize(cls,19,clen);
	return 0;
}

static int gz_decode_huffman(bitstream_t* bt,huffman_table_t* hclen,huffman_table_t* huffman,int num)
{
	int i;
	int cls[512] = {0};
	int oldval = 0;
	int numcls = 0;
	int times;
	huffman_item_t* item;
	for(numcls = 0; numcls < num;)
	{
		item = huffmanRead(bt,hclen);
		if(!item)
		{
			return -1;
		}
		switch(item->value)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			oldval = item->value;
			cls[numcls++] = oldval;
			break;
		case 16:
			times = bitstreamReadValue(bt,2,1) + 3;
			for(i = 0; i < times; i ++)
			{
				cls[numcls++] = oldval;
			}
			break;
		case 17:
			times = bitstreamReadValue(bt,3,1) + 3;
			for(i = 0; i < times; i ++)
			{
				cls[numcls++] = 0;
			}
			break;
		case 18:
			times = bitstreamReadValue(bt,7,1) + 11;
			for(i = 0; i < times; i ++)
			{
				cls[numcls++] = 0;
			}
			break;
		}
	}
	if(numcls != num)
	{
		return -1;
	}
	huffmanNormlize(cls,numcls,huffman);
	return 0;
}

static int gz_decode_block(bitstream_t* bt,huffman_table_t* lit,huffman_table_t* dist,short int* raw)
{
	huffman_item_t* item;
	int value;
	int eob = 0;
	int length,distance;
	int size = 0;
	while(!eob)
	{
		item = huffmanRead(bt,lit);
		if(!item)
		{
			return -1;
		}
		length = 0;
		value = item->value;
		switch(value)
		{
		default:
			*raw++ = (short int)0xffff;
			*raw++ = (short int)value;
			size ++;
			continue;
		case 256:
			eob = 1;
			continue;
		case 257:
		case 258:
		case 259:
		case 260:
		case 261:
		case 262:
		case 263:
		case 264:
			length = value - 254;
			break;
		case 265:
		case 266:
		case 267:
		case 268:
			length = 11 + (value - 265) * 2 + bitstreamReadValue(bt,1,1);
			break;
		case 269:
		case 270:
		case 271:
		case 272:
			length = 19 + (value - 269) * 4 + bitstreamReadValue(bt,2,1);
			break;
		case 273:
		case 274:
		case 275:
		case 276:
			length = 35 + (value - 273) * 8 + bitstreamReadValue(bt,3,1);
			break;
		case 277:
		case 278:
		case 279:
		case 280:
			length = 67 + (value - 277) * 16 + bitstreamReadValue(bt,4,1);
			break;
		case 281:
		case 282:
		case 283:
		case 284:
			length = 131 + (value - 281) * 32 + bitstreamReadValue(bt,5,1);
			break;
		case 285:
			length = 258;
			break;
		}

		item = huffmanRead(bt,dist);
		if(!item)
		{
			return size;
		}
		value = item->value;
		switch(value)
		{
		case 0:
		case 1:
		case 2:
		case 3:
			distance = 1 + value;
			break;
		case 4:
		case 5:
			distance = 5 + (value - 4) * 2 + bitstreamReadValue(bt,1,1);
			break;
		case 6:
		case 7:
			distance = 9 + (value - 6) * 4 + bitstreamReadValue(bt,2,1);
			break;
		case 8:
		case 9:
			distance = 17 + (value - 8) * 8 + bitstreamReadValue(bt,3,1);
			break;
		case 10:
		case 11:
			distance = 33 + (value - 10) * 16 + bitstreamReadValue(bt,4,1);
			break;
		case 12:
		case 13:
			distance = 65 + (value - 12) * 32 + bitstreamReadValue(bt,5,1);
			break;
		case 14:
		case 15:
			distance = 129 + (value - 14) * 64 + bitstreamReadValue(bt,6,1);
			break;
		case 16:
		case 17:
			distance = 257 + (value - 16) * 128 + bitstreamReadValue(bt,7,1);
			break;
		case 18:
		case 19:
			distance = 513 + (value - 18) * 256 + bitstreamReadValue(bt,8,1);
			break;
		case 20:
		case 21:
			distance = 1025 + (value - 20) * 512 + bitstreamReadValue(bt,9,1);
			break;
		case 22:
		case 23:
			distance = 2049 + (value - 22) * 1024 + bitstreamReadValue(bt,10,1);
			break;
		case 24:
		case 25:
			distance = 4097 + (value - 24) * 2048 + bitstreamReadValue(bt,11,1);
			break;
		case 26:
		case 27:
			distance = 8193 + (value - 26) * 4096 + bitstreamReadValue(bt,12,1);
			break;
		case 28:
		case 29:
			distance = 16385 + (value - 28) * 8192 + bitstreamReadValue(bt,13,1);
			break;
		default:
			return size;
		}
		*raw++ = (short int)length;
		*raw++ = (short int)distance;
		size ++;
	};
	return size;
}


static int gz_decode_with_file(char* ifile,short int* raw,char* ofile)
{
	int len;
	int nlen;
	unsigned char flag;
	bitstream_t bt;
	int btype,bfinal = 0;
	huffman_table_t hclen;
	huffman_table_t hlit;
	huffman_table_t hdist;
	int numhlit,numhclen,numhdist;
	unsigned char buf[64];
	int size;
	int dsize = 0;
	int block = 0;
	int nofile = 0;
	int noalloc = 1;
	FILE* out = NULL;
	FILE* in = fopen(ifile,"rb");
	if(!in)
	{
		return 0;
	}
	size = fread(buf,1,10,in);

	if(memcmp(buf,"\x1f\x8b\x08",3))
	{
		fclose(in);
		return 0;
	}

	if(ofile)
	{
		out = fopen(ofile,"wb");
		if(!out)
		{
			nofile = 1;
		}
	}
	if(!raw)
	{
		raw = (short int*)malloc(256 * 1024);
		if(!raw)
		{
			fclose(in);
			if(out)
			{
				fclose(out);
			}
			return 0;
		}
		noalloc = 0;
	}

	flag = buf[3];
	
	if(flag & 0x04)
	{
		fread(buf,1,2,in);
		len = (buf[1] << 8) | buf[0];
		fseek(in,len,SEEK_CUR);
	}
	if(flag & 0x08)
	{
		char oname[256] = {0};
		char* name = oname;
		while(!feof(in))
		{
			fread(name,1,1,in);
			if(name[0] == 0x00)
			{
				break;
			}
			name++;
		}
		if(!strcmp(oname,""))
		{
			sprintf(oname,"%s.unzip",ifile);
		}
		if(!ofile)
		{
			out = fopen(oname,"wb");
			if(!out)
			{
				fclose(in);
				free(raw);
				return 0;
			}
		}
	}
	if(flag & 0x10)
	{
		while(!feof(in))
		{
			fread(buf,1,1,in);
			if(buf[0] == 0x00)
			{
				break;
			}
		}
	}
	if(flag & 0x02)
	{
		fseek(in,2,SEEK_CUR);
	}
	bitstreamInit(&bt,in,NULL,0);
	while(!bfinal && bitstreamMore(&bt))
	{
		block ++;
		bfinal = bitstreamReadValue(&bt,1,1);
		btype = bitstreamReadValue(&bt,2,1);
		switch(btype)
		{
		case 0x00:/*no compression*/
			bitstreamAlign(&bt);
			len = bitstreamReadValue(&bt,16,1);
			nlen = bitstreamReadValue(&bt,16,1);
			if((len + nlen) != 0xffff)
			{
				bfinal = 1;
				continue;
			}
			while(len --)
			{
				buf[0] = bitstreamReadValue(&bt,8,1);
				if(out)
				{
					fwrite(buf,1,1,out);
				}
				else
				{
					memcpy(ofile + dsize,buf,1);
				}
				dsize ++;
			}
			continue;
		case 0x01:/*FIXED huffman table*/
			huffmanInitLITable(&hlit);
			huffmanInitDISTable(&hdist);
			size = gz_decode_block(&bt,&hlit,&hdist,raw);
			break;
		case 0x02:/*DYNAMIC huffman table*/
			numhlit = bitstreamReadValue(&bt,5,1) + 257;
			numhdist = bitstreamReadValue(&bt,5,1) + 1;
			numhclen = bitstreamReadValue(&bt,4,1) + 4;
			gz_decode_codelen(&bt,&hclen,numhclen);
			gz_decode_huffman(&bt,&hclen,&hlit,numhlit);
			gz_decode_huffman(&bt,&hclen,&hdist,numhdist);
			size = gz_decode_block(&bt,&hlit,&hdist,raw);
			break;
		default:
			bfinal = 1;
			continue;
		}
		if(size > 0)
		{
			if(out)
			{
				len = lz77DecodeFile(raw,size,out);
			}
			else
			{
				len = lz77Decode(raw,size,ofile + dsize);
			}
			dsize += len;
		}
	};
	fclose(in);
	if(out)
	{
		fclose(out);
	}
	if(!noalloc)
	{
		free(raw);
	}
	return dsize;
}

int gzipDecode(unsigned char* buf,int size,short int* raw,char* out)
{
	int len;
	int nlen;
	unsigned char flag;
	bitstream_t bt;
	int btype,bfinal = 0;
	int blksize;
	huffman_table_t hclen;
	huffman_table_t hlit;
	huffman_table_t hdist;
	int numhlit,numhclen,numhdist;
	int noalloc = 1;
	FILE* wr = NULL;

	if(!size)
	{
		return gz_decode_with_file((char*)buf,raw,out);
	}
	if(out)
	{
		wr = fopen(out,"wb");
	}

	if(!memcmp(buf,"\x1f\x8b\x08",3))
	{
		flag = buf[3];
		buf += 10;
		size -= 10;
		
		if(flag & 0x04)
		{
			len = (buf[1] << 8) | buf[0];
			buf += 2 + len;
			size -= 2 + len;
		}
		if(flag & 0x08)
		{
			char oname[256] = {0};
			strcpy(oname,(char*)buf);
			len = strlen(oname) + 1;
			if(!out)
			{
				wr = fopen(oname,"wb");
			}
			buf += len;
			size -= len;
		}
		if(flag & 0x10)
		{
			while(*buf != 0x00)
			{
				size --;
				buf ++;
			}
			buf ++;
			size --;
		}
		if(flag & 0x02)
		{
			buf += 2;
			size -= 2;
		}
	}

	if(!raw)
	{
		raw = (short int*)malloc(256 * 1024);
		if(!raw)
		{
			return 0;
		}
		noalloc = 0;
	}

	bitstreamInit(&bt,NULL,buf,size);
	size = 0;
	while(!bfinal && bitstreamMore(&bt))
	{
		blksize = 0;
		bfinal = bitstreamReadValue(&bt,1,1);
		btype = bitstreamReadValue(&bt,2,1);
		switch(btype)
		{
		case 0x00:/*no compression*/
			bitstreamAlign(&bt);
			len = bitstreamReadValue(&bt,16,1);
			nlen = bitstreamReadValue(&bt,16,1);
			if((len + nlen) != 0xffff)
			{
				bfinal = 1;
				continue;
			}
			while(len --)
			{
				unsigned char ch;
				if(wr == NULL)
				{
					*out++ = bitstreamReadValue(&bt,8,1);
					size ++;
				}
				else
				{
					ch = bitstreamReadValue(&bt,8,1);
					fwrite(&ch,1,1,wr);	
				}
				size ++;
			}
			continue;
		case 0x01:/*FIXED huffman table*/
			huffmanInitLITable(&hlit);
			huffmanInitDISTable(&hdist);
			blksize = gz_decode_block(&bt,&hlit,&hdist,raw);
			break;
		case 0x02:/*DYNAMIC huffman table*/
			numhlit = bitstreamReadValue(&bt,5,1) + 257;
			numhdist = bitstreamReadValue(&bt,5,1) + 1;
			numhclen = bitstreamReadValue(&bt,4,1) + 4;
			gz_decode_codelen(&bt,&hclen,numhclen);
			gz_decode_huffman(&bt,&hclen,&hlit,numhlit);
			gz_decode_huffman(&bt,&hclen,&hdist,numhdist);
			blksize = gz_decode_block(&bt,&hlit,&hdist,raw);
			break;
		default:
			bfinal = 1;
			continue;
		}
		if(blksize > 0)
		{
			if(wr == NULL)
			{
				len = lz77Decode(raw,blksize,out);
				out += len;
			}
			else
			{
				len = lz77DecodeFile(raw,blksize,wr);
			}
			size += len;
		}
	};
	if(!noalloc)
	{
		free(raw);
	}
	return size;
}




typedef struct
{
	FILE* fp;
	libphoto_output_t info;
	unsigned int pal[256 * 3];
	int numpal;
	int* rgb;
	char* fdat;
	char* odat;
	char* idat;
	size_t dsize;
	char* block;
	size_t msize;
}libpng_t;

static int png_get_linesize(libpng_t* png)
{
	int linesize;
	switch(png->info.colortype)
	{
		case 0x00:/*Gray image,bpp should be 1/2/4/8*/
			linesize = png->info.width * png->info.bpp / 8;
			break;
		default:
		case 0x02:/*true color,bpp should be 8/16*/
			linesize = 3 * png->info.width * png->info.bpp / 8;
			break;
		case 0x03:/*Indexed color,bpp should be 1/2/4/8*/
			linesize = png->info.width * png->info.bpp / 8;
			break;
		case 0x04:/*Gray image with alpha,bpp should be 8/16*/
			linesize = 2 * png->info.width * png->info.bpp / 8;
			break;
		case 0x06:/*true color with alpha,bpp should be 8/16*/
			linesize = 4 * png->info.width * png->info.bpp / 8;
			break;
	}
	return linesize;
}

static int png_decode_ihdr(libpng_t* png,unsigned char* buf)
{
	int linesize;
	libphoto_output_t* info = &png->info;
	info->width = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	info->height = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
	info->bpp = buf[8];
	info->colortype = buf[9];
	info->interlace = buf[12];
	if(!png->rgb)
	{
		linesize = png_get_linesize(png);
		png->rgb = (int*)malloc(linesize * info->height * sizeof(int));
		png->odat = (char*)malloc((linesize + 1) * info->height);
		png->idat = (char*)malloc(linesize * info->height);
		png->fdat = (char*)malloc(linesize * info->height);
	}
	info->rgb = png->rgb;
	printf("width = %d,height = %d,bpp = %d,interlace = %d,color type = %d\r\n",info->width,info->height,info->bpp,info->interlace,buf[9]);
	return 0;
}

static int png_decode_plte(libpng_t* png,unsigned char* buf)
{
	unsigned int* pal = png->pal;
	int i;
	png->numpal = (png->info.bpp > 8)?256:(2 << png->info.bpp);
	for(i = 0; i < png->numpal; i ++)
	{
		pal[i] = (buf[2] << 16) | (buf[1] << 8) | buf[0];
		buf += 3;
	}
	return 0;
}

static int png_decode_idat(libpng_t* png,unsigned char* buf,int size)
{
	memcpy(png->idat + png->dsize,buf,size);
	png->dsize += size;
	return png->dsize;
}

static int png_filter_picture(libpng_t* png)
{
	unsigned char* fdat = (unsigned char*)png->fdat;
	int num;
	int i;
	int j;
	int k;
	int x;
	unsigned int pa,pb,pc,pr;
	unsigned int p;
	unsigned int a,b,c;
	unsigned char type;
	char* src = (char*)png->odat;
	int linesize = png_get_linesize(png);
	int w;
	int h = png->info.height;
	switch(png->info.colortype)
	{
	case 2:
		num = 3;
		break;
	case 4:
		num = 2;
		break;
	case 6:
		num = 4;
		break;
	default:
		num = 1;
		break;

	}
	w = linesize / num;
	
	for(i = 0; i < h; i ++)
	{
		type = *src ++;
		for(j = 0; j < w; j ++)
		{
			for(k = 0;k < num; k ++,fdat ++,src ++)
			{
				x = *(src + 0);
				a = (j == 0)?0x00:*(fdat - num);
				b = (i == 0)?0x00:*(fdat - linesize);
				c = (i == 0)?0x00:((j == 0)?0x00:*(fdat - linesize - num));
				switch(type)
				{
				default:
					return -1;
				case 0:/*None*/
					*fdat = x;
					break;
				case 1:/*Sub*/
					*fdat = x + a;
					break;
				case 2:/*Up*/
					*fdat = x + b;
					break;
				case 3:/*Average*/
					*fdat = x + ((a + b) >> 1);
					break;
				case 4:/*Paeth*/
					p = a + b - c;
					pa = abs(p - a);
					pb = abs(p - b);
					pc = abs(p - c);
					if((pa <= pb) && (pa <= pc) )
					{
						pr = a;
					}
					else if(pb <= pc)
					{
						pr = b;
					}
					else
					{
						pr = c;
					}
					*fdat = x + pr;
					break;
				}
			}
		}
	}
	return 0;
}

static int png_convert_picture(libpng_t* png)
{
	unsigned int* rgb = (unsigned int*)png->info.rgb;
	int i;
	int j;
	if(png->info.bpp == 1)
	{
	}
	else if(png->info.bpp == 2)
	{
	}
	else if(png->info.bpp == 4)
	{

	}
	else if(png->info.bpp == 8)
	{
		unsigned char* src = (unsigned char*)png->fdat;
		switch(png->info.colortype)
		{
		case 0x00:/*Gray image*/
			break;
		case 0x01:
		case 0x05:/*no these cases*/
			break;
		case 0x02:/*RGB color*/
			for(i = 0; i < png->info.height; i ++)
			{
				for(j = 0; j < png->info.width; j ++,src += 3)
				{
					*rgb ++ = 0xff000000 | (src[2] << 16) | (src[1] << 8) | src[0];
				}
			}
			break;
		case 0x03:/*Indexed color*/
			break;
		case 0x04:/*Gray image with alpha*/
			break;
		case 0x06:/*RGBA color*/
			for(i = 0; i < png->info.height; i ++)
			{
				for(j = 0; j < png->info.width; j ++,src += 4)
				{
					*rgb++ = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
				}
			}
			break;
		}
	}
	else if(png->info.bpp == 16)
	{
	}
	return 0;
}

static int png_decode_picture(libpng_t* png)
{
	/*
		MARK:the first two bytes are zlib header,and ignored it now!
		<CMF> <FLG>
		CMF:always it is 0x78
			D0 ~ D3,compression method,only 8 is avail
			D4 ~ D7,compression info
		FLG:
			D0 ~ D4,FCHECK,check bits for CMF & FLG
			D5,FDICT,preset dictionary
			D6 ~ D7,compression level
	*/
	int len = gzipDecode((unsigned char*)png->idat + 2,png->dsize - 2,NULL,(char*)png->odat);
	png_filter_picture(png);
	return png_convert_picture(png);
}

static int png_decode_block(libpng_t* png)
{
	unsigned char mem[8] = {0};
	char* buf;
	size_t size;
	if(4 != fread(mem,1,4,png->fp))
	{
		return 1;
	}
	size = ((mem[0] << 24) | (mem[1] << 16) | (mem[2] << 8) | mem[3]);
	size += 8;
	if(!png->block || (size > png->msize))
	{
		if(png->block)
		{
			free(png->block);
		}
		if(size < 65536)
		{
			png->block = (char*)malloc(65536);
			png->msize = 65536;
		}
		else
		{
			png->msize = size;
			png->block = (char*)malloc(size);
		}
		if(!png->block)
		{
			return 3;
		}
	}
	if(size != fread(png->block,1,size,png->fp))
	{
		return 4;
	}
	buf = png->block;
	if(!memcmp(buf,"IHDR",4))
	{
		png_decode_ihdr(png,(unsigned char*)buf + 4);
	}
	else if(!memcmp(buf,"PLTE",4))
	{
		png_decode_plte(png,(unsigned char*)buf + 4);
	}
	else if(!memcmp(buf,"IDAT",4))
	{
		png_decode_idat(png,(unsigned char*)buf + 4,size - 8);
	}
	else if(!memcmp(buf,"IEND",4))
	{
		return png_decode_picture(png);
	}
	else
	{
	}
	return -1;
}

void* pngCreate(char* filename)
{
	FILE* fp;
	char buf[8];
	libpng_t* png;
	fp = fopen(filename,"rb");
	if(!fp)
	{
		return NULL;
	}
	fread(buf,1,8,fp);
	if(memcmp(buf,"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a",8))
	{
		fclose(fp);
		return NULL;
	}
	png = (libpng_t*)malloc(sizeof(libpng_t));
	if(!png)
	{
		return NULL;
	}
	memset(png,0,sizeof(libpng_t));
	png->fp = fp;

	return png;
}

int pngDecode(void* hdl,libphoto_output_t* attr)
{
	int ret;
	libpng_t* png = (libpng_t*)hdl;
	do
	{
		ret = png_decode_block(png);
		if(ret == 0)
		{
			if(attr)
			{
				*attr = png->info;
			}
			break;
		}
	}while(ret < 0);
	return ret;
}

int pngDestroy(void* hdl)
{
	libpng_t* png = (libpng_t*)hdl;
	fclose(png->fp);
	free(png->block);
	free(png->idat);
	free(png->odat);
	free(png->fdat);
	free(png->rgb);
	free(png);
	return 0;
}



#endif


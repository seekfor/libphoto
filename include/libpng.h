#ifndef __LIBPNG_H__
#define __LIBPNG_H__


typedef struct
{
	FILE* fp;
	unsigned char pool[4096];
	unsigned char* buf;
	unsigned int bufval;
	int size;
	int rd;
}bitstream_t;


typedef struct
{
	int numbits;
	int value;
	int code;
}huffman_item_t;


typedef struct
{
	int num;
	huffman_item_t vlc[512];
}huffman_table_t;


#ifdef __cplusplus
extern "C"
{
#endif

	void bitstreamInit(bitstream_t* bt,FILE* fp,unsigned char* buf,int size);
	int bitstreamReadValue(bitstream_t* bt,int bits,int peek);
	int bitstreamReadBits(bitstream_t* bt,int bits,int peek);
	void bitstreamSkip(bitstream_t* bt,int bits);
	void bitstreamAlign(bitstream_t* bt);
	int bitstreamMore(bitstream_t* bt);
	unsigned int bitstreamSwap32(unsigned int val,int bits);
	unsigned char bitstreamSwap8(unsigned char val,int bits);

	int gzipDecode(unsigned char* buf,int size,short int* raw,char* out);


	void huffmanInitLITable(huffman_table_t* t);
	void huffmanInitDISTable(huffman_table_t* t);
	int huffmanNormlize(int* cls,int num,huffman_table_t* huffman);
	huffman_item_t* huffmanRead(bitstream_t* bt,huffman_table_t* t);


	int lz77Decode(short int* raw,int size,char* out);
	int lz77DecodeFile(short int* raw,int size,FILE* out);



	void* pngCreate(char* filename);
	int pngDecode(void* hdl,libphoto_output_t* attr);
	int pngDestroy(void* hdl);


#ifdef __cplusplus
}
#endif





#endif


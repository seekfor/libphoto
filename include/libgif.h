#ifndef __LIBGIF_H__
#define __LIBGIF_H__

#define CFG_MAX_STRING_SIZE		(8192)


#ifdef __cplusplus
extern "C"
{
#endif
	void* gifCreate(char* filename);
	void* gifDecodeTiff(char* buf,int size,int width,int height,char** outbuf);
	int gifDecode(void* hdl,libphoto_output_t* attr);
	int gifDestroy(void* hdl);


#ifdef __cplusplus
}
#endif

#endif

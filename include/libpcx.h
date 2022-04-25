#ifndef __LIBPCX_H__
#define __LIBPCX_H__



#ifdef __cplusplus
extern "C"
{
#endif
	void* pcxCreate(char* filename);
	int pcxDecode(void* hdl,libphoto_output_t* info);
	int pcxDestroy(void* hdl);
	
	int pcxEncode(int* rgb,int width,int height,char* bmp);
#ifdef __cplusplus
}
#endif


#endif


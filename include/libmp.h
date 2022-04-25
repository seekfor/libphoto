#ifndef __LIBBMP_H__
#define __LIBBMP_H__


#ifdef __cplusplus
extern "C"
{
#endif
	void* bmpCreate(char* filename);
	int bmpDecode(void* hdl,libphoto_output_t* info);
	int bmpDestroy(void* hdl);

	int bmpEncode(int* rgb,int width,int height,char* bmp);
#ifdef __cplusplus
}
#endif

#endif

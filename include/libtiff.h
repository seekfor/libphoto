#ifndef __LIBTIFF_H__
#define __LIBTIFF_H__

#ifdef __cplusplus
extern "C"
{
#endif
	void* tiffCreate(char* filename);
	int tiffDecode(void* hdl,libphoto_output_t* attr,int* rgb);
	int tiffDestroy(void* hdl);

#ifdef __cplusplus
}
#endif



#endif


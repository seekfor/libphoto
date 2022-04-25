#ifndef __LIBGIF_H__
#define __LIBGIF_H__

#define CFG_MAX_STRING_SIZE		(8192)


#ifdef __cplusplus
extern "C"
{
#endif
	void* libgifCreate(char* filename);
	int libgifDecode(void* hdl,libphoto_output_t* attr);
	int libgifDestroy(void* hdl);
#ifdef __cplusplus
}
#endif

#endif

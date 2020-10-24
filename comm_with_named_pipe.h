#ifndef __comm_with_named_pipe_h__
#define __comm_with_named_pipe_h__

#include <stdint.h>

typedef void (*fn_comm_result)(int code, uint8_t* data, size_t size, void* p);
int comm_with_named_pipe(const char* pipe_name, uint8_t* data, size_t size, int need_response, fn_comm_result pfn, void* p);

#endif // __comm_with_named_pipe_h__

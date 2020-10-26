#ifndef __WIN_CMD_WRAPPER_H__
#define __WIN_CMD_WRAPPER_H__

#include <wchar.h>

int run_command_wrapper(const wchar_t* cmd, const wchar_t* argv_fmt, ...);
int run_command(const wchar_t* cmd, const wchar_t* args);

#endif // __WIN_CMD_WRAPPER_H__

#ifndef __PROXY_SETTINGS_H__
#define __PROXY_SETTINGS_H__

#ifdef __cplusplus
extern "C" {
#endif

//
//    //set proxy
//    enable_system_proxy(L"62.81.236.23", 80);
//
//    //disable proxy 
//    disable_system_proxy();
//

BOOL enable_system_proxy(const wchar_t* proxy_full_addr, int port);
BOOL disable_system_proxy(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __PROXY_SETTINGS_H__

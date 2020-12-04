#ifndef __CMD_HANDLER_H__
#define __CMD_HANDLER_H__

#if 1
#define SVC_NAME                "ssrRouteService"
#define SVC_NAME_W             L"ssrRouteService"
#define PIPE_NAME               "ssrRouteServicePipe"
#define TAP_DEVICE_NAME        L"ssr-route-tap0"
#define FILTER_PROVIDER_NAME    SVC_NAME_W
#else
#define SVC_NAME "OutlineService"
#define SVC_NAME_W L"OutlineService"
#define PIPE_NAME "OutlineServicePipe"
#define TAP_DEVICE_NAME L"outline-tap0"
#define FILTER_PROVIDER_NAME SVC_NAME_W
#endif

extern BOOL svc_message_handler(const BYTE* msg, size_t msg_size, BYTE* result, size_t* result_size, void* p);
extern void* handler_data_p;

#endif // __CMD_HANDLER_H__

#ifndef __CMD_HANDLER_H__
#define __CMD_HANDLER_H__

#define SVC_NAME                "ssrRouteService"
#define SVC_NAME_W             L"ssrRouteService"
#define PIPE_NAME               "ssrRouteServicePipe"
#define TAP_DEVICE_NAME        L"ssr-route-tap0"
#define FILTER_PROVIDER_NAME    SVC_NAME_W

BOOL svc_message_handler(const BYTE* msg, size_t msg_size, BYTE* result, size_t* result_size, void* p);

#endif // __CMD_HANDLER_H__

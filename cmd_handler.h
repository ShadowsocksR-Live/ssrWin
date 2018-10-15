#ifndef __CMD_HANDLER_H__
#define __CMD_HANDLER_H__

#define SVC_NAME "OutlineService"
#define PIPE_NAME "OutlineServicePipe"
#define TAP_DEVICE_NAME L"outline-tap0"

BOOL svc_message_handler(const BYTE *msg, size_t msg_size, BYTE *result, size_t *result_size, void *p);

#endif // __CMD_HANDLER_H__

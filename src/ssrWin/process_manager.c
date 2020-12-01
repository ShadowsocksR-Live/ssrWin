#include "process_manager.h"

#include "net_adapter_enum.h"
#include "cmd_handler.h"
#include "server_connectivity.h"

struct tap_device_info {
    wchar_t device_name[MAX_PATH/4];
    wchar_t device_ip[MAX_PATH/4];
    // wchar_t router_ip[MAX_PATH/4];
};

void extract_tap_adapter_info(int *stop, PIP_ADAPTER_ADDRESSES addresse, void *p) {
    struct tap_device_info *info = (struct tap_device_info *)p;
    PIP_ADAPTER_UNICAST_ADDRESS_LH unicast = NULL;

    if (addresse->IfType==IF_TYPE_SOFTWARE_LOOPBACK) {
        return;
    }
    if (lstrcmpW(addresse->FriendlyName, info->device_name) != 0) {
        return;
    }

    unicast = addresse->FirstUnicastAddress;
    while (unicast) {
        if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
            draft_inet_ntop(unicast->Address.lpSockaddr, info->device_ip, ARRAYSIZE(info->device_ip));
            break;
        }
        unicast = unicast->Next;
    }
    if (stop) { *stop = TRUE; }
}


const wchar_t * testTapDevice(wchar_t *buffer, size_t count) {
    struct tap_device_info info = { TAP_DEVICE_NAME };
    enum_adapter_info(AF_UNSPEC, extract_tap_adapter_info, &info);
    lstrcpynW(buffer, info.device_ip, (int)count);
    return lstrlenW(buffer) ? buffer : NULL;
}

int _main(int argc, char *argv[]) {
    wchar_t ip[MAX_PATH] = { 0 };
    if (testTapDevice(ip, ARRAYSIZE(ip))) {

    }
    return 0;
}

#include <WinSock2.h>
#include <wininet.h>
#pragma comment(lib, "Wininet.lib")

#include "proxy_settings.h"

BOOL enable_system_proxy(const wchar_t* proxy_addr, int port)
{
    //proxy_full_addr : eg L"210.78.22.87:8000"
    INTERNET_PER_CONN_OPTION_LISTW list;
    BOOL    bReturn;
    DWORD   dwBufSize = sizeof(list);
    INTERNET_PER_CONN_OPTIONW options[3];
    wchar_t proxy_full_addr[MAX_PATH] = { 0 };

    // Fill out list struct.
    list.dwSize = sizeof(list);
    // NULL == LAN, otherwise connectoid name.
    list.pszConnection = NULL;
    // Set three options.
    list.dwOptionCount = 3;
    list.pOptions = options;

    // Set flags.
    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[0].Value.dwValue = PROXY_TYPE_DIRECT | PROXY_TYPE_PROXY;

    // Set proxy name.
    list.pOptions[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    wsprintfW(proxy_full_addr, L"%s:%d", proxy_addr, port);
    list.pOptions[1].Value.pszValue = (LPWSTR)proxy_full_addr;

    // Set proxy override.
    list.pOptions[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    list.pOptions[2].Value.pszValue = L"<local>";

    // Set the options on the connection.
    bReturn = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, dwBufSize);

    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH , NULL, 0);
    return bReturn;
}

BOOL disable_system_proxy(const wchar_t* conn_name)
{
    //conn_name: active connection name. 
    INTERNET_PER_CONN_OPTION_LISTW list;
    BOOL    bReturn;
    DWORD   dwBufSize = sizeof(list);
    INTERNET_PER_CONN_OPTIONW options[3];

    // Fill out list struct.
    list.dwSize = sizeof(list);
    // NULL == LAN, otherwise connectoid name.
    list.pszConnection = NULL;
    // Set three options.
    list.dwOptionCount = 3;
    list.pOptions = options;

    // Set flags.
    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[0].Value.dwValue = PROXY_TYPE_DIRECT;

    list.pOptions[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].Value.pszValue = L"";

    list.pOptions[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    list.pOptions[2].Value.pszValue = L"";

    // Set the options on the connection.
    bReturn = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, dwBufSize);
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH , NULL, 0);
    return bReturn;
}

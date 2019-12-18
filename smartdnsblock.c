// Copyright 2019 The ssrlive
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// If this doesn't come first, there will be compile errors.
#include <winsock2.h>

#include <iphlpapi.h>

#include <fwpmtypes.h>
#include <fwpmu.h>
#include <rpcdce.h>
#include <stdio.h>
#include <tchar.h>

#include "smartdnsblock.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "rpcrt4.lib")

ULONG GET_ADAPTERS_ADDRESSES_BUFFER_SIZE = 16384;

PCWSTR SUBLAYER_NAME = L"Smart DNS Block";

UINT64 LOWER_FILTER_WEIGHT = 10;
UINT64 HIGHER_FILTER_WEIGHT = 20;

//////////////////////////////////////////////////////////////////////////
HANDLE engine = 0;

FWPM_SESSION0 session = { 0 };

FWPM_SUBLAYER0 sublayer = { 0 };
FWPM_FILTER_CONDITION0 udpBlockConditions[2] = { 0 };
FWPM_FILTER0 udpBlockFilter = { 0 };
UINT64 filterId = 0;

FWPM_FILTER_CONDITION0 tapDeviceWhitelistCondition[1];
FWPM_FILTER0 tapDeviceWhitelistFilter;
UINT64 filterId2 = 0;

int begin_smart_dns_block(const wchar_t * tap_device_name, const wchar_t *filter_provider_name) {
    UINT32 interfaceIndex;
    PIP_ADAPTER_ADDRESSES adapterAddress;
    PIP_ADAPTER_ADDRESSES adaptersAddresses = NULL;
    DWORD result;
    BOOL success = FALSE;

    do {
        // Lookup the interface index of outline-tap0.
        adaptersAddresses = (IP_ADAPTER_ADDRESSES *)malloc(GET_ADAPTERS_ADDRESSES_BUFFER_SIZE);
        result = GetAdaptersAddresses(AF_INET, 0, NULL, adaptersAddresses, &GET_ADAPTERS_ADDRESSES_BUFFER_SIZE);
        if (result != NO_ERROR) {
            fprintf(stdout, "could not fetch network device list: %d\n", (int)result);
            break;
        }

        adapterAddress = adaptersAddresses;
        while (adapterAddress && wcscmp(tap_device_name, adapterAddress->FriendlyName) != 0) {
            adapterAddress = adapterAddress->Next;
        }

        if (!adapterAddress) {
            fwprintf(stderr, L"could not find %s\n", tap_device_name);
            break;
        }

        interfaceIndex = adapterAddress->IfIndex;
        fwprintf(stdout, L"found %s at index %d\n", tap_device_name, (int)interfaceIndex);

        // Connect to the filtering engine. By using a dynamic session, all of our changes are
        // *non-destructive* and will vanish on exit/crash/whatever.
        memset(&session, 0, sizeof(session));
        session.flags = FWPM_SESSION_FLAG_DYNAMIC;
        result = FwpmEngineOpen0(NULL, RPC_C_AUTHN_DEFAULT, NULL, &session, &engine);
        if (result != ERROR_SUCCESS) {
            fprintf(stderr, "could not connect to to filtering engine: %d\n", (int)result);
            break;
        }
        fprintf(stdout, "connected to filtering engine\n");

        // Create our own sublayer.
        //
        // This is recommended by the API documentation to avoid weird interactions with other
        // applications' filters:
        //   https://docs.microsoft.com/en-us/windows/desktop/fwp/best-practices
        //
        // Notes:
        //  - Without a unique ID our filters will be added to FWPM_SUBLAYER_UNIVERSAL *even if they
        //    reference this new sublayer*.
        //  - Since the documentation doesn't say much about sublayer weights, we specify the highest
        //    possible sublayer weight. This seems to work well.
        memset(&sublayer, 0, sizeof(sublayer));
        UuidCreate(&sublayer.subLayerKey);
        sublayer.displayData.name = (PWSTR)SUBLAYER_NAME;
        sublayer.weight = MAXUINT16;

        result = FwpmSubLayerAdd0(engine, &sublayer, NULL);
        if (result != ERROR_SUCCESS) {
            fprintf(stderr, "could not create filtering sublayer: %d\n", result);
            break;
        }
        fprintf(stdout, "created filtering sublayer\n");

        // Create our filters:
        //  - The first blocks all UDP traffic bound for port 53.
        //  - The second whitelists all traffic on the TAP device.
        //
        // Crucially, the second has a higher weight.
        //
        // Note:
        //  - Since OutlineService adds a blanket block on all IPv6 traffic, we only need to create IPv4
        //    filters.
        //  - Thanks to the simplicity of the filters and how they will be automatically destroyed on
        //    exit, there's no need to use a transaction here.

        // Blanket UDP port 53 block.
        udpBlockConditions[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
        udpBlockConditions[0].matchType = FWP_MATCH_EQUAL;
        udpBlockConditions[0].conditionValue.type = FWP_UINT8;
        udpBlockConditions[0].conditionValue.uint16 = IPPROTO_UDP;
        udpBlockConditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
        udpBlockConditions[1].matchType = FWP_MATCH_EQUAL;
        udpBlockConditions[1].conditionValue.type = FWP_UINT16;
        udpBlockConditions[1].conditionValue.uint16 = 53;

        memset(&udpBlockFilter, 0, sizeof(udpBlockFilter));
        udpBlockFilter.filterCondition = udpBlockConditions;
        udpBlockFilter.numFilterConditions = 2;
        udpBlockFilter.displayData.name = (PWSTR)filter_provider_name;
        udpBlockFilter.subLayerKey = sublayer.subLayerKey;
        udpBlockFilter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;
        udpBlockFilter.action.type = FWP_ACTION_BLOCK;
        udpBlockFilter.weight.type = FWP_UINT64;
        udpBlockFilter.weight.uint64 = &LOWER_FILTER_WEIGHT;
        result = FwpmFilterAdd0(engine, &udpBlockFilter, NULL, &filterId);
        if (result != ERROR_SUCCESS) {
            fprintf(stderr, "could not block port 53: %d\n", (int)result);
            break;
        }
        fprintf(stdout, "port 53 blocked with filter %d", (int)filterId);

        // Whitelist all traffic on the TAP device.
        tapDeviceWhitelistCondition[0].fieldKey = FWPM_CONDITION_LOCAL_INTERFACE_INDEX;
        tapDeviceWhitelistCondition[0].matchType = FWP_MATCH_EQUAL;
        tapDeviceWhitelistCondition[0].conditionValue.type = FWP_UINT32;
        tapDeviceWhitelistCondition[0].conditionValue.uint32 = interfaceIndex;

        memset(&tapDeviceWhitelistFilter, 0, sizeof(tapDeviceWhitelistFilter));
        tapDeviceWhitelistFilter.filterCondition = tapDeviceWhitelistCondition;
        tapDeviceWhitelistFilter.numFilterConditions = 1;
        tapDeviceWhitelistFilter.displayData.name = (PWSTR)filter_provider_name;
        tapDeviceWhitelistFilter.subLayerKey = sublayer.subLayerKey;
        tapDeviceWhitelistFilter.layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4;
        tapDeviceWhitelistFilter.action.type = FWP_ACTION_PERMIT;
        tapDeviceWhitelistFilter.weight.type = FWP_UINT64;
        tapDeviceWhitelistFilter.weight.uint64 = &HIGHER_FILTER_WEIGHT;

        result = FwpmFilterAdd0(engine, &tapDeviceWhitelistFilter, NULL, &filterId2);
        if (result != ERROR_SUCCESS) {
            fwprintf(stderr, L"could not whitelist traffic on %s : %d\n", tap_device_name, (int)result);
            break;
        }
        fwprintf(stdout, L"whitelisted traffic on %s with filter %d\n", tap_device_name, filterId2);
        success = TRUE;
    } while (0);
    if (adaptersAddresses) {
        free(adaptersAddresses);
    }
    if (success == FALSE) {
        end_smart_dns_block();
    }
    return result;
}

void end_smart_dns_block(void) {
    if (engine) {
        if (filterId) {
            FwpmFilterDeleteById0(engine, filterId);
        }
        if (filterId2) {
            FwpmFilterDeleteById0(engine, filterId2);
        }
        FwpmSubLayerDeleteByKey0(engine, &sublayer.subLayerKey);
        FwpmEngineClose0(engine);
    }
    filterId = 0;
    filterId2 = 0;
    engine = NULL;
}

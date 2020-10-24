#ifndef __SMART_DNS_BLOCK_H__
#define __SMART_DNS_BLOCK_H__

int begin_smart_dns_block(const wchar_t* tap_device_name, const wchar_t* filter_provider_name);
void end_smart_dns_block(void);

#endif // __SMART_DNS_BLOCK_H__

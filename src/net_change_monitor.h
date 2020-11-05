#pragma once

struct net_change_ctx;

struct net_change_ctx* net_status_start_monitor(void (*notification)(void* p), void* p);
void net_status_stop_monitor(struct net_change_ctx* ctx);

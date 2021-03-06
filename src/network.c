/* Copyright (c) 2014-2016, Marel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "socketcan.h"
#include "canopen.h"
#include "canopen/nmt.h"
#include "canopen/heartbeat.h"
#include "canopen/network.h"
#include "canopen/sdo.h"
#include "net-util.h"
#include "time-utils.h"
#include "sock.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

int co_net_send_nmt(const struct sock* sock, int cs, int nodeid)
{
	struct can_frame cf = { 0 };
	cf.can_id = 0;
	cf.can_dlc = 2;
	nmt_set_cs(&cf, cs);
	nmt_set_nodeid(&cf, nodeid);
	return sock_send(sock, &cf, 0);
}

int co_net__request_heartbeat(const struct sock* sock, int nodeid)
{
	struct can_frame cf = { 0 };
	cf.can_id = R_HEARTBEAT + nodeid;
	cf.can_id |= CAN_RTR_FLAG;
	return sock_send(sock, &cf, 0);
}

int co_net__request_sdo(const struct sock* sock, int nodeid)
{
	struct can_frame cf;
	sdo_clear_frame(&cf);
	cf.can_id = R_RSDO + nodeid;
	cf.can_dlc = 4;
	sdo_set_index(&cf, 0x1000);
	sdo_set_subindex(&cf, 0);
	sdo_set_cs(&cf, SDO_CCS_UL_INIT_REQ);
	return sock_send(sock, &cf, 0);
}

int co_net__wait_for_bootup(const struct sock* sock, char* nodes_seen,
			    int start, int end, int timeout)
{
	struct can_frame cf;
	struct canopen_msg msg;

	int t = gettime_ms(CLOCK_MONOTONIC);
	int t_end = t + timeout;

	while (sock_timed_recv(sock, &cf, MAX(0, t_end - t)) > 0) {
		t = gettime_ms(CLOCK_MONOTONIC);

		canopen_get_object_type(&msg, &cf);

		if (!(start <= msg.id && msg.id <= end))
			continue;

		if (msg.object != CANOPEN_HEARTBEAT)
			continue;

		nodes_seen[msg.id] = 1;
		t_end = t + timeout;
	}

	return 0;
}

int co_net__wait_for_sdo(const struct sock* sock, char* nodes_seen,
			 int start, int end, int timeout)
{
	struct can_frame cf;
	struct canopen_msg msg;

	int t = gettime_ms(CLOCK_MONOTONIC);
	int t_end = t + timeout;

	while (sock_timed_recv(sock, &cf, MAX(0, t_end - t)) > 0) {
		t = gettime_ms(CLOCK_MONOTONIC);

		canopen_get_object_type(&msg, &cf);

		if (!(start <= msg.id && msg.id <= end))
			continue;

		if (msg.object != CANOPEN_TSDO)
			continue;

		nodes_seen[msg.id] = 1;
		t_end = t + timeout;
	}

	return 0;
}

int co_net_reset(const struct sock* sock, char* nodes_seen, int timeout)
{
	co_net_send_nmt(sock, NMT_CS_RESET_COMMUNICATION, 0);
	return co_net__wait_for_bootup(sock, nodes_seen, 0, 127, timeout);
}

int co_net_reset_range(const struct sock* sock, char* nodes_seen, int start,
		       int end, int timeout)
{
	for (int i = start; i <= end; ++i)
		co_net_send_nmt(sock, NMT_CS_RESET_COMMUNICATION, i);

	return co_net__wait_for_bootup(sock, nodes_seen, start, end, timeout);
}

int co_net_probe(const struct sock* sock, char* nodes_seen, int start, int end,
		 int timeout)
{
	for (int i = start; i <= end; ++i)
		if (!nodes_seen[i])
			co_net__request_heartbeat(sock, i);

	return co_net__wait_for_bootup(sock, nodes_seen, start, end, timeout);
}

int co_net_probe_sdo(const struct sock* sock, char* nodes_seen, int start,
		     int end, int timeout)
{
	for (int i = start; i <= end; ++i)
		if (!nodes_seen[i])
			co_net__request_sdo(sock, i);

	return co_net__wait_for_sdo(sock, nodes_seen, start, end, timeout);
}

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

#ifndef _CANOPEN_DRIVER_H
#define _CANOPEN_DRIVER_H

#include <unistd.h>
#include <stdint.h>

#define co_sdo_send(self, index, subindex, value) \
({ \
 	typeof(value) network_order, host_order = (value); \
	co_byteorder((void*)&network_order, &host_order, \
		     sizeof(network_order), sizeof(host_order)); \
	co_sdo_send_blob(self, index, subindex, &network_order, \
			 sizeof(network_order)); \
})

struct co_drv;
struct co_sdo_req;

enum co_sdo_type {
	CO_SDO_DOWNLOAD = 1,
	CO_SDO_UPLOAD
};

enum co_sdo_status {
	CO_SDO_REQ_PENDING = 0,
	CO_SDO_REQ_OK,
	CO_SDO_REQ_LOCAL_ABORT,
	CO_SDO_REQ_REMOTE_ABORT,
	CO_SDO_REQ_CANCELLED,
	CO_SDO_REQ_NOMEM,
};

enum co_options {
	CO_OPT_UNSPEC = 0,
	CO_OPT_INHIBIT_START = 1,
};

enum co_pdo_type {
	CO_TPDO1 = 1,
	CO_RPDO1,
	CO_TPDO2,
	CO_RPDO2,
	CO_TPDO3,
	CO_RPDO3,
	CO_TPDO4,
	CO_RPDO4,
};

enum co_pdo_xmission_type {
	CO_PDO_XMISSION_SYNCHRONOUS = 0,
	CO_TPDO_XMISSION_RTR_SYNCHRONOUS = 0xfc,
	CO_TPDO_XMISSION_RTR_EVENT_DRIVEN = 0xfd,
	CO_PDO_XMISSION_MANUFACTURER_EVENT_DRIVEN = 0xfe,
	CO_PDO_XMISSION_STANDARD_EVENT_DRIVEN = 0xff,
};

struct co_pdo_map_entry {
	uint16_t index;
	uint8_t subindex;
	uint8_t length;
};

struct co_pdo_map {
	enum co_pdo_type type;
	enum co_pdo_xmission_type xmission_type;
	uint16_t inhibit_time;
	uint16_t event_time;
	uint8_t sync_start_value;

	struct co_pdo_map_entry* entries;
};

struct co_emcy {
	uint16_t code;
	uint8_t reg;
	uint64_t manufacturer_error;
};

typedef void (*co_free_fn)(void*);
typedef void (*co_pdo_fn)(struct co_drv*, const void* data, size_t size);
typedef void (*co_sdo_done_fn)(struct co_drv*, struct co_sdo_req* req);
typedef void (*co_emcy_fn)(struct co_drv*, struct co_emcy*);
typedef void (*co_start_fn)(struct co_drv*);

const char* co_get_network_name(const struct co_drv* self);
int co_get_nodeid(const struct co_drv* self);
uint32_t co_get_device_type(const struct co_drv* self);
uint32_t co_get_vendor_id(const struct co_drv* self);
uint32_t co_get_product_code(const struct co_drv* self);
uint32_t co_get_revision_number(const struct co_drv* self);
const char* co_get_name(const struct co_drv* self);

void co_set_context(struct co_drv* self, void* context, co_free_fn fn);
void* co_get_context(const struct co_drv* self);

void co_set_pdo1_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo2_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo3_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_pdo4_fn(struct co_drv* self, co_pdo_fn fn);
void co_set_emcy_fn(struct co_drv* self, co_emcy_fn fn);
void co_set_start_fn(struct co_drv* self, co_start_fn fn);

int co_rpdo1(struct co_drv* self, const void* data, size_t size);
int co_rpdo2(struct co_drv* self, const void* data, size_t size);
int co_rpdo3(struct co_drv* self, const void* data, size_t size);
int co_rpdo4(struct co_drv* self, const void* data, size_t size);

void co_setopt(struct co_drv* self, enum co_options opt);
void co_start(struct co_drv* self);

struct co_sdo_req* co_sdo_req_new(struct co_drv* drv);
void co_sdo_req_ref(struct co_sdo_req* self);
int co_sdo_req_unref(struct co_sdo_req* self);
void co_sdo_req_set_indices(struct co_sdo_req* self, int index, int subindex);
void co_sdo_req_set_type(struct co_sdo_req* self, enum co_sdo_type type);
void co_sdo_req_set_data(struct co_sdo_req* self, const void* data, size_t sz);
void co_sdo_req_set_done_fn(struct co_sdo_req* self, co_sdo_done_fn fn);
void co_sdo_req_set_context(struct co_sdo_req* self, void* context,
			    co_free_fn free_fn);
void* co_sdo_req_get_context(const struct co_sdo_req* self);
int co_sdo_req_start(struct co_sdo_req* self);
const void* co_sdo_req_get_data(const struct co_sdo_req* self);
size_t co_sdo_req_get_size(const struct co_sdo_req* self);
int co_sdo_req_get_index(const struct co_sdo_req* self);
int co_sdo_req_get_subindex(const struct co_sdo_req* self);
enum co_sdo_status co_sdo_req_get_status(const struct co_sdo_req* self);

int co_sdo_send_blob(struct co_drv* self, int index, int subindex,
		     const void* payload, size_t size);

int co_map_pdo(struct co_drv* self, const struct co_pdo_map* map);

void co_byteorder(void* dst, const void* src, size_t dst_size, size_t src_size);

#endif /* _CANOPEN_DRIVER_H */

#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include "socketcan.h"
#include "canopen/master.h"
#include "canopen/sdo_req.h"
#include "canopen/emcy.h"
#include "canopen-driver.h"
#include "string-utils.h"

size_t strlcpy(char*, const char*, size_t);

struct co_sdo_req {
	struct sdo_req req;
	struct co_drv* drv;
	co_sdo_done_fn on_done;
};

const char* co__drv_find_dso(const char* name)
{
	static __thread char result[256];

	char name_buffer[256];
	strlcpy(name_buffer, name, 256);

	snprintf(result, sizeof(result), "/usr/lib/canopen/co_drv_%s.so",
		 string_tolower(name_buffer));
	result[sizeof(result) - 1] = '\0';

	if (access(result, R_OK) < 0)
		return NULL;

	return result;
}

int co_drv_load(struct co_drv* drv, const char* name)
{
	assert(!drv->dso);

	const char* path = co__drv_find_dso(name);
	if (!path)
		return -1;

	drv->dso = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	dlerror();
	if (!drv->dso)
		return -1;

	drv->init_fn = dlsym(drv->dso, "co_drv_init");
	dlerror();
	if (drv->init_fn)
		return 0;

	dlclose(drv->dso);
	memset(drv, 0, sizeof(*drv));
	return -1;
}

int co_drv_init(struct co_drv* drv)
{
	return drv->init_fn(drv);
}

void co_drv_unload(struct co_drv* drv)
{
	if (drv->context && drv->free_fn)
		drv->free_fn(drv->context);

	dlclose(drv->dso);

	memset(drv, 0, sizeof(*drv));
}

#pragma GCC visibility push(default)

int co_get_nodeid(const struct co_drv* self)
{
	return co_master_get_node_id(co_drv_node(self));
}

uint32_t co_get_device_type(const struct co_drv* self)
{
	return co_drv_node(self)->device_type;
}

uint32_t co_get_vendor_id(const struct co_drv* self)
{
	return co_drv_node(self)->vendor_id;
}

uint32_t co_get_product_code(const struct co_drv* self)
{
	return co_drv_node(self)->product_code;
}

uint32_t co_get_revision_number(const struct co_drv* self)
{
	return co_drv_node(self)->revision_number;
}

const char* co_get_name(const struct co_drv* self)
{
	return co_drv_node(self)->name;
}

void co_set_context(struct co_drv* self, void* context, co_free_fn fn)
{
	self->context = context;
	self->free_fn = fn;
}

void* co_get_context(const struct co_drv* self)
{
	return self->context;
}

void co_set_pdo1_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo1_fn = fn;
}

void co_set_pdo2_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo2_fn = fn;
}

void co_set_pdo3_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo3_fn = fn;
}

void co_set_pdo4_fn(struct co_drv* self, co_pdo_fn fn)
{
	self->pdo4_fn = fn;
}

int co_rpdo1(struct co_drv* self, const void* data, size_t size)
{
	return co__rpdox(co_get_nodeid(self), R_RPDO1, data, size);
}

int co_rpdo2(struct co_drv* self, const void* data, size_t size)
{
	return co__rpdox(co_get_nodeid(self), R_RPDO2, data, size);
}

int co_rpdo3(struct co_drv* self, const void* data, size_t size)
{
	return co__rpdox(co_get_nodeid(self), R_RPDO3, data, size);
}

int co_rpdo4(struct co_drv* self, const void* data, size_t size)
{
	return co__rpdox(co_get_nodeid(self), R_RPDO4, data, size);
}

void co_set_emcy_fn(struct co_drv* self, co_emcy_fn fn)
{
	self->emcy_fn = fn;
}

const char* co_get_network_name(const struct co_drv* self)
{
	return self->iface;
}

void co_sdo_req_ref(struct co_sdo_req* self)
{
	sdo_req_ref(&self->req);
}

int co_sdo_req_unref(struct co_sdo_req* self)
{
	return sdo_req_unref(&self->req);
}

static void co__sdo_req_on_done(struct sdo_req* req)
{
	struct co_sdo_req* self = (void*)req;

	co_sdo_done_fn on_done = self->on_done;
	if (on_done)
		on_done(self->drv, self);

	co_sdo_req_unref(self);
}

struct co_sdo_req* co_sdo_req_new(struct co_drv* drv)
{
	struct co_sdo_req* self = malloc(sizeof(*self));
	if (!self)
		return NULL;

	memset(self, 0, sizeof(*self));

	struct sdo_req* req = &self->req;

	req->ref = 1;
	req->on_done = co__sdo_req_on_done;
	self->drv = drv;

	return self;
}

void co_sdo_req_set_indices(struct co_sdo_req* self, int index, int subindex)
{
	self->req.index = index;
	self->req.subindex = subindex;
}

void co_sdo_req_set_type(struct co_sdo_req* self, enum co_sdo_type type)
{
	switch (type) {
	case CO_SDO_DOWNLOAD:
		self->req.type = SDO_REQ_DOWNLOAD;
		break;
	case CO_SDO_UPLOAD:
		self->req.type = SDO_REQ_UPLOAD;
		break;
	default:
		abort();
	}
}

void co_sdo_req_set_data(struct co_sdo_req* self, const void* data, size_t size)
{
	vector_assign(&self->req.data, data, size);
}

void co_sdo_req_set_done_fn(struct co_sdo_req* self, co_sdo_done_fn fn)
{
	self->on_done = fn;
}

void co_sdo_req_set_context(struct co_sdo_req* self, void* context,
			    co_free_fn free_fn)
{
	self->req.context = context;
	self->req.context_free_fn = free_fn;
}

void* co_sdo_req_get_context(const struct co_sdo_req* self)
{
	return self->req.context;
}

int co_sdo_req_start(struct co_sdo_req* self)
{
	int nodeid = co_get_nodeid(self->drv);
	return sdo_req_start(&self->req, sdo_req_queue_get(nodeid));
}

const void* co_sdo_req_get_data(const struct co_sdo_req* self)
{
	return self->req.data.data;
}

size_t co_sdo_req_get_size(const struct co_sdo_req* self)
{
	return self->req.data.index;
}

int co_sdo_req_get_index(const struct co_sdo_req* self)
{
	return self->req.index;
}

int co_sdo_req_get_subindex(const struct co_sdo_req* self)
{
	return self->req.subindex;
}

enum co_sdo_status co_sdo_req_get_status(const struct co_sdo_req* self)
{
	switch (self->req.status) {
	case SDO_REQ_PENDING: return CO_SDO_REQ_PENDING;
	case SDO_REQ_OK: return CO_SDO_REQ_OK;
	case SDO_REQ_LOCAL_ABORT: return CO_SDO_REQ_LOCAL_ABORT;
	case SDO_REQ_REMOTE_ABORT: return CO_SDO_REQ_REMOTE_ABORT;
	case SDO_REQ_CANCELLED: return CO_SDO_REQ_CANCELLED;
	case SDO_REQ_NOMEM: return CO_SDO_REQ_NOMEM;
	}

	abort();
	return -1;
}

void co_byteorder(void* dst, const void* src, size_t dst_size, size_t src_size)
{
	return byteorder2(dst, src, dst_size, src_size);
}

#pragma GCC visibility pop

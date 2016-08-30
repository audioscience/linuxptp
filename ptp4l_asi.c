
#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>

#include "libmain.h"
#include "clock.h"

#include <libubus.h>

struct pub_obj {
	struct ubus_object obj;
	struct ubus_object_type type;
	struct ubus_method methods[2];
	void *priv;
};

static struct blob_buf b;
static struct port_capable_info as_capable_info[MAX_PORTS];

static struct app_state {
	struct ubus_context *ubus_ctx;
	struct clock *c;
	int num_obj;
	struct pub_obj *objs;
} s;

void ubus_connection_lost(struct ubus_context *ctx)
{
	ctx->sock.fd = -1;
	clock_register_port_update_cb(s.c, NULL, NULL);
	fprintf(stderr, "Disconnected from ubusd\n");
}

#define BOOL_AS_STRING(val) (val?"true":"false")
#define INFO_FIELD_UNPACK(ptr, fn) ptr->fn.actual,ptr->fn.expected, BOOL_AS_STRING(ptr->fn.unmet)
#define INFO_FIELD_UNPACK_BOOL(ptr, fn) BOOL_AS_STRING(ptr->fn.actual),BOOL_AS_STRING(ptr->fn.expected), BOOL_AS_STRING(ptr->fn.unmet)

struct blob_attr *as_info_payload(struct port_capable_info *info)
{
	struct timespec ts;
	char *msg = NULL;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint64_t ns = ((uint64_t)ts.tv_sec)*NS_PER_SEC+ts.tv_nsec;
	blob_buf_init(&b, 0);
	asprintf(&msg, "{\"tm\":%llu,\"port-num\":%d,\"as-capable\":%s,"
		"\"max-peer-delay\":[%lld,%lld,%s],"
		"\"min-peer-delay\":[%lld,%lld,%s],"
		"\"max-missed-pdr\":[%hhu,%hhu,%s],"
		"\"max-multiple-seq-pdr\":[%hhu,%hhu,%s],"
		"\"peer-port-id-valid\":[%s,%s,%s],"
		"\"nrate-ratio-valid\":[%s,%s,%s],"
		"\"port-state-acceptable\":[%s,%s,%s]}", ns, info->port_num, BOOL_AS_STRING(info->as_capable),
		INFO_FIELD_UNPACK(info, max_peer_delay),
		INFO_FIELD_UNPACK(info, min_peer_delay),
		INFO_FIELD_UNPACK(info, max_missed_pdr),
		INFO_FIELD_UNPACK(info, max_multiple_seq_pdr),
		INFO_FIELD_UNPACK_BOOL(info, peer_port_id_valid),
		INFO_FIELD_UNPACK_BOOL(info, nrate_ratio_valid),
		INFO_FIELD_UNPACK_BOOL(info, port_state_acceptable));
	blob_put(&b, BLOB_ATTR_STRING, msg, strlen(msg)-1);
	free(msg);
	return blob_data(b.head);
}

int ubus_method_handler(struct ubus_context *ctx, struct ubus_object *obj,
			      struct ubus_request_data *req,
			      const char *method, struct blob_attr *msg)
{
	struct pub_obj *p = container_of(obj, struct pub_obj, obj);
	int port_num = (int)p->priv;
	struct port_capable_info *info = &as_capable_info[port_num-1];
	if (port_num != info->port_num)
		return UBUS_STATUS_NO_DATA;
	struct blob_attr *payload = as_info_payload(info);
	ubus_send_reply(ctx, req, payload);
	return 0;
}

void port_update_cb(struct port_capable_info *info, void *priv)
{
	if (!info || !info->port_num)
		return;
	assert(s.ubus_ctx);
	assert(s.ubus_ctx->sock.fd >= 0);
	as_capable_info[info->port_num-1] = *info;
	struct blob_attr *payload = as_info_payload(info);
	ubus_notify(s.ubus_ctx, &s.objs[info->port_num-1].obj, "update", payload, -1);
}

void setup_ubus_objects(void)
{
	int idx;

	s.num_obj = clock_num_ports(s.c);
	s.objs = calloc(sizeof(struct pub_obj), s.num_obj);
	for (idx = 0; idx < s.num_obj; idx++) {
		struct ubus_object *o = &s.objs[idx].obj;
		struct ubus_method *m = s.objs[idx].methods;
		struct ubus_object_type *t = &s.objs[idx].type;
		char *name = NULL;
		asprintf(&name, "0:vol-st/avb/gptp/port/%d/as-capable", idx+1);
		s.objs[idx].priv = (void*)idx+1;
		o->name = name;
		o->type = t;
		m[0].name = NULL;
		m[0].handler = ubus_method_handler;
		m[1].name = "*";
		m[1].handler = NULL;
		o->n_methods = 1;
		o->methods = &m[0];
		t->n_methods = 1;
		t->methods = &m[1];
		int ret = ubus_add_object(s.ubus_ctx, o);
		if (ret) {
			fprintf(stderr, "Cannot add object for port %d, error %d\n", idx, ret);
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	char *config_file = NULL;

	if (argc > 1)
		config_file = argv[1];

	s.c = ptp4l_setup(config_file);
	if (!s.c) {
		fprintf(stderr, "Configuration error\n");
		exit(1);
	}

	while (true) {
		int ret = clock_poll(s.c);
		if (ret)
			break;

		if (!s.ubus_ctx) {
			s.ubus_ctx = ubus_connect(NULL);
			if (!s.ubus_ctx)
				continue;
			s.ubus_ctx->connection_lost = ubus_connection_lost;
			setup_ubus_objects();
			clock_register_port_update_cb(s.c, port_update_cb, NULL);
			fprintf(stderr, "Connected to ubusd\n");
		}

		if (s.ubus_ctx->sock.fd < 0) {
			if (!ubus_reconnect(s.ubus_ctx, NULL)) {
				clock_register_port_update_cb(s.c, port_update_cb, NULL);
				fprintf(stderr, "Connected to ubusd\n");
			}
		} else {
			ubus_handle_event(s.ubus_ctx);
			/* process pending messages */
			s.ubus_ctx->pending_timer.cb(&s.ubus_ctx->pending_timer);
		}
	}
	ptp4l_destroy(s.c);
	ubus_free(s.ubus_ctx);
	return 0;
}

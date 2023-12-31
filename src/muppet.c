#include <fts.h>
#include <kore/kore.h>
#include <kore/http.h>
#include <mustach/mustach.h>
#include <mustach/kore_mustach.h>

#if defined(__linux__)
#include <kore/seccomp.h>

KORE_SECCOMP_FILTER("muppet",
		KORE_SYSCALL_ALLOW(connect),
		KORE_SYSCALL_ALLOW(newfstatat),
		KORE_SYSCALL_ALLOW(getdents64),
		KORE_SYSCALL_ALLOW_ARG(socket, 0, AF_UNIX),
);
#endif

#include "assets.h"

#define MPV_SOCKET "/tmp/mpv_socket"
#define LIBRARY_PATH "/mnt/Videos"

int page(struct http_request *);

void websocket_connect(struct connection *);
void websocket_disconnect(struct connection *);
void websocket_message(struct connection *, uint8_t, void *, size_t);
int page_ws_connect(struct http_request *);
void handle_sock(void *, int);
int ptree(char * const argv[], struct kore_json_item *);
void selection_sort(struct kore_json_item **, int);
void render_index(void *, u_int64_t);
void kore_worker_configure(void);

struct sock_wrap {
	struct kore_event evt;
	struct connection *c;
	int sockfd;
};

void
kore_worker_configure(void)
{
	render_index(NULL, 0);
	kore_timer_add(render_index, 60000, NULL, 0);
}

void
render_index(void *arg, u_int64_t interval)
{
	FILE *f;
	struct kore_json_item *json;
	struct kore_buf *result;
	char *files[] = {LIBRARY_PATH, NULL};

	json = kore_json_create_object(NULL, NULL);
	ptree(files, json);
	kore_mustach_json(asset_index_html, json, Mustach_With_AllExtensions, &result);
	kore_json_item_free(json);

	if ((f = fopen("landing/index.html", "w")) != NULL) {
		fprintf(f, kore_buf_stringify(result, NULL));
		fclose(f);
	}

	kore_buf_free(result);
}

int
page(struct http_request *req)
{
	http_response_header(req, "location", "/landing/index.html");
	http_response(req, 301, NULL, 0);
	return (KORE_RESULT_OK);
}

void
handle_sock(void *c, int err)
{
	struct sock_wrap *w = c;
	char buf[BUFSIZ];
	ssize_t ret;

	if (err) {
		kore_websocket_send(w->c, WEBSOCKET_OP_CLOSE, NULL, 0);
		return;
	}

	if (w->evt.flags & KORE_EVENT_READ) {
		if ((ret = read(w->sockfd, buf, sizeof(buf))) > 0) {
			kore_websocket_send(w->c, WEBSOCKET_OP_TEXT, buf, ret);
		} else {
			kore_websocket_send(w->c, WEBSOCKET_OP_CLOSE, NULL, 0);
		}
	}
}

void
websocket_connect(struct connection *c)
{
	struct sockaddr_un addr;
	struct sock_wrap *wrap;

	wrap = kore_malloc(sizeof(*wrap));
	c->hdlr_extra = wrap;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, MPV_SOCKET, sizeof(addr.sun_path) - 1);

	if ((wrap->sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		kore_log(LOG_NOTICE, "failed to open socket");
		kore_websocket_send(c, WEBSOCKET_OP_CLOSE, NULL, 0);
		return;
	}

	if (connect(wrap->sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
		kore_log(LOG_NOTICE, "failed to connect sock");
		kore_websocket_send(c, WEBSOCKET_OP_CLOSE, NULL, 0);
		return;
	}

	wrap->c = c;
	wrap->evt.handle = handle_sock;
	kore_platform_schedule_read(wrap->sockfd, wrap);

	kore_log(LOG_NOTICE, "%p: connected", c);
}

void
websocket_message(struct connection *c, uint8_t op, void *data, size_t len)
{
	struct sock_wrap *wrap = c->hdlr_extra;

	if (write(wrap->sockfd, data, len) == -1) {
		kore_log(LOG_NOTICE, "failed write to sock");
		kore_websocket_send(c, WEBSOCKET_OP_CLOSE, NULL, 0);
	}
}

void
websocket_disconnect(struct connection *c)
{
	struct sock_wrap *wrap = c->hdlr_extra;

	if (wrap->sockfd != -1)
		close(wrap->sockfd);

	kore_log(LOG_NOTICE, "%p: disconnected", c);
}

int
page_ws_connect(struct http_request *req)
{
	kore_websocket_handshake(req, "websocket_connect",
			"websocket_message", "websocket_disconnect");
	return (KORE_RESULT_OK);
}

int
ptree(char * const argv[], struct kore_json_item *json)
{
	struct kore_json_item *f, *farr, **tarr;
	FTS *ftsp;
	FTSENT *p;
	int fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;
	int n = 10, i = 0;
	char *ext, imgpath[BUFSIZ];

	farr = kore_json_create_array(json, "media-files");

	if ((ftsp = fts_open(argv, fts_options, NULL)) == NULL) {
		kore_log(LOG_NOTICE, "fts_open");
		return (KORE_RESULT_ERROR);
	}

	tarr = kore_malloc(n * sizeof(struct kore_json_item *));

	while ((p = fts_read(ftsp)) != NULL) {

		if (p->fts_info != FTS_F)
			continue;

		if ((ext = strrchr(p->fts_name, '.')) == NULL)
			continue;

		ext++;
		if (strcmp(ext, "mkv") && strcmp(ext, "mp4"))
			continue;

		f = kore_json_create_object(NULL, p->fts_name);
		kore_json_create_string(f, "title", p->fts_name);
		kore_json_create_string(f, "path", p->fts_path);
		snprintf(imgpath, sizeof(imgpath), "/static/img/%s.gif", p->fts_name);
		kore_json_create_string(f, "image", imgpath);

		if (i >= n) {
			n += i;
			tarr = kore_realloc(tarr, n * sizeof(struct kore_json_item *));
		}

		tarr[i++] = f;
	}

	selection_sort(tarr, i);
	for (int j = 0; j < i; j++)
		kore_json_item_attach(farr, tarr[j]);

	kore_free(tarr);
	fts_close(ftsp);
	return (KORE_RESULT_OK);
}

void
selection_sort(struct kore_json_item **arr, int n)
{
	struct kore_json_item *temp;
	int i, j, m; 

	for (i = 0; i < n - 1; i++) {
		m = i;
		for (j = i + 1; j < n; j++)
			if (strcmp(arr[j]->name, arr[m]->name) < 0)
				m = j;

		temp = arr[m];
		arr[m] = arr[i];
		arr[i] = temp;
	}
}

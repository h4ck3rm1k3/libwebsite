#ifndef WS_WEBSITE_H
#define WS_WEBSITE_H

#include <ev.h>
#include <obstack.h>
#include <netinet/in.h>

typedef int bool;
#define TRUE 1
#define FALSE 0

#define ws_CONNECTION_STRUCT(targ, typ) (targ)->_conn_size = sizeof(typ)
#define ws_REQUEST_STRUCT(targ, typ) (targ)->_req_size = sizeof(typ)
#define ws_MESSAGE_STRUCT(targ, typ) (targ)->_message_size = sizeof(typ)
#define ws_HEADERS_CB(targ, fun) \
    (targ)->req_callbacks[WS_REQ_CB_HEADERS] = (ws_request_cb)fun
#define ws_REQUEST_CB(targ, fun) \
    (targ)->req_callbacks[WS_REQ_CB_REQUEST] = (ws_request_cb)fun
#define ws_WEBSOCKET_CB(targ, fun) \
    (targ)->req_callbacks[WS_REQ_CB_WEBSOCKET] = (ws_request_cb)fun
#define ws_FINISH_CB(targ, fun) \
    (targ)->req_callbacks[WS_REQ_CB_FINISH] = (ws_request_cb)fun
#define ws_CONNECT_CB(targ, fun) \
    (targ)->conn_callbacks[WS_CONN_CB_CONNECT] = (ws_connection_cb)fun
#define ws_DISCONNECT_CB(targ, fun) \
    (targ)->conn_callbacks[WS_CONN_CB_DISCONNECT] = (ws_connection_cb)fun
#define ws_MESSAGE_CB(targ, fun) \
    (targ)->wsock_callbacks[WS_WEBSOCKET_CB_MESSAGE] = (ws_websocket_cb)fun
#define ws_SET_TIMEOUT(targ, value) (targ)->network_timeout = (value)
#define ws_MESSAGE_DATA(msg, ptr, len, free_fun) \
    (msg)->data = ptr; \
    (msg)->length = len; \
    (msg)->free_cb = free_fun;

#ifndef WS_NO_ATOMIC_REFCNT
#define ws_MESSAGE_INCREF(val) (__sync_add_and_fetch(&(val)->refcnt, 1))
#define ws_MESSAGE_DECREF(val) if(!__sync_sub_and_fetch(&(val)->refcnt, 1))\
    ws_message_free(val);
#else
#define ws_MESSAGE_INCREF(val) (++(val)->refcnt)
#define ws_MESSAGE_DECREF(val) if(!--(val)->refcnt) ws_message_free(val)
#endif

typedef enum {
    WS_REQ_CB_HEADERS, // got headers
    WS_REQ_CB_REQUEST, // got request body
    WS_REQ_CB_WEBSOCKET, // Request is actually a websocket
    WS_REQ_CB_FINISH, // response fully sent
    WS_REQ_CB_COUNT,
} ws_request_cb_enum;

typedef enum {
    WS_CONN_CB_CONNECT,
    WS_CONN_CB_DISCONNECT,
    WS_CONN_CB_COUNT,
} ws_connection_cb_enum;

typedef enum {
    WS_WEBSOCKET_CB_MESSAGE,
    WS_WEBSOCKET_CB_COUNT,
} ws_websocket_cb_enum;

typedef enum {
    WS_H_HOST,
    WS_H_CONTENT_LENGTH,
    WS_H_CONNECTION,
    WS_H_UPGRADE,
    WS_H_ORIGIN,
    WS_H_WEBSOCKET_PROTO,
    WS_H_WEBSOCKET_KEY1,
    WS_H_WEBSOCKET_KEY2,
    WS_STD_HEADERS,
} ws_header_enum;

typedef enum {
    WS_HTTP_10,
    WS_HTTP_11,
} ws_version_enum;

typedef enum {
    WS_REPLY_NOT_READY,
    WS_REPLY_FINISHED,
    WS_REPLY_CHUNKED,
    WS_REPLY_SENDFILE,
} ws_reply_enum;

struct ws_request_s;
struct ws_connection_s;

typedef struct ws_message_s {
    size_t refcnt;
    size_t length;
    char *data;
    void (*free_cb)(void *);
} ws_message_t;

typedef int (*ws_request_cb)(struct ws_request_s *req);
typedef int (*ws_connection_cb)(struct ws_connection_s *conn);
typedef int (*ws_websocket_cb)(struct ws_connection_s *conn,
    struct ws_message_s *msg);

typedef struct ws_header_pair_s {
    char *name;
    char *value;
} ws_header_pair_t;

typedef struct ws_hparser_s {
    void *index;
    int count;
} ws_hparser_t;

typedef struct ws_request_s {
    struct obstack pieces;
    struct ws_connection_s *conn;
    struct ws_request_s *next;
    struct ws_request_s *prev;

    ws_request_cb req_callbacks[WS_REQ_CB_COUNT];
    ws_websocket_cb wsock_callbacks[WS_WEBSOCKET_CB_COUNT];
    ev_tstamp network_timeout;
    char *headers_buf;
    int bufposition;
    int headerlen;
    char *body;
    int bodylen;
    int bodyposition;
    char *uri;
    char *method;
    ws_version_enum http_version;
    ws_header_pair_t *allheaders;

    char **headerindex;
    int reply_pos;
    int reply_state;
    char *reply_head;
    int reply_head_size;
    int _contlen_offset;
    char *reply_body;
    int reply_body_size;

    bool websocket;
} ws_request_t;

typedef struct ws_connection_s {
    struct ev_loop *loop;
    struct ev_io watch;
    struct ev_io reply_watch;
    struct sockaddr_in addr;
    ev_tstamp network_timeout;
    int _req_size;
    int max_header_size;
    int _message_size;
    int max_message_size;
    int max_message_queue;
    struct ws_server_s *serv;
    struct ws_connection_s *next;
    struct ws_connection_s *prev;
    ws_request_cb req_callbacks[WS_REQ_CB_COUNT];
    ws_connection_cb conn_callbacks[WS_CONN_CB_COUNT];
    ws_websocket_cb wsock_callbacks[WS_WEBSOCKET_CB_COUNT];
    struct ws_request_s *first_req;
    struct ws_request_s *last_req;
    size_t request_num;
    int close_on_finish;
    char *websocket_buf;
    size_t websocket_buf_size;
    size_t websocket_buf_offset;
    ws_message_t **websocket_queue;
    size_t websocket_queue_size;
    size_t websocket_qstart;
    size_t websocket_qlen;
    size_t websocket_queue_offset; // offset INSIDE the current message
} ws_connection_t;

typedef struct ws_server_s {
    struct ev_loop *loop;
    ev_tstamp network_timeout;
    int max_header_size;
    int _conn_size;
    int _req_size;
    int _message_size;
    int max_message_size;
    int max_message_queue;
    struct ws_listener_s *listeners;
    size_t listeners_num;
    struct ws_connection_s *first_conn;
    struct ws_connection_s *last_conn;
    size_t connection_num;
    ws_request_cb req_callbacks[WS_REQ_CB_COUNT];
    ws_connection_cb conn_callbacks[WS_CONN_CB_COUNT];
    ws_websocket_cb wsock_callbacks[WS_WEBSOCKET_CB_COUNT];
    ws_hparser_t header_parser;
} ws_server_t;

int ws_statusline(ws_request_t *req, const char *line);
int ws_add_header(ws_request_t *req, const char *name, const char *value);
int ws_finish_headers(ws_request_t *req);
int ws_reply_data(ws_request_t *req, const char *data, size_t data_size);
int ws_request_free(ws_request_t *req);

int ws_server_init(ws_server_t *serv, struct ev_loop *loop);
int ws_server_destroy(ws_server_t *serv);
int ws_add_tcp(ws_server_t *serv, in_addr_t addr, int port);
int ws_add_unix(ws_server_t *serv, const char *filename, size_t len);
int ws_add_fd(ws_server_t *serv, int fd);
int ws_index_header(ws_server_t *serv, const char *name);
int ws_server_start(ws_server_t *serv);

ws_message_t *ws_message_copy_data(ws_connection_t *conn,
    void *data, size_t len);
ws_message_t *ws_message_new(ws_connection_t *conn);
int ws_message_init(ws_message_t *msg);
int ws_message_send(ws_connection_t *conn, ws_message_t *msg);
void ws_message_free(ws_message_t *msg);

void ws_quickstart(ws_server_t *serv, const char *hostname,
    int port, ws_request_cb cb);

void *ws_match_new();
void ws_match_free(void *hint);
size_t ws_match_add(void *box, const char *string, size_t result);
size_t ws_match_iadd(void *box, const char *string, size_t result);
int ws_match_compile(void *box);
bool ws_match(void *box, const char *string, size_t *result);
bool ws_imatch(void *box, const char *string, size_t *result);

void *ws_fuzzy_new();
void ws_fuzzy_free(void *hint);
size_t ws_fuzzy_add(void *box, const char *string, bool prefix, size_t result);
size_t ws_fuzzy_iadd(void *box, const char *string, bool prefix, size_t result);
int ws_fuzzy_compile(void *box);
int ws_rfuzzy_compile(void *box);
bool ws_fuzzy(void *box, const char *string, size_t *result);
bool ws_rfuzzy(void *box, const char *string, size_t *result);
bool ws_ifuzzy(void *box, const char *string, size_t *result);
bool ws_irfuzzy(void *box, const char *string, size_t *result);

#endif // WS_WEBSITE_H

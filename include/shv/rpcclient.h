#ifndef SHV_RPCCLIENT_H
#define SHV_RPCCLIENT_H
#include <stdbool.h>
#include <shv/rpcclient_impl.h>
#include <shv/rpcurl.h>

typedef struct rpcclient *rpcclient_t;


rpcclient_t rpcclient_connect(const struct rpcurl *url);

rpcclient_t rpcclient_stream_new(int readfd, int writefd);
rpcclient_t rpcclient_stream_tcp_connect(const char *location, int port);
rpcclient_t rpcclient_stream_unix_connect(const char *location);

rpcclient_t rpcclient_datagram_new(void);
rpcclient_t rpcclient_datagram_udp_connect(const char *location, int port);

rpcclient_t rpcclient_serial_new(int fd);
rpcclient_t rpcclient_serial_connect(const char *path);

#define rpcclient_disconnect(CLIENT) ((CLIENT)->disconnect(CLIENT))

bool rpcclient_login(rpcclient_t, const char *username, const char *password,
	enum rpc_login_type type) __attribute__((nonnull(1)));



#define rpcclient_nextmsg(CLIENT) (&(CLIENT)->nextmsg(CLIENT))

#define rpcclient_unpack(CLIENT) (&(CLIENT)->unpack)

#define rpcclient_pack(CLIENT) (&(CLIENT)->pack)

#define rpcclient_send(CLIENT) ((CLIENT)->send(CLIENT_PTR))

#define rpcclient_logger(CLIENT) ((CLIENT)->logger)


#endif

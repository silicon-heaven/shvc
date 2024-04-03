#ifndef SHV_RPCPROTOCOL_STREAM_H
#define SHV_RPCPROTOCOL_STREAM_H


bool cp_pack_stream(void *ptr, const struct cpitem *item);
void cp_unpack_stream(void *ptr, struct cpitem *item);
int ctl_stream(rpcclient_t client, enum rpcclient_ctlop op);
struct rpcprotocol_interface *rpcprotocol_stream_new(int socket_fd, struct rpcprotocol_info *trans_info);
int8_t nextmsg_protocol_stream(struct rpcprotocol_interface *protocol);
bool msgsend_protocol_stream(struct rpcprotocol_interface *protocol_interface);
bool msgflush_protocol_stream(struct rpcprotocol_interface *protocol_interface);
void destroy_protocol_stream(struct rpcprotocol_interface *protocol_interface);


#endif

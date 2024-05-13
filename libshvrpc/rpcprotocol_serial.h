#ifndef SHV_RPCPROTOCOL_SERIAL_H
#define SHV_RPCPROTOCOL_SERIAL_H

#include "rpcprotocol_common.h"
#include "crc.h"


static inline void conditional_crc_send_update(
	rpcprotocol_private *protocol, const uint8_t *data, size_t data_len) {
	if (protocol->layers.serial.crc_enabled)
		protocol->layers.serial.crc_send.as_crc_t = crc_update(
			protocol->layers.serial.crc_send.as_crc_t, data, data_len);
}

static inline void conditional_crc_recv_update(
	rpcprotocol_private *protocol, uint8_t byte) {
	if (protocol->layers.serial.crc_enabled)
		protocol->layers.serial.crc_recv.as_crc_t = crc_update(
			protocol->layers.serial.crc_recv.as_crc_t, &byte, SINGLE_BYTE);
}

static inline void conditional_crc_recv_init(rpcprotocol_private *protocol) {
	if (protocol->layers.serial.crc_enabled)
		protocol->layers.serial.crc_recv.as_crc_t = crc_init();
}

struct rpcprotocol_interface *rpcprotocol_serial_new(
	int fd, struct rpcprotocol_info *trans_info, bool crc_enabled);
bool nextmsg_protocol_serial(struct rpcprotocol_interface *protocol_interface);
bool validmsg_protocol_serial(struct rpcprotocol_interface *protocol_interface);
bool msgsend_protocol_serial(struct rpcprotocol_interface *protocol_interface);
bool is_sending_msg_in_progress(struct rpcprotocol_interface *protocol_interface);
bool abort_message(struct rpcprotocol_interface *protocol_interface);

#endif

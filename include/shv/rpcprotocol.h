/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCPROTOCOL_H
#define SHV_RPCPROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


/*! @file
 * This file defines the structures and enums used for specifying the protocol
 * information for RPC (Remote Procedure Call) communication. It includes
 * details about the communication scheme, transport layer, and destination
 * specifics.
 */

/*! Enum defining the protocol schemes available for RPC communication.
 */
enum protocol_scheme {
	/*! TCP/IP protocol for network communication. */
	TCP,
	/*! Unix Domain Sockets for local communication. */
	UNIX,
	/*! Serial communication over TTY devices. */
	TTY
};

/*! Enum defining the transport layers used in RPC communication.
 */
enum transport_layer {
	/*! Stream-based transport layer, typically over TCP or UNIX sockets. */
	STREAM,
	/*! Serial transport layer, used over physical serial connections or
	   emulated TTY. */
	SERIAL
};

/*! Union holding additional protocol-specific information.
 */
union additional_protocol_info {
	/*! Port number for TCP/UNIX socket connections. */
	uint16_t port;
	/*! Baud rate for serial (TTY) connections. */
	uint32_t baudrate;
};

/*! Structure defining a destination for RPC communication.
 */
struct protocol_destination {
	/*! File path or address of the destination. */
	char *name;
	/*! Additional connection-specific information.
	 * See @ref additional_protocol_info for more information.
	 */
	union additional_protocol_info additional_info;
};

/*! Structure holding information about the RPC protocol being used.
 */
struct rpcprotocol_info {
	/*! The protocol scheme.
	 * See @ref protocol_scheme for more information.
	 */
	enum protocol_scheme protocol_scheme;
	/*! The transport layer.
	 * See @ref transport_layer for more information.
	 */
	enum transport_layer transport_layer;
	/*! Destination specifics.
	 * See @ref protocol_destination for more information.
	 */
	struct protocol_destination destination;
};

/*! Structure representing an interface for RPC protocol communication.
 */
struct rpcprotocol_interface {
	/*! Byte stream for low-level byte-wise communication. */
	FILE *byte_stream;
	/*! Message stream for higher-level, structured messages. */
	FILE *message_stream;
	/*! Pointer to the transport information.
	 * See @ref rpcprotocol_info for more information.
	 */
	struct rpcprotocol_info *trans_info;
	/*! Error number indicating the last error occurred. */
	int errnum;
};

#endif

/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCCLIENT_URL_H
#define SHV_RPCCLIENT_URL_H
/*! @file
 */

#include <shv/rpcclient_stream.h>
#include <shv/rpcurl.h>

/*! Create RPC client based on the provided URL.
 *
 * @param url RPC URL with connection info. The URL location string must stay
 *   valid and unchanged during the client's existence because it is used
 *   directly by created client object.
 * @returns SHV RPC client handle.
 */
rpcclient_t rpcclient_new(const struct rpcurl *url);

/*! Establish a new connection based on the provided URL.
 *
 * This is convenient combination of @ref rpcclient_new and @ref
 * rpcclient_reset.
 *
 * @param url RPC URL with connection info.
 * @returns SHV RPC client handle or `NULL` in case connection failed; you can
 *   investigate the `errno` to identify why that might have been the case.
 */
rpcclient_t rpcclient_connect(const struct rpcurl *url);

#endif

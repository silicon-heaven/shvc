/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCACCESS_H
#define SHV_RPCACCESS_H
#include <stdint.h>

/**
 * Access levels used in the SHV RPC.
 */

/** Access levels supported by SHVC. */
typedef uint8_t rpcaccess_t;

/** Alias for minimal access level that is not used in SHV RPC. */
#define RPCACCESS_NONE (0)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * The lowest possible access level. This level allows user to list SHV nodes
 * and to discover methods. Nothing more is allowed.
 */
#define RPCACCESS_BROWSE (1)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with read access and thus access should be allowed only to
 * methods that perform reading of values. Those methods should not have side
 * effects.
 */
#define RPCACCESS_READ (8)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with write access and thus access should be allowed to the
 * method that modify some values.
 */
#define RPCACCESS_WRITE (16)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to methods that control and command.
 */
#define RPCACCESS_COMMAND (24)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to methods used to modify configuration.
 */
#define RPCACCESS_CONFIG (32)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to methods used to service devices and SHV network.
 */
#define RPCACCESS_SERVICE (40)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to methods used to service devices and SHV network
 * that can harm the network or device.
 */
#define RPCACCESS_SUPER_SERVICE (48)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to methods used only for development purposes.
 */
#define RPCACCESS_DEVEL (56)
/** Access level for :c:type:`rpcaccess_t`.
 *
 * Provides user with access to all methods.
 */
#define RPCACCESS_ADMIN (63)

/** Convert access level to string representation for granted access.
 *
 * Granted access is pre-SHV 3.0 access control mechanism that was based on the
 * strings. The implementation is provided for the compatibility purposes.
 *
 * :param access: Access level to be converted.
 * :return: Pointer to string constant representing the access level. Empty
 *   string is returned for unsupported access levels.
 */
const char *rpcaccess_granted_str(rpcaccess_t access);

/** Convert string representation for granted access to access level.
 *
 * :param str: Granted access string representation.
 * :return: Access level (in case of an invalid string it returns
 *   :c:macro:`RPCACCESS_NONE`).
 */
[[gnu::nonnull]]
rpcaccess_t rpcaccess_granted_access(const char *str);

/** Extract access level from access granted string.
 *
 * The access level is removed from the passed string (and thus making it
 * shorter).
 *
 * Granted access is pre-SHV 3.0 access control mechanism that was based on the
 * strings. The implementation is provided for the compatibility purposes.
 *
 * :param str: String with access specifier.
 * :return: Access level.
 */
[[gnu::nonnull]]
rpcaccess_t rpcaccess_granted_extract(char *str);

#endif

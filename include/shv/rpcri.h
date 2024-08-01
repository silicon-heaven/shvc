/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCRI_H
#define SHV_RPCRI_H
/*! @file
 * RPC RI is is a representation of resource identifier. Resource identifiers
 * are human readable and are used in the broker's signal filtering. They can
 * also be used by other tools. The format is either PATH:METHOD for methods
 * or PATH:METHOD:SIGNAL for signals.
 */

#include <stdbool.h>

/*! Check if given path or method matches given RPC RI.
 *
 *  The pattern matching works for standard wildcard patterns '?', '*' and
 *  '[' (including negation with '!') and follows POSIX.2, 3.13. It also
 *  supports double wildcard '**' that matches multiple nodes.
 *
 * @param ri: RPC RI that should be used to match method or signal.
 * @param path: SHV Path that that RI should match.
 * @param method: SHV RPC method name that RI should match.
 * @param signal: RPC signal name that RI should match. This can be
 *   ``NULL`` and in such case RI matches method (``PATH:METHOD``) or non-empty
 *   string for signal matching (``PATH:METHOD:SIGNAL``).
 *   `NULL` can be passed if you are not interested in the error location.
 * @returns True if RI matches, false otherwise.
 */
bool rpcri_match(const char *ri, const char *path, const char *method,
	const char *signal) __attribute__((nonnull(1, 2, 3)));

#endif /* SHV_RPCRI_H */

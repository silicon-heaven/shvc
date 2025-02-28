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

/*! Check if given method or signal matches given RPC RI.
 *
 *  The pattern matching works for standard wildcard patterns '?', '*' and
 *  '[' (including negation with '!') and follows POSIX.2, 3.13. It also
 *  supports double wildcard '**' that matches multiple nodes.
 *
 * @param ri: RPC RI that should be used to match method or signal.
 * @param path: SHV Path that RI should match.
 * @param method: SHV RPC method name that RI should match.
 * @param signal: RPC signal name that RI should match. This can be `NULL` and
 *   in such case RI matches method (`PATH:METHOD`) or non-empty string for
 *   signal matching (`PATH:METHOD:SIGNAL`).
 * @returns `true` if RI matches, `false` otherwise.
 */
[[gnu::nonnull(1, 2, 3)]]
bool rpcri_match(
	const char *ri, const char *path, const char *method, const char *signal);

/*! Check if given path matches given pattern.
 *
 * This is provided separately from RI due to not being valid RI. It is the same
 * pattern matching that is applied to the `PATH` portion of the RPC RI.
 *
 * @param pattern: The pattern path should match.
 * @param path: The patch to be compared against the pattern.
 * @returns `true` if path matches the pattern and `false` otherwise.
 */
[[gnu::nonnull]]
bool rpcpath_match(const char *pattern, const char *path);

/*! Check if given string matches given pattern.
 *
 * This is provided separately from RI due to not being valid RI. It is the same
 * pattern matching that is applied to the `METHOD` and `SIGNAL` portion of the
 * RPC RI.
 *
 * @param pattern: The pattern string should match.
 * @param string: The string to be compared against the pattern.
 * @returns `true` if string matches the pattern and `false` otherwise.
 */
[[gnu::nonnull]]
bool rpcstr_match(const char *pattern, const char *string);

#endif /* SHV_RPCRI_H */

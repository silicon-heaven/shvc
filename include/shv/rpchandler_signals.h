/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_SIGNALS_H
#define SHV_RPCHANDLER_SIGNALS_H
#include <shv/rpchandler.h>
#include <shv/rpcri.h>

/**
 * Handler for the signals subscription and management.
 *
 * It is implemented in such a way that you can just add RIs you want to
 * subscribe for and this handled will in background subscribe for them. It will
 * also in background perform any unsubscribe operations.
 */

/** Object representing SHV RPC Signal Handle. */
typedef struct rpchandler_signals *rpchandler_signals_t;

/** The prototype of the function that is called when signal is received.
 *
 * This is convenient way to get signal messages out of the RPC Handler. It is
 * pretty much only hook that is called from :c:member:`rpchandler_funcs.msg`
 * and thus for implementation documentation refer there.
 *
 * Warning: The stage for efficiency expects that it is the only one who
 * performs subscriptions and thus it won't filter subscriptions according to
 * the real subscribed RIs! It will be called for all signals reaching the
 * assigned handler's stage.
 */
[[gnu::nonnull(2)]]
typedef void (*rpchandler_signal_func_t)(
	void *cookie, struct rpchandler_msg *msgctx);

/** Create new RPC Signals Handle.
 *
 * :param func: The function that is called when signal is received.
 * :param cookie: The cookie passed to the `func`.
 * :return: A new RPC Signals Handler object.
 */
[[gnu::malloc]]
rpchandler_signals_t rpchandler_signals_new(
	rpchandler_signal_func_t func, void *cookie);

/** Free all resources occupied by :c:type:`rpchandler_signals_t` object.
 *
 * This is destructor for the object created by
 * :c:func:`rpchandler_signals_new`.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 */
void rpchandler_signals_destroy(rpchandler_signals_t rpchandler_signals);

/** Get the RPC Handler stage for this Signals Handler.
 *
 * This provides you with stage to be used in list of stages for the RPC
 * handler.
 *
 * If you specify ``func`` to :c:func:`rpchandler_signals_new` then this should
 * most likely be the last stage in the pipeline because it will consume all
 * signals.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 * :return: Stage to be used in array of stages for RPC Handler.
 */
[[gnu::nonnull]]
struct rpchandler_stage rpchandler_signals_stage(
	rpchandler_signals_t rpchandler_signals);

/** Add RI to be subscribed for.
 *
 * The subscription happens on the background and thus return from this function
 * doesn't signal immediate propagation. You can even call this before you start
 * RPC handler or even include this object in the stages.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 * :param ri: String containing RPC RI. It doesn't have to stay valid after
 *   function return.
 */
[[gnu::nonnull]]
void rpchandler_signals_subscribe(
	rpchandler_signals_t rpchandler_signals, const char *ri);

/** Remove previously subscribed RI.
 *
 * The removal happens on the background and thus return from this function
 * doesn't signal immediate propagation.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 * :param ri: String containing RPC RI. It doesn't have to stay valid after
 *   function return.
 */
[[gnu::nonnull]]
void rpchandler_signals_unsubscribe(
	rpchandler_signals_t rpchandler_signals, const char *ri);

/** Query if all subscribe and unsubscribe operations were performed.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 * :return: ``true`` if all subscribes and unsubscribes so far were propagated
 *   to the server and ``false`` otherwise.
 */
[[gnu::nonnull]]
bool rpchandler_signals_status(rpchandler_signals_t rpchandler_signals);

/** Wait until all subscribe and unsubscribe operations are performed.
 *
 * :param rpchandler_signals: RPC Signals Handler object.
 * :param abstime: The absolute time when wait will result into a timeout. It
 *   can be ``NULL`` if it should never timeout.
 * :return: ``false`` on timeout and ``true`` on success.
 */
[[gnu::nonnull(1)]]
bool rpchandler_signals_wait(
	rpchandler_signals_t rpchandler_signals, struct timespec *abstime);

#endif

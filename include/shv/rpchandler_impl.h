/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_IMPL_H
#define SHV_RPCHANDLER_IMPL_H
/*! @file
 * Functions for exclusive usage from @ref rpchandler_funcs functions.
 */

#include <shv/rpchandler.h>
#include <shv/rpcdir.h>


/*! Validate the received message.
 *
 * This calls @ref rpcclient_validmsg under the hood. You must call it to
 * validate that message is valid. Do not act on the received data if this
 * function returns `false`.
 *
 * This function should be called before return from @ref rpchandler_funcs.msg
 * to check if the received values are actually correct. Contrary to the @ref
 * rpcclient_validmsg this can be called multiple times in the same message
 * handler.
 *
 * @param ctx: Handle context passed to @ref rpchandler_funcs.msg.
 * @returns `true` if message is valid and `false` otherwise.
 */
bool rpchandler_msg_valid(struct rpchandler_msg *ctx) __attribute__((nonnull));


/*! Add result of `ls`.
 *
 * This is intended to be called only from @ref rpchandler_funcs.ls functions.
 *
 * The same **name** can be specified multiple times but it is added only once.
 * The reason for this is because in the tree the same node might belong to
 * multiple handlers and thus multiple handlers would report it and because they
 * can't know about each other we just need to prevent from duplicates to be
 * added.
 *
 * @param ctx: Context passed to the @ref rpchandler_funcs.ls.
 * @param name: Name of the node.
 */
void rpchandler_ls_result(struct rpchandler_ls *ctx, const char *name)
	__attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with constant string.
 *
 * @ref rpchandler_dir_result needs to duplicate **name** to filter out
 * duplicates. That is not required if string is constant or can be considered
 * such for the time of ls method handling. This function simply takes pointer
 * to the **name** and uses it internally in RPC Handler without copying it.
 *
 * @param ctx: Context passed to the @ref rpchandler_funcs.ls.
 * @param name: Name of the node (must be pointer to memory that is kept valid
 *   for the duration of ls method handler).
 */
void rpchandler_ls_result_const(struct rpchandler_ls *ctx, const char *name)
	__attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with name generated from
 * format string.
 *
 * @param ctx: Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt: Format string used to generate node name.
 */
void rpchandler_ls_result_fmt(struct rpchandler_ls *ctx, const char *fmt, ...)
	__attribute__((nonnull));

/*! The variant of the @ref rpchandler_ls_result with name generated from
 * format string.
 *
 * @param ctx: Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt: Format string used to generate node name.
 * @param args: List of variable arguments used in format string.
 */
void rpchandler_ls_result_vfmt(struct rpchandler_ls *ctx, const char *fmt,
	va_list args) __attribute__((nonnull));


/*! Add result of `dir`.
 *
 * This is intended to be called only from @ref rpchandler_funcs.dir functions.
 *
 * Compared to the @ref rpchandler_ls_result is not filtering duplicates,
 * because handlers should not have duplicate methods between each other. There
 * is also no need for function like @ref rpchandler_ls_result_const
 * because **method** is used immediately without need to preserve it.
 *
 * @param ctx: Context passed to the @ref rpchandler_funcs.dir.
 * @param method: The method description.
 */
void rpchandler_dir_result(struct rpchandler_dir *ctx,
	const struct rpcdir *method) __attribute__((nonnull));


/// @cond
struct obstack *_rpchandler_obstack(rpchandler_t rpchandler)
	__attribute__((nonnull));
/// @endcond
/*! Provides access to the obstack in RPC Handler object.
 *
 * You can use this to temporally store data in it. All data will be freed
 * right after return from any of the @ref rpchandler_funcs functions.
 *
 * @param CTX: Context passed to @ref rpchandler_funcs functions.
 * @returns Pointer to the obstack instance.
 */
#define rpchandler_obstack(CTX) \
	_Generic((CTX), \
		const struct rpchandler_msg *: _rpchandler_obstack, \
		const struct rpchandler_msg *: _rpchandler_obstack, \
		const struct rpchandler_idle *: _rpchandler_obstack)( \
		(rpchandler_t)(CTX)->_internal)


/*! Combined call to the @ref rpchandler_msg_new and @ref rpcmsg_pack_response.
 *
 * This function should be followed by packing of response result value, message
 * closing @ref cp_pack_container_end and sent with @ref rpchandler_msg_send.
 *
 * @param ctx: Context passed to @ref rpchandler_funcs.msg
 * @returns Packer object or `NULL` in case resposne can't be sent.
 */
__attribute__((nonnull)) static inline cp_pack_t rpchandler_msg_new_response(
	struct rpchandler_msg *ctx) {
	cp_pack_t res = rpchandler_msg_new(ctx);
	if (res)
		rpcmsg_pack_response(res, &ctx->meta);
	return res;
}

/*! Combined call to the @ref rpchandler_msg_new, @ref
 * rpcmsg_pack_response_void, and @ref rpchandler_msg_send.
 *
 * @param ctx: Context passed to @ref rpchandler_funcs.msg
 * @returns Value returned from @ref rpchandler_msg_send.
 */
__attribute__((nonnull)) static inline bool rpchandler_msg_send_response_void(
	struct rpchandler_msg *ctx) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_response_void(pack, &ctx->meta);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_error, and
 * @ref rpchandler_msg_send.
 *
 * @param ctx: Context passed to @ref rpchandler_funcs.msg
 * @param error: Error code to be reported as response to the request.
 * @param msg: Optional message describing the error details.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
__attribute__((nonnull)) static inline bool rpchandler_msg_send_error(
	struct rpchandler_msg *ctx, rpcerrno_t error, const char *msg) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_error(pack, &ctx->meta, error, msg);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_vferror, and
 * @ref rpchandler_msg_send.
 *
 * @param ctx: Context passed to @ref rpchandler_funcs.msg
 * @param error: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @param args: Variable list of arguments to be used with **fmt**.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
__attribute__((nonnull)) static inline bool rpchandler_msg_send_vferror(
	struct rpchandler_msg *ctx, rpcerrno_t error, const char *fmt, va_list args) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_vferror(pack, &ctx->meta, error, fmt, args);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_ferror, and
 * @ref rpchandler_msg_send.
 *
 * @param ctx: Context passed to @ref rpchandler_funcs.msg
 * @param error: Error code to be reported as response to the request.
 * @param fmt: Format string used to generate the error message.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
__attribute__((nonnull)) static inline bool rpchandler_msg_send_ferror(
	struct rpchandler_msg *ctx, rpcerrno_t error, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool res = rpchandler_msg_send_vferror(ctx, error, fmt, args);
	va_end(args);
	return res;
}

#endif

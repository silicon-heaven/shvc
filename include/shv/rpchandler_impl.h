/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_IMPL_H
#define SHV_RPCHANDLER_IMPL_H
/*! @file
 * Functions for exclusive usage from @ref rpchandler_funcs functions.
 */

#include <shv/rpcdir.h>
#include <shv/rpchandler.h>


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
 * @param ctx Handle context passed to @ref rpchandler_funcs.msg.
 * @returns `true` if message is valid and `false` otherwise.
 */
[[gnu::nonnull]]
bool rpchandler_msg_valid(struct rpchandler_msg *ctx);

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
 * @param ctx Context passed to the @ref rpchandler_funcs.ls.
 * @param name Name of the node.
 */
[[gnu::nonnull]]
void rpchandler_ls_result(struct rpchandler_ls *ctx, const char *name);

/*! The variant of the @ref rpchandler_ls_result with constant string.
 *
 * @ref rpchandler_dir_result needs to duplicate **name** to filter out
 * duplicates. That is not required if string is constant or can be considered
 * such for the time of ls method handling. This function simply takes pointer
 * to the **name** and uses it internally in RPC Handler without copying it.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.ls.
 * @param name Name of the node (must be pointer to memory that is kept valid
 *   for the duration of ls method handler).
 */
[[gnu::nonnull]]
void rpchandler_ls_result_const(struct rpchandler_ls *ctx, const char *name);

/*! The variant of the @ref rpchandler_ls_result with name generated from
 * format string.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt Format string used to generate node name.
 */
[[gnu::nonnull]]
void rpchandler_ls_result_fmt(struct rpchandler_ls *ctx, const char *fmt, ...);

/*! The variant of the @ref rpchandler_ls_result with name generated from
 * format string.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.ls.
 * @param fmt Format string used to generate node name.
 * @param args List of variable arguments used in format string.
 */
[[gnu::nonnull]]
void rpchandler_ls_result_vfmt(
	struct rpchandler_ls *ctx, const char *fmt, va_list args);

/*! Mark the node @ref rpchandler_ls.name as existing.
 *
 * This should be used only if @ref rpchandler_ls.name is not `NULL`. After
 * calling this you should return from @ref rpchandler_funcs.ls to allow
 * handler immediately act upon it.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.ls.
 */
[[gnu::nonnull]]
void rpchandler_ls_exists(struct rpchandler_ls *ctx);


/*! Add result of `dir`.
 *
 * This is intended to be called only from @ref rpchandler_funcs.dir functions.
 *
 * Compared to the @ref rpchandler_ls_result is not filtering duplicates,
 * because handlers should not have duplicate methods between each other. There
 * is also no need for function like @ref rpchandler_ls_result_const
 * because **method** is used immediately without need to preserve it.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.dir.
 * @param method The method description.
 */
[[gnu::nonnull]]
void rpchandler_dir_result(struct rpchandler_dir *ctx, const struct rpcdir *method);

/*! Mark the method @ref rpchandler_dir.name as existing.
 *
 * This should be used only if @ref rpchandler_dir.name is not `NULL`. After
 * calling this you should return from @ref rpchandler_funcs.dir to allow
 * handler immediately act upon it.
 *
 * @param ctx Context passed to the @ref rpchandler_funcs.dir.
 */
[[gnu::nonnull]]
void rpchandler_dir_exists(struct rpchandler_dir *ctx);


/// @cond
[[gnu::nonnull]]
struct obstack *_rpchandler_msg_obstack(struct rpchandler_msg *ctx);
[[gnu::nonnull]]
struct obstack *_rpchandler_idle_obstack(struct rpchandler_idle *ctx);
/// @endcond
/*! Provides access to the obstack in RPC Handler object.
 *
 * You can use this to temporally store data in it. All data will be freed
 * right after return from any of the @ref rpchandler_funcs functions.
 *
 * @param CTX Context passed to @ref rpchandler_funcs functions.
 * @returns Pointer to the obstack instance.
 */
#define rpchandler_obstack(CTX) \
	_Generic((CTX), \
		struct rpchandler_msg *: _rpchandler_msg_obstack, \
		struct rpchandler_idle *: _rpchandler_idle_obstack)(CTX)


/*! Combined call to the @ref rpchandler_msg_new and @ref rpcmsg_pack_response.
 *
 * This function should be followed by packing of response result value and call
 * to @ref rpchandler_msg_send_response. You can abandon this message with @ref
 * rpchandler_msg_drop.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @returns Packer object or `NULL` in case resposne can't be sent.
 */
[[gnu::nonnull]]
static inline cp_pack_t rpchandler_msg_new_response(struct rpchandler_msg *ctx) {
	cp_pack_t res = rpchandler_msg_new(ctx);
	if (res)
		rpcmsg_pack_response(res, &ctx->meta);
	return res;
}

/*! Combined call to the @ref cp_pack_container_end and @ref
 * rpchandler_msg_send.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param pack Packer returned from @ref rpchandler_msg_new_response.
 * @returns Packer object or `NULL` in case resposne can't be sent.
 */
[[gnu::nonnull(1)]]
static inline bool rpchandler_msg_send_response(
	struct rpchandler_msg *ctx, cp_pack_t pack) {
	if (pack == NULL)
		return false;
	cp_pack_container_end(pack);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref
 * rpcmsg_pack_response_void, and @ref rpchandler_msg_send.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull]]
static inline bool rpchandler_msg_send_response_void(struct rpchandler_msg *ctx) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_response_void(pack, &ctx->meta);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_error, and
 * @ref rpchandler_msg_send.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param error Error code to be reported as response to the request.
 * @param msg Optional message describing the error details.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull(1)]]
static inline bool rpchandler_msg_send_error(
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
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param error Error code to be reported as response to the request.
 * @param fmt Format string used to generate the error message.
 * @param args Variable list of arguments to be used with **fmt**.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull]]
static inline bool rpchandler_msg_send_vferror(struct rpchandler_msg *ctx,
	rpcerrno_t error, const char *fmt, va_list args) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_vferror(pack, &ctx->meta, error, fmt, args);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_ferror, and
 * @ref rpchandler_msg_send.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param error Error code to be reported as response to the request.
 * @param fmt Format string used to generate the error message.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull]]
static inline bool rpchandler_msg_send_ferror(
	struct rpchandler_msg *ctx, rpcerrno_t error, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	bool res = rpchandler_msg_send_vferror(ctx, error, fmt, args);
	va_end(args);
	return res;
}

/*! Send error that method is not found as a response.
 *
 * This should be used if method is valid but access level is too low.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull]]
bool rpchandler_msg_send_method_not_found(struct rpchandler_msg *ctx);

/*! Combined call to the @ref rpchandler_msg_new and @ref rpcmsg_pack_signal.
 *
 * This function should be followed by packing of signal value and call to @ref
 * rpchandler_msg_send_signal. You can abandon this message with @ref
 * rpchandler_msg_drop.
 *
 * The signal path and user's ID is automatically taken from received request
 * and thus must be called only if `ctx->meta.type == RPCMSG_T_REQUEST`.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param source name of the method the signal is associated with.
 * @param signal name of the signal.
 * @param access The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @param repeat Signals that this is repeat of some previous signal and this
 *   value didn't change right now but some time in the past.
 * @returns Packer object or `NULL` in case signal can't be sent.
 */
[[gnu::nonnull]]
static inline cp_pack_t rpchandler_msg_new_signal(struct rpchandler_msg *ctx,
	const char *source, const char *signal, rpcaccess_t access, bool repeat) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack)
		rpcmsg_pack_signal(pack, ctx->meta.path, source, signal,
			ctx->meta.user_id, access, repeat);
	return pack;
}

/*! Combined call to the @ref cp_pack_container_end and @ref
 * rpchandler_msg_send.
 *
 * It is provided for visual consistency with @ref rpchandler_msg_new_signal.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param pack Packer returned from @ref rpchandler_msg_new_response.
 * @returns Packer object or `NULL` in case resposne can't be sent.
 */
[[gnu::nonnull(1)]]
static inline bool rpchandler_msg_send_signal(
	struct rpchandler_msg *ctx, cp_pack_t pack) {
	if (pack == NULL)
		return false;
	cp_pack_container_end(pack);
	return rpchandler_msg_send(ctx);
}

/*! Combined call to the @ref rpchandler_msg_new, @ref rpcmsg_pack_signal_void,
 * and @ref rpchandler_msg_send.
 *
 * The signal path and user's ID is automatically taken from received request
 * and thus must be called only if `ctx->meta.type == RPCMSG_T_REQUEST`.
 *
 * @param ctx Context passed to @ref rpchandler_funcs.msg
 * @param source name of the method the signal is associated with.
 * @param signal name of the signal.
 * @param access The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @param repeat Signals that this is repeat of some previous signal and this
 *   value didn't change right now but some time in the past.
 * @returns Value returned from @ref rpchandler_msg_send.
 */
[[gnu::nonnull]]
static inline bool rpchandler_msg_new_signal_void(struct rpchandler_msg *ctx,
	const char *source, const char *signal, rpcaccess_t access, bool repeat) {
	cp_pack_t pack = rpchandler_msg_new(ctx);
	if (pack == NULL)
		return false;
	rpcmsg_pack_signal_void(pack, ctx->meta.path, source, signal,
		ctx->meta.user_id, access, repeat);
	return rpchandler_msg_send(ctx);
}

/*! Check for null parameter and validate message.
 *
 * This is convenient combination of checking for null parameter and @ref
 * rpchandler_msg_valid.
 *
 * @param ctx Handle context passed to @ref rpchandler_funcs.msg.
 * @returns `true` if request should be handled and `false` otherwise.
 */
[[gnu::nonnull]]
static inline bool rpchandler_msg_valid_nullparam(struct rpchandler_msg *ctx) {
	bool is_null = !rpcmsg_has_value(ctx->item) ||
		cp_unpack_type(ctx->unpack, ctx->item) == CPITEM_NULL;
	if (rpchandler_msg_valid(ctx)) {
		if (is_null)
			return true;
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "No parameter expected");
	}
	return false;
}

/*! Check for standard `get` method parameter and validate message.
 *
 * This is convenient combination of checking for null or int parameter and @ref
 * rpchandler_msg_valid that can be used in `get` method implementation.
 *
 * @param ctx Handle context passed to @ref rpchandler_funcs.msg.
 * @param oldness The optional pointer where maximal age of the value will be
 *   stored. This age is in milliseconds as per SHV standard. It is valid only
 *   if `true` is returned.
 * @returns `true` if request should be handled and `false` otherwise.
 */
[[gnu::nonnull(1)]]
static inline bool rpchandler_msg_valid_getparam(
	struct rpchandler_msg *ctx, long long *oldness) {
	bool valid = true;
	if (rpcmsg_has_value(ctx->item)) {
		cp_unpack(ctx->unpack, ctx->item);
		valid = ctx->item->type == CPITEM_NULL || ctx->item->type == CPITEM_INT;
		if (ctx->item->type == CPITEM_INT && oldness)
			*oldness = ctx->item->as.Int;
	}
	if (rpchandler_msg_valid(ctx)) {
		if (valid)
			return true;
		rpchandler_msg_send_error(
			ctx, RPCERR_INVALID_PARAM, "No parameter or Int expected");
	}
	return false;
}

#endif

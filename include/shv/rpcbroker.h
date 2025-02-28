/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCBROKER_H
#define SHV_RPCBROKER_H
/*! @file
 * Implementaion of RPC Broker based on RPC Handler.
 */

#include <stdbool.h>
#include <shv/rpchandler.h>
#include <shv/rpclogin.h>
#include <shv/rpcserver.h>

/*! RPC Broker object */
typedef struct rpcbroker *rpcbroker_t;

/*! The callback used to get the access level for the given method on given
 * path. See @ref rpcbroker_role.access.
 *
 * @param cookie User provided pointer.
 * @param path The SHV path method is associated with.
 * @param method The SHV method name.
 * @return The deduced access level for this method.
 */
typedef rpcaccess_t (*rpcbroker_access_t)(
	void *cookie, const char *path, const char *method);

/*! Broker role definition. */
struct rpcbroker_role {
	/*! Name of the role.
	 *
	 * This is used only as identification of the assigned role when client info
	 * is queried.
	 */
	const char *name;
	/*! The callback used to deduce the access level for the node and method. */
	rpcbroker_access_t access;
	/*! The cookie passed to the `access` callback. */
	void *access_cookie;
	/*! Optional callback that is called when role is no longer required (client
	 * is unregistered).
	 *
	 * You can reuse the same role for more clients, you just have to use
	 * reference counting.
	 */
	/*! Assigned mount point or `NULL` if this client should not be accessible.
	 *
	 * Note that mount point must be unique and must not be bellow other already
	 * existing one.
	 */
	const char *mount_point;
	/*! Null terminated array of subscription RIs.
	 *
	 * This is used only during the initial setup and not used afterwards. It is
	 * expected that this is used only with some preallocated array and not
	 * allocated for every client because client's can't specify subscription in
	 * login.
	 */
	const char **subscriptions;
	/*! The callback used to free this role.
	 *
	 * Role is initialized and provided when client is registered or on login
	 * and especially with login it is problematic that client can get multiple
	 * roles in the same session if reset is used. Broker needs to have ability
	 * to just invalidate the previous role and this callback provides that
	 * ability.
	 *
	 * It can be `NULL` if this is statically allocated. It can also use
	 * reference counting to share the same role between multiple clients.
	 *
	 * @param role Pointer to this role structure.
	 */
	void (*free)(struct rpcbroker_role *role);
};

/*! Result for the @ref rpcbroker_login_t callback. */
struct rpcbroker_login_res {
	/*! This signals if @ref rpcbroker_login_res.errmsg was used (if `false`)
	 * or @ref rpcbroker_login_res.role (if `true`).
	 */
	bool success;
	// @cond
	union {
		// @endcond
		/*! The role assigned by this login. */
		const struct rpcbroker_role *role;
		/*! Optional pointer to error message. It can be NULL if default message
		 * should be used.
		 *
		 * This can be used to specify custom reason for login failure but it
		 * can't be dynamically generated because there is no way to free it
		 * afterwards. Instead you should always return pointer to static
		 * string.
		 */
		const char *errmsg;
		// @cond
	};
	// @endcond
};

/*! Callback used to validate login and provide role for the brokers' client.
 *
 * @param cookie User provided pointer.
 * @param login Pointer to the login information parsed from login request
 *   message.
 * @param nonce The nonce that was provided to the client.
 */
typedef struct rpcbroker_login_res (*rpcbroker_login_t)(
	void *cookie, const struct rpclogin *login, const char *nonce);

/*! Flags disabling the usage of the locks. You should specify this only if
 * broker is used in single threaded poll implementation. Never specify this in
 * multithreaded program.
 */
#define RPCBROKER_F_NOLOCK (1 << 0)


/*! Create a new SHV RPC Broker object.
 *
 * @param name The name identifying the broker. The string is used as is and
 *   thus it must be valid during the Broker object existence. You can pass
 *   `NULL` in case you do not want to name the broker.
 * @param login Callback used to verify the login information provided by the
 *   client and providing role for it.
 * @param login_cookie Cookie passed to the `login` callback.
 * @param flags The combination of `RPCBROKER_F_*` flags.
 * @returns Broker object.
 */
[[gnu::malloc, gnu::nonnull(2)]]
rpcbroker_t rpcbroker_new(
	const char *name, rpcbroker_login_t login, void *login_cookie, int flags);

/*! Destroy the SHV RPC Broker object. */
void rpcbroker_destroy(rpcbroker_t broker);

/*! Register client to the broker.
 *
 * This is client that has immediate access to the broker without having to
 * perform login before. The provided role must fully identify it.
 *
 * This is commonly used when application connects to some other broker.
 *
 * This call requires already existing RPC Handler but at the same time provides
 * initialization for its stages. This is because registration and handler
 * initialization is a chicken and an egg problem. The solution is to create
 * handler with stage array having reserved slots for broker. Pointers to those
 * stages are passed to the registration for fill in. You must start handling
 * messages with RPC Handler only after registration because otherwise you would
 * have those stages uninitialized.
 *
 * @param broker Broker object.
 * @param handler Peers' RPC handler.
 * @param access_stage Pointer to the first stage of the handler to be filled
 *   in. This stage modifies access level of the messages and thus it must be
 *   the first stage in the array.
 * @param rpc_stage Pointer to the stage to be filled in. This stage actually
 *   manages messages. It is commonly at the end of the stage array.
 * @param role The role used by this client. The role must be valid all the time
 *   client stays registered. You must pass `NULL` if role is assigned by login.
 * @returns ID assigned to the newly registered client.
 */
[[gnu::nonnull(1, 2, 3, 4)]]
int rpcbroker_client_register(rpcbroker_t broker, rpchandler_t handler,
	struct rpchandler_stage *access_stage, struct rpchandler_stage *rpc_stage,
	const struct rpcbroker_role *role);

/*! Remove client registration from the broker.
 *
 * @param broker Broker object.
 * @param client_id The ID of the previously registered client. That is
 *   integer previously returned either by @ref rpcbroker_client_register and
 *   not unregistered yet.
 */
[[gnu::nonnull]]
void rpcbroker_client_unregister(rpcbroker_t broker, int client_id);

/*! Provide RPC Handler object for registered client.
 *
 * @param broker Broker object.
 * @param client_id The ID of the previously registered client. That is
 *   integer previously returned by @ref rpcbroker_client_register and not
 *   unregistered yet.
 * @returns The @ref rpchandler_t or `NULL` in case of invalid client ID.
 */
[[gnu::nonnull]]
rpchandler_t rpcbroker_client_handler(rpcbroker_t broker, int client_id);


/*! Context used for sending signals through broker. */
struct rpcbroker_sigctx {
	/*! Packer you must use to pack signal parameter. */
	cp_pack_t pack;
};

/*! Prepare packing for signal propagated through the whole broker.
 *
 * Call to this functions must be eventually followed by @ref
 * rpcbroker_send_signal or @ref rpcbroker_drop_signal.
 *
 * @param broker Broker object.
 * @param path SHV path to the node method is associated with.
 * @param source name of the method the signal is associated with.
 * @param signal name of the signal.
 * @param uid User's ID to be added to the signal. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param access The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @param repeat Signals that this is repeat of some previous signal and this
 *   value didn't change right now but some time in the past.
 * @returns Context to be used for packing and released by @ref
 *   rpcbroker_send_signal or @ref rpcbroker_drop_signal. `NULL` is returned if
 *   this signal would not be propagated to any peer and thus should not be
 *   packed at all.
 */
[[gnu::nonnull(1, 2, 3, 4)]]
struct rpcbroker_sigctx *rpcbroker_new_signal(rpcbroker_t broker,
	const char *path, const char *source, const char *signal, const char *uid,
	rpcaccess_t access, bool repeat);

/*! Send signal message.
 *
 * @param ctx Context provided by @ref rpcbroker_new_signal.
 * @returns Boolean signaling if sending was successful.
 */
[[gnu::nonnull]]
bool rpcbroker_send_signal(struct rpcbroker_sigctx *ctx);

/*! Drop signal message.
 *
 * @param ctx Context provided by @ref rpcbroker_new_signal.
 * @returns Boolean signaling if dropping was successful.
 */
[[gnu::nonnull]]
bool rpcbroker_drop_signal(struct rpcbroker_sigctx *ctx);

/*! Send signal propagated through the whole broker.
 *
 * @param broker Broker object.
 * @param path SHV path to the node method is associated with.
 * @param source name of the method the signal is associated with.
 * @param signal name of the signal.
 * @param uid User's ID to be added to the signal. It can be `NULL` and in
 *   such case User ID won't be part of the message.
 * @param access The access level for this signal. This is used to filter
 *   access  to the signals and in general should be consistent with access
 *   level of the method this signal is associated with.
 * @param repeat Signals that this is repeat of some previous signal and this
 *   value didn't change right now but some time in the past.
 * @returns Boolean signaling if sending was successful.
 */
[[gnu::nonnull(1, 2, 3, 4)]]
bool rpcbroker_send_signal_void(rpcbroker_t broker, const char *path,
	const char *source, const char *signal, const char *uid, rpcaccess_t access,
	bool repeat);


/*! The state used in @ref rpcbroker_run. */
struct rpcbroker_state {
	/*! Broker object. */
	rpcbroker_t broker;

	/*! Name of the broker. This can be `NULL` if no name is given. */
	const char *name;

	/*! Array of servers the broker should accept clients from. */
	rpcserver_t *servers;
	/*! Number of servers in @ref rpcbroker_state.servers */
	size_t servers_cnt;

	/*! Callback used to register a new client to the broker.
	 *
	 * It is your responsibility to register the client to the broker through
	 * this function.
	 *
	 * This function must return client ID as returned by @ref
	 * rpcbroker_client_register or it call return negative number to signal
	 * that client wasn't registered.
	 */
	int (*new_client)(void *cookie, rpcbroker_t broker, rpcserver_t server,
		rpcclient_t client);
	/*! Callback used to remove the client from the broker.
	 *
	 * This client was previously registered through @ref
	 * rpcbroker_state.new_client.
	 */
	void (*del_client)(void *cookie, rpcbroker_t broker, int client_id);
	/*! The user specified pointer passed to the @ref rpcbroker_state.new_client
	 * and @ref rpcbroker_state.del_client.
	 */
	void *cookie;
};

/*! Run broker handling loop.
 *
 * This is single-threaded implementation (you can use @ref RPCBROKER_F_NOLOCK).
 *
 * @param state The state used for the broker execution.
 * @param halt Pointer to the variable that can be set non-zero in the signal
 *   handler to halt the loop on next iteration. You can pass `NULL` if you do
 *   not plan on terminating loop this way. Note that this works only if signal
 *   is delivered to this function's calling thread.
 */
[[gnu::nonnull(1)]]
void rpcbroker_run(
	const struct rpcbroker_state *state, volatile sig_atomic_t *halt);

#endif

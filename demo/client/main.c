#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_login.h>
#include <shv/rpchandler_responses.h>
#include <shv/rpccall.h>
#include "opts.h"

#define TRACK_ID "4"

static size_t logsiz = BUFSIZ > 128 ? BUFSIZ : 128;

struct track {
	long long *buf;
	size_t len, siz;
};

static void print_track(struct track *track) {
	for (size_t i = 0; i < track->len; i++)
		printf(" %lld", track->buf[i]);
}


static int rpccall_app_name(enum rpccall_stage stage, cp_pack_t pack,
	int request_id, cp_unpack_t unpack, struct cpitem *item, void *ctx) {
	char **name = ctx;
	switch (stage) {
		case CALL_S_PACK:
			rpcmsg_pack_request_void(pack, ".app", "name", request_id);
			return 0;
		case CALL_S_RESULT:
			free(*name);
			*name = cp_unpack_strdup(unpack, item);
			return 0;
		case CALL_S_VOID_RESULT:
			free(*name);
			*name = NULL;
			return 0;
		case CALL_S_ERROR:
			// TODO possibly store error string somewhere?
			return 1;
		case CALL_S_COMERR:
		case CALL_S_TIMERR:
			return -1;
	}
	return 0;
}

static int rpccall_has_device(enum rpccall_stage stage, cp_pack_t pack,
	int request_id, cp_unpack_t unpack, struct cpitem *item, void *ctx) {
	bool *has_device = ctx;
	switch (stage) {
		case CALL_S_PACK:
			rpcmsg_pack_request(pack, "test", "ls", request_id);
			cp_pack_str(pack, "device");
			cp_pack_container_end(pack);
			return 0;
		case CALL_S_RESULT:
			*has_device = cp_unpack_type(unpack, item) == CPITEM_BOOL &&
				item->as.Bool;
		case CALL_S_VOID_RESULT:
		case CALL_S_ERROR:
			return 0;
		case CALL_S_COMERR:
		case CALL_S_TIMERR:
			return -1;
	}
	return 0;
}

static int rpccall_track_get(enum rpccall_stage stage, cp_pack_t pack,
	int request_id, cp_unpack_t unpack, struct cpitem *item, void *ctx) {
	struct track *track = ctx;
	switch (stage) {
		case CALL_S_PACK:
			rpcmsg_pack_request_void(
				pack, "test/device/track/" TRACK_ID, "get", request_id);
			return 0;
		case CALL_S_RESULT:
			track->len = 0;
			for_cp_unpack_list(unpack, item) {
				if (track->siz <= track->len) {
					assert(track->siz > 0);
					long long *new_track = realloc(
						track->buf, (track->siz *= 2) * sizeof *track->buf);
					assert(new_track);
					track->buf = new_track;
				}
				if (item->type == CPITEM_INT)
					track->buf[track->len++] = item->as.Int;
			}
			return 0;
		case CALL_S_VOID_RESULT:
			track->len = 0;
			return 0;
		case CALL_S_ERROR:
			// TODO possibly store error string somewhere?
			return 1;
		case CALL_S_COMERR:
		case CALL_S_TIMERR:
			return -1;
	}
	return 0;
}

static int rpccall_track_set(enum rpccall_stage stage, cp_pack_t pack,
	int request_id, cp_unpack_t unpack, struct cpitem *item, void *ctx) {
	struct track *track = ctx;
	switch (stage) {
		case CALL_S_PACK:
			rpcmsg_pack_request(
				pack, "test/device/track/" TRACK_ID, "set", request_id);
			cp_pack_list_begin(pack);
			for (size_t i = 0; i < track->len; i++)
				cp_pack_int(pack, track->buf[i]);
			cp_pack_container_end(pack);
			cp_pack_container_end(pack);
			break;
		case CALL_S_RESULT:
		case CALL_S_VOID_RESULT:
			return 0;
		case CALL_S_ERROR:
			// TODO possibly store error string somewhere?
			return 1;
		case CALL_S_COMERR:
		case CALL_S_TIMERR:
			return -1;
	}
	return 0;
}

int main(int argc, char **argv) {
	int exit_code = 1;
	struct conf conf;
	parse_opts(argc, argv, &conf);

	/* Setup client connection. */
	const char *errpos;
	struct rpcurl *url = rpcurl_parse(conf.url, &errpos);
	if (url == NULL) {
		fprintf(stderr, "Error: Invalid URL: '%s'.\n", conf.url);
		/* The number 21 in this case refers to the number of chars that precede
		 * the string format specifier on the line above to print URL */
		size_t offset = errpos ? errpos - conf.url : 0;
		fprintf(stderr, "%*s^\n", 21 + (unsigned)offset, "");
		return exit_code;
	}
	rpcclient_t client = rpcclient_connect(url);
	if (client == NULL) {
		fprintf(stderr, "Error: Connection failed to: '%s'.\n", conf.url);
		fprintf(stderr, "Hint: Make sure broker is running and URL is correct.\n");
		rpcurl_free(url);
		return exit_code;
	}
	client->logger_in =
		rpclogger_new(rpclogger_func_stderr, "<= ", logsiz, conf.verbose);
	client->logger_out =
		rpclogger_new(rpclogger_func_stderr, "=> ", logsiz, conf.verbose);

	/* Define a stages for RPC Handler using App and Responses Handler */
	rpchandler_login_t login = rpchandler_login_new(&url->login);
	rpchandler_app_t app = rpchandler_app_new("demo-client", PROJECT_VERSION);
	rpchandler_responses_t responses = rpchandler_responses_new();
	const struct rpchandler_stage stages[] = {
		rpchandler_login_stage(login),
		rpchandler_app_stage(app),
		rpchandler_responses_stage(responses),
		{},
	};

	/* Initialize and run RPC Handler */
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	pthread_t rpchandler_thread;
	rpchandler_spawn_thread(handler, &rpchandler_thread, NULL);

	/* Wait for login to be performed */
	rpcmsg_error login_err;
	const char *login_errmsg;
	if (!rpchandler_login_wait(login, &login_err, &login_errmsg, NULL)) {
		exit_code = 3;
		fprintf(stderr, "Failed to login to: %s\n", conf.url);
		fprintf(stderr, "%s\n", login_errmsg);
		goto cleanup;
	}

	/* Query the name of the broker we are connected to */
	char *app_name = NULL;
	if (rpccall(handler, responses, rpccall_app_name, &app_name))
		goto cleanup;
	fprintf(stdout, "The '.app:name' is: %s\n", app_name);
	free(app_name);

	/* Check if demo device is available */
	bool has_device = false;
	if (rpccall(handler, responses, rpccall_has_device, &has_device))
		goto cleanup;
	if (!has_device) {
		fprintf(stderr, "Error: Device not mounted at 'test/device'.\n");
		fprintf(stderr, "Hint: Make sure the device is connected to the broker.\n");
		goto cleanup;
	}

	/* Get the track state */
	struct track track = {.siz = 4};
	track.buf = malloc(track.siz * sizeof *track.buf);
	if (rpccall(handler, responses, rpccall_track_get, &track))
		goto cleanup;
	printf("Demo device's track " TRACK_ID ":");
	print_track(&track);
	putchar('\n');

	/* Increase track */
	for (size_t i = 0; i < track.len; i++)
		track.buf[i]++;
	if (rpccall(handler, responses, rpccall_track_set, &track))
		goto cleanup;

	/* Get the updated version. */
	if (rpccall(handler, responses, rpccall_track_get, &track))
		goto cleanup;
	printf("New demo device's track " TRACK_ID ":");
	print_track(&track);
	putchar('\n');

	free(track.buf);

	exit_code = 0;

cleanup:
	// TODO rpcclient_disconnect(client); instead of cancel
	pthread_cancel(rpchandler_thread);
	pthread_join(rpchandler_thread, NULL);
	rpchandler_destroy(handler);
	rpchandler_responses_destroy(responses);
	rpchandler_app_destroy(app);
	rpchandler_login_destroy(login);
	rpclogger_destroy(client->logger_in);
	rpclogger_destroy(client->logger_out);
	rpcclient_destroy(client);
	rpcurl_free(url);

	return exit_code;
}

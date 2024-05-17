#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_responses.h>
#include <shv/rpccall.h>
#include "opts.h"
#include "rpc_connect.h"

#define TIMEOUT 300
#define TRACK_ID "4"


/* Common error handling for RPC method calls */
#define CASE_ERROR(METHOD) \
	case RPCCALL_COMERR: \
	case RPCCALL_TIMEOUT: \
		comerr = true; \
		fprintf(stderr, "Error: Failed to call '" METHOD "'\n"); \
		break; \
	case RPCCALL_ERROR: { \
		char *errmsg; \
		rpcmsg_unpack_error(CCTX_UNPACK, &CCTX_ITEM, NULL, &errmsg); \
		fprintf(stderr, "Error: Call to " METHOD ": %s\n", errmsg); \
		free(errmsg); \
		comerr = true; \
		break; \
	}

static bool track_get(rpchandler_t handler, rpchandler_responses_t responses,
	long long **track, size_t *track_siz, size_t *track_cnt) {
	bool comerr = false;
	rpccall_void(
		handler, responses, "test/device/track/" TRACK_ID, "get", 3, TIMEOUT) {
		case RPCCALL_RESULT:
			*track_cnt = 0;
			for_cp_unpack_list(CCTX_UNPACK, &CCTX_ITEM) {
				if (*track_siz <= *track_cnt) {
					long long *new_track =
						realloc(*track, (*track_siz *= 2) * sizeof *track);
					assert(new_track);
					*track = new_track;
				}
				if (CCTX_ITEM.type == CPITEM_INT)
					(*track)[(*track_cnt)++] = CCTX_ITEM.as.Int;
			}
			break;
		case RPCCALL_VOID_RESULT:
			*track_cnt = 0;
			break;
		case RPCCALL_PARAM:
			CASE_ERROR("test/device/track/" TRACK_ID ":get")
	}
	return !comerr;
}

int main(int argc, char **argv) {
	int exit_code = 1;
	struct conf conf;
	parse_opts(argc, argv, &conf);

	/* Setup client connection. */
	rpcclient_t client = rpc_connect(&conf);
	if (client == NULL)
		return exit_code;

	/* Define a stages for RPC Handler using App and Responses Handler */
	rpchandler_app_t app = rpchandler_app_new("demo-client", PROJECT_VERSION);
	rpchandler_responses_t responses_handler = rpchandler_responses_new();
	const struct rpchandler_stage stages[] = {
		rpchandler_app_stage(app),
		rpchandler_responses_stage(responses_handler),
		{},
	};

	/* Initialize and run RPC Handler */
	rpchandler_t handler = rpchandler_new(client, stages, NULL);
	pthread_t rpchandler_thread;
	rpchandler_spawn_thread(handler, NULL, &rpchandler_thread, NULL);


	bool comerr = false;

	/* Query the name of the broker we are connected to */
	char *app_name = NULL;
	rpccall_void(handler, responses_handler, ".app", "name", 3, TIMEOUT) {
		case RPCCALL_RESULT:
			free(app_name);
			app_name = cp_unpack_strdup(CCTX_UNPACK, &CCTX_ITEM);
			break;
		case RPCCALL_VOID_RESULT:
		case RPCCALL_PARAM:
			CASE_ERROR(".app:name")
	}
	if (comerr)
		goto cleanup;
	fprintf(stdout, "The '.app:name' is: %s\n", app_name);
	free(app_name);

	/* Check if demo device is available */
	bool has_device = false;
	rpccall(handler, responses_handler, "test", "ls", 3, TIMEOUT) {
		case RPCCALL_PARAM:
			cp_pack_str(cctx.pack, "device");
			break;
		case RPCCALL_RESULT:
			has_device = cp_unpack_type(CCTX_UNPACK, &CCTX_ITEM) == CPITEM_BOOL &&
				CCTX_ITEM.as.Bool;
			break;
		case RPCCALL_VOID_RESULT:
			has_device = false;
			break;
			CASE_ERROR("test:ls")
	}
	if (comerr)
		goto cleanup;
	if (!has_device) {
		fprintf(stderr, "Error: Device not mounted at 'test/device'.\n");
		fprintf(stderr, "Hint: Make sure the device is connected to the broker.\n");
		goto cleanup;
	}

	/* Get the track state */
	size_t track_siz = 4, track_cnt = 0;
	long long *track = malloc(track_siz * sizeof *track);
	if (!track_get(handler, responses_handler, &track, &track_siz, &track_cnt))
		goto cleanup;
	printf("Demo device's track " TRACK_ID ":");
	for (size_t i = 0; i < track_cnt; i++)
		printf(" %lld", track[i]);
	putchar('\n');


	/* Increase track */
	for (size_t i = 0; i < track_cnt; i++)
		track[i]++;
	rpccall(handler, responses_handler, "test/device/track/" TRACK_ID, "set", 3,
		TIMEOUT) {
		case RPCCALL_PARAM:
			cp_pack_list_begin(CCTX_PACK);
			for (size_t i = 0; i < track_cnt; i++)
				cp_pack_int(CCTX_PACK, track[i]);
			cp_pack_container_end(CCTX_PACK);
			break;
		case RPCCALL_RESULT:
		case RPCCALL_VOID_RESULT:
			break;
			CASE_ERROR("test:ls")
	}
	if (comerr)
		goto cleanup;

	/* Get the updated version. */
	if (!track_get(handler, responses_handler, &track, &track_siz, &track_cnt))
		goto cleanup;
	printf("New demo device's track " TRACK_ID ":");
	for (size_t i = 0; i < track_cnt; i++)
		printf(" %lld", track[i]);
	putchar('\n');

	exit_code = 0;

cleanup:
	// TODO rpcclient_disconnect(client); instead of cancel
	pthread_cancel(rpchandler_thread);
	pthread_join(rpchandler_thread, NULL);
	rpchandler_destroy(handler);
	rpchandler_responses_destroy(responses_handler);
	rpchandler_app_destroy(app);
	rpclogger_destroy(client->logger);
	rpcclient_destroy(client);

	return exit_code;
}

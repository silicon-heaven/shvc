#include <stdio.h>
#include <string.h>
#include <shv/rpcurl.h>
#include <shv/rpcclient.h>
#include <shv/rpchandler_app.h>
#include <shv/rpchandler_responses.h>
#include <malloc.h>
#include "opts.h"
#include "rpc_device_operations.h"
#include "rpc_connection.h"
#include "rpc_request.h"
#include "rpc_error_logging.h"
#include "utils.h"

int main(int argc, char **argv) {
	int return_value = 1;
	struct conf conf;
	parse_opts(argc, argv, &conf);

	/* Parse URL that's stored in `conf` structure */
	struct rpcurl *rpcurl;
	if (!parse_rpcurl(conf.url, &rpcurl))
		goto parse_rpcurl_cleanup;

	/* Connect to the RPC URL, initializing client in the process */
	rpcclient_t client;
	if (!connect_to_rpcurl(rpcurl, &client))
		goto connect_to_rpcurl_cleanup;

	/* Create a logger that's also supplied to our client instance */
	rpclogger_t logger = initiate_logger_if_verbose(conf, client);

	/* Log in using the client to a valid RPC URL */
	if (!login_with_rpcclient(client, rpcurl))
		goto login_with_rpcclient_cleanup;

	/* Define a stage for RPC Handler using App and Responses Handler */
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
	rpchandler_spawn_thread(handler, &error_handler, &rpchandler_thread, NULL);

	rpc_request_ctx ctx = {.handler = handler,
		.responses_handler = responses_handler,
		.timeout = CALL_TIMEOUT};

	if (!is_device_mounted(&ctx)) {
		fprintf(stderr, "Error: Device not mounted at 'test/device'.\n");
		fprintf(stderr, "Hint: Make sure the device is connected to the broker.\n");
		goto cleanup;
	}

	char *app_name = NULL;
	if (!get_app_name(&ctx, &app_name) || app_name == NULL) {
		fprintf(stderr, "Error: Couldn't obtain '.app:name'.\n");
		goto cleanup;
	}

	fprintf(stdout, "The '.app:name' is: %s\n", app_name);
	free(app_name);

	if (rpcclient_errno(client) != RPCMSG_E_NO_ERROR) {
		fprintf(stderr, "Error: %s\n", strerror(rpcclient_errno(client)));
		goto cleanup;
	}

	if (!track_getter_call(&ctx)) {
		fprintf(stderr, "Error: Call to 'track/" TRACK_ID ":get' failed.\n");
		goto cleanup;
	}

	size_t buf_length = 0;
	long long *data_buf = malloc(sizeof(long long) * 2);
	assert(data_buf);

	if (!track_getter_result(&ctx, &data_buf, &buf_length)) {
		fprintf(stderr,
			"Error: Parsing response from 'track/" TRACK_ID ":get' failed.\n");
		goto vector_cleanup;
	}

	printf("The value of track/" TRACK_ID " is: ");
	print_ll_array(data_buf, buf_length);

	if (rpcclient_errno(client) != RPCMSG_E_NO_ERROR) {
		fprintf(stderr, "Error: %s\n", strerror(rpcclient_errno(client)));
	}

	const long long array_to_set[] = {2, 3, 4, 8};
	const size_t length = sizeof(array_to_set) / sizeof(array_to_set[0]);

	if (!try_set_track(&ctx, array_to_set, length)) {
		fprintf(stderr, "Error: Call to 'track/" TRACK_ID ":set' failed.\n");
		goto vector_cleanup;
	}

	fprintf(stdout,
		"The value of track/" TRACK_ID " has successfully been set to: ");
	print_ll_array(array_to_set, length);

	return_value = 0;

vector_cleanup:
	free(data_buf);
cleanup:
	rpchandler_destroy(handler);
	rpchandler_responses_destroy(responses_handler);
	rpchandler_app_destroy(app);
login_with_rpcclient_cleanup:
	rpclogger_destroy(logger);
connect_to_rpcurl_cleanup:
	rpcclient_destroy(client);
parse_rpcurl_cleanup:
	rpcurl_free(rpcurl);

	return return_value;
}

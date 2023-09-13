#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <shv/cp.h>
#include "opts.h"


static void cpon_state_realloc(struct cpon_state *state) {
	state->cnt = (state->cnt ?: 1) * 2;
	state->ctx = realloc(state->ctx, state->cnt * sizeof *state->ctx);
}


int main(int argc, char **argv) {
	struct conf conf;
	parse_opts(argc, argv, &conf);

	if (conf.op == OP_DETECT) {
		// TODO detect form input and output names
		conf.op = OP_PACK;
	}

	/* Prepare our input */
	FILE *in;
	if (conf.input == NULL || !strcmp(conf.input, "-"))
		in = stdin;
	else {
		in = fopen(conf.input, "r");
		if (in == NULL) {
			fprintf(stderr, "Unable to open input file: %s: %s\n", conf.input,
				strerror(errno));
			return 1;
		}
	}

	/* Prepare access to the output */
	FILE *out;
	if (conf.output == NULL || !strcmp(conf.output, "-"))
		out = stdout;
	else {
		out = fopen(conf.output, "w");
		if (out == NULL) {
			fprintf(stderr, "Unable to open output file: %s: %s\n", conf.output,
				strerror(errno));
			return 1;
		}
	}

	/* States and handles we use to copy data from unpacker to the packer. */
	uint8_t buf[BUFSIZ];
	struct cpitem item = {
		.buf = buf,
		.bufsiz = BUFSIZ,
	};
	struct cpon_state cpon_state = {.indent = conf.pretty ? "  " : NULL,
		.ctx = NULL,
		.cnt = 0,
		.realloc = cpon_state_realloc};

	bool success = false;

	/* Unpack all items and pack them in the opposite format */
	while (({
		size_t res;
		if (conf.op == OP_PACK)
			res = cpon_unpack(in, &cpon_state, &item);
		else
			res = chainpack_unpack(in, &item);
		res > 0;
	})) {
		if (item.type == CPITEM_INVALID)
			break;
		ssize_t res;
		if (conf.op == OP_PACK)
			res = chainpack_pack(out, &item);
		else
			res = cpon_pack(out, &cpon_state, &item);
		if (res < 0)
			break;
	};

	/* Success is if we convert all input to the output */
	success = feof(in);

	free(cpon_state.ctx);
	if (in != stdin)
		fclose(in);
	if (out != stdout)
		fclose(out);
	return success ? 0 : 1;
}

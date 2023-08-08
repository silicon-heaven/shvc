/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#include <string.h>

#define SUITE "config"
#include <check_suite.h>

#include <config.h>

/* Test various command line arguments */
TEST_CASE(arguments) {}

char *parse_arguments_arg0[] = {"foo"};
char *parse_arguments_arg1[] = {"foo", "/dev/null"};

#define ARGUMENTS(arg) .argc = sizeof(arg) / sizeof(*(arg)), .argv = arg
const struct {
	struct config config;
	int argc;
	char **argv;
} parse_arguments_data[] = {
	{
		.config =
			{
				.source_file = NULL,
			},
		ARGUMENTS(parse_arguments_arg0),
	},
	{
		.config =
			{
				.source_file = "/dev/null",
			},
		ARGUMENTS(parse_arguments_arg1),
	},
};

ARRAY_TEST(arguments, parse_arguments, parse_arguments_data) {
	struct config conf;
	load_config(&conf, _d.argc, _d.argv);
	ck_assert_pstr_eq(conf.source_file, _d.config.source_file);
}
END_TEST

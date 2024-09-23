#ifndef _FOO_CONFIG_H_
#define _FOO_CONFIG_H_

struct config {
	const char *source_file;
};


/* Parse arguments and load configuration file
 * Returned pointer is to statically allocated (do not call free on it).
 */
void load_config(struct config *conf, int argc, char **argv)
	__attribute__((nonnull));

#endif

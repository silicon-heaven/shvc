/*****************************************************************************
 * Copyright (C) 2022 Elektroline Inc. All rights reserved.
 * K Ladvi 1805/20
 * Prague, 184 00
 * info@elektroline.cz (+420 284 021 111)
 *****************************************************************************/
#ifndef _FOO_CONFIG_H_
#define _FOO_CONFIG_H_


/* Parse arguments and load configuration file
 * Returned pointer is to statically allocated (do not call free on it).
 */
void load_config(int argc, char **argv) __attribute__((nonnull));

#endif

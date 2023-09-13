#ifndef BROKER_H
#define BROKER_H


extern int shvbroker_tcp_port;
extern char *shvbroker_unix_path;

void setup_shvbroker(void);

void teardown_shvbroker(void);

#endif

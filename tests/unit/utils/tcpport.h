#ifndef TCPPORT_H
#define TCPPORT_H

#include <stdbool.h>


/*! Check if given TCP/IP port is bound on localhost. */
bool tcpport_isbound(int port);

/* Find empty TCP/IP port for localost. */
int tcpport_empty(void);

#endif

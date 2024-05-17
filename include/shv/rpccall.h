/* SPDX-License-Identifier: MIT */
#ifndef SHV_RPCHANDLER_CALL_H
#define SHV_RPCHANDLER_CALL_H
/*! @file
 * Macro only utility to call methods with RPC Handler and receive resposne with
 * RPC Handler Responses.
 */

#include <shv/rpchandler_responses.h>


#define CCTX_PACK (cctx.pack)
#define CCTX_UNPACK (cctx.receive->unpack)
#define CCTX_ITEM (cctx.receive->item)
#define CCTX_META (cctx.meta)

enum rpccall_state {
	RPCCALL_PARAM,
	RPCCALL_VOID_RESULT,
	RPCCALL_RESULT,
	RPCCALL_ERROR,
	RPCCALL_COMERR,
	RPCCALL_TIMEOUT,
};

struct rpccall_ctx {
	enum rpccall_state state;
	cp_pack_t pack;
	rpcresponse_t response;
	struct rpcreceive *receive;
	const struct rpcmsg_meta *meta;
	/*! The attempt number starting with zero. */
	unsigned attempt;
};

#define rpccall(handler, responses, path, method, attempts, timeout) \
	for (struct rpccall_ctx cctx = {.state = RPCCALL_ERROR, .attempt = 0}; ({ \
			 bool __continue = true; \
			 switch (cctx.state) { \
				 case RPCCALL_PARAM: \
					 cp_pack_container_end(cctx.pack); \
					 cctx.pack = NULL; \
					 if (!rpchandler_msg_send(handler)) { \
						 rpcresponse_discard((responses), cctx.response); \
						 cctx.state = RPCCALL_COMERR; \
						 break; \
					 } \
					 cctx.attempt++; \
					 if (rpcresponse_waitfor(cctx.response, &cctx.receive, \
							 &cctx.meta, timeout)) { \
						 cctx.state = cctx.meta->type == RPCMSG_T_ERROR \
							 ? RPCCALL_ERROR \
							 : (rpcreceive_has_param(cctx.receive) \
									   ? RPCCALL_RESULT \
									   : RPCCALL_VOID_RESULT); \
						 break; \
					 } \
					 rpcresponse_discard((responses), cctx.response); \
					 cctx.response = NULL; \
					 if (cctx.attempt >= (attempts)) { \
						 cctx.state = RPCCALL_TIMEOUT; \
						 break; \
					 } \
				 case RPCCALL_RESULT: \
				 case RPCCALL_VOID_RESULT: \
				 case RPCCALL_ERROR: \
					 if (cctx.response != NULL && \
						 rpcresponse_validmsg(cctx.response)) { \
						 __continue = false; \
						 break; \
					 } \
					 int request_id = rpchandler_next_request_id(handler); \
					 cctx.pack = rpchandler_msg_new_request( \
						 (handler), (path), (method), request_id); \
					 if (cctx.pack) { \
						 cctx.state = RPCCALL_PARAM; \
						 cctx.response = \
							 rpcresponse_expect((responses), request_id); \
					 } else \
						 cctx.state = RPCCALL_COMERR; \
					 break; \
				 case RPCCALL_COMERR: \
				 case RPCCALL_TIMEOUT: \
					 __continue = false; \
					 break; \
			 } \
			 __continue; \
		 });) \
		switch (cctx.state)


#define rpccall_void(handler, responses, path, method, attempts, timeout) \
	for (struct rpccall_ctx cctx = {.state = RPCCALL_ERROR, .attempt = 0}; ({ \
			 bool __continue = true; \
			 switch (cctx.state) { \
				 case RPCCALL_PARAM: \
					 abort(); \
				 case RPCCALL_RESULT: \
				 case RPCCALL_VOID_RESULT: \
				 case RPCCALL_ERROR: \
					 if (cctx.response != NULL && \
						 rpcresponse_validmsg(cctx.response)) { \
						 __continue = false; \
						 break; \
					 } \
					 cctx.state = RPCCALL_TIMEOUT; \
					 while (cctx.attempt < (attempts)) { \
						 cctx.response = rpcresponse_send_request_void( \
							 (handler), (responses), (path), (method)); \
						 if (cctx.response == NULL) { \
							 cctx.state = RPCCALL_COMERR; \
							 break; \
						 } \
						 cctx.attempt++; \
						 if (rpcresponse_waitfor(cctx.response, &cctx.receive, \
								 &cctx.meta, timeout)) { \
							 cctx.state = cctx.meta->type == RPCMSG_T_ERROR \
								 ? RPCCALL_ERROR \
								 : (rpcreceive_has_param(cctx.receive) \
										   ? RPCCALL_RESULT \
										   : RPCCALL_VOID_RESULT); \
							 break; \
						 } \
						 rpcresponse_discard((responses), cctx.response); \
					 } \
					 break; \
				 case RPCCALL_COMERR: \
				 case RPCCALL_TIMEOUT: \
					 __continue = false; \
					 break; \
			 } \
			 __continue; \
		 });) \
		switch (cctx.state)

#endif

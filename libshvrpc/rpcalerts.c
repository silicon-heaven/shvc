#include <stdlib.h>
#include <shv/rpcalerts.h>
#include <shv/rpchandler_device.h>

bool rpcalerts_pack(cp_pack_t pack, const struct rpcalerts *alert) {
	cp_pack_imap_begin(pack);
	cp_pack_int(pack, RPCALERTS_KEY_DATE);
	cp_pack_datetime(pack, (struct cpdatetime){.msecs = alert->time, .offutc = 0});

	cp_pack_int(pack, RPCALERTS_KEY_LEVEL);
	cp_pack_int(pack, alert->level);

	cp_pack_int(pack, RPCALERTS_KEY_ID);
	cp_pack_str(pack, alert->id);
	cp_pack_container_end(pack);
	return true;
}

struct rpcalerts *rpcalerts_unpack(
	cp_unpack_t unpack, struct cpitem *item, struct obstack *obstack) {
	if (cp_unpack_type(unpack, item) != CPITEM_IMAP)
		return NULL;

	struct rpcalerts *res = obstack_alloc(obstack, sizeof(*res));
	*res = (struct rpcalerts){
		.time = 0,
		.level = 0,
		.id = "",
	};

	for_cp_unpack_imap(unpack, item, ikey) {
		switch (ikey) {
			case RPCALERTS_KEY_DATE:
				struct cpdatetime datetime;
				if (cp_unpack_datetime(unpack, item, datetime))
					res->time = datetime.msecs;
				break;
			case RPCALERTS_KEY_LEVEL:
				cp_unpack_int(unpack, item, res->level);
				break;
			case RPCALERTS_KEY_ID:
				res->id = cp_unpack_strdupo(unpack, item, obstack);
				break;
			default:
				cp_unpack_skip(unpack, item);
				break;
		}
	}

	return res;
}

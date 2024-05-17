#include <shv/cp_unpack_pack.h>

bool cp_unpack_pack(cp_unpack_t unpack, struct cpitem *item, cp_pack_t pack) {
	uint8_t buf[BUFSIZ];
	item->buf = buf;
	item->bufsiz = BUFSIZ;
	unsigned depth = 0;
	do {
		cp_unpack(unpack, item);
		switch (item->type) {
			case CPITEM_BLOB:
			case CPITEM_STRING:
				if (item->as.Blob.flags & CPBI_F_FIRST)
					depth++;
				if (item->as.Blob.flags & CPBI_F_LAST)
					depth--;
				break;
			case CPITEM_LIST:
			case CPITEM_MAP:
			case CPITEM_IMAP:
			case CPITEM_META:
				depth++;
				break;
			case CPITEM_CONTAINER_END:
				depth--;
				break;
			case CPITEM_INVALID:
				item->buf = NULL;
				item->bufsiz = 0;
				return false;
			default:
				break;
		}
		if (!cp_pack(pack, item)) {
			item->buf = NULL;
			item->bufsiz = 0;
			return false;
		}
	} while (depth);
	item->buf = NULL;
	item->bufsiz = 0;
	return true;
}

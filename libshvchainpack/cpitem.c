#include <shv/cp.h>

static const char *const typenames[] = {
	[CPITEM_INVALID] = "INVALID",
	[CPITEM_NULL] = "NULL",
	[CPITEM_BOOL] = "BOOL",
	[CPITEM_INT] = "INT",
	[CPITEM_UINT] = "UINT",
	[CPITEM_DOUBLE] = "DOUBLE",
	[CPITEM_DECIMAL] = "DECIMAL",
	[CPITEM_BLOB] = "BLOB",
	[CPITEM_STRING] = "STRING",
	[CPITEM_DATETIME] = "DATETIME",
	[CPITEM_LIST] = "LIST",
	[CPITEM_MAP] = "MAP",
	[CPITEM_IMAP] = "IMAP",
	[CPITEM_META] = "META",
	[CPITEM_CONTAINER_END] = "CONTAINER_END",
};

const char *cpitem_type_str(enum cpitem_type tp) {
	return typenames[tp <= CPITEM_CONTAINER_END ? tp : 0];
}

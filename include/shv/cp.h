#ifndef SHV_CP_H
#define SHV_CP_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


enum cp_format {
	CP_ChainPack = 1,
	CP_Cpon,
	CP_Json,
};


enum cp_item_type {
	CP_ITEM_INVALID = 0,
	CP_ITEM_NULL,
	CP_ITEM_BOOL,
	CP_ITEM_INT,
	CP_ITEM_UINT,
	CP_ITEM_DOUBLE,
	CP_ITEM_DECIMAL,
	CP_ITEM_BLOB,
	CP_ITEM_STRING,
	CP_ITEM_DATETIME,
	CP_ITEM_LIST,
	CP_ITEM_MAP,
	CP_ITEM_IMAP,
	CP_ITEM_META,
	CP_ITEM_CONTAINER_END,
};

const char *cp_item_type_str(enum cp_item_type);

/*!
 */
struct cpbuf {
	/*! Pointer to the buffer with data chunk.
	 *
	 * It is defined both as `const` as well as modifiable. The pack functions
	 * use `buf` while unpack functions use `rbuf`. Be aware that this can
	 * allow write to the buffer marked as `const` if you pass it to unpack
	 * function.
	 *
	 * The `chr` and `rchr` are additionally provided just to allow easy
	 * assignment of the strings without changing type (which would be required
	 * due to change of the sign).
	 */
	union {
		uint8_t *buf;
		const uint8_t *rbuf;
		char *chr;
		const char *rchr;
	};
	/*! Number of valid bytes in `buf` or `rbuf`. */
	size_t len;
	/*! Size of the `buf` or `rbuf`. This is used only by unpacking functions. */
	size_t siz;
	/*! Number of bytes not yet unpacked or `-1`. This can be used to inform
	 * packer about the full string length (it can use it to select a different
	 * packing method in some cases). It also serves as counter for unpacking
	 * functions, so it should not be modified in case of unpacking.
	 */
	ssize_t eoff;
	/*! Signaling of first and last block. You should set both to `true` if this
	 * is the only block to be packed. If you plan to append additional chunks
	 * you should set on first iteration only `first` and to terminate you need
	 * to set `last`.
	 */
	bool first, last;
};

struct cpdatetime {
	int64_t msecs;
	int32_t offutc;
};

struct cpdecimal {
	int64_t mantisa;
	int32_t exponent;
};

struct cpitem {
	enum cp_item_type type;
	union {
		struct cpbuf String;
		struct cpbuf Blob;
		struct cpdatetime Datetime;
		struct cpdecimal Decimal;
		uint64_t UInt;
		int64_t Int;
		double Double;
		bool Bool;
	} as;
};


ssize_t chainpack_pack(FILE *, const struct cpitem *) __attribute__((nonnull(2)));
ssize_t _chainpack_pack_uint(FILE *, uint64_t v);
size_t chainpack_unpack(FILE *, struct cpitem *) __attribute__((nonnull));
size_t _chainpack_unpack_uint(FILE *, uint64_t *v, bool *ok);


struct cpon_state {
	const char *indent;
	size_t depth;
	struct cpon_state_ctx {
		enum cp_item_type tp;
		bool first;
		bool even;
	} * ctx;
	size_t cnt;
	void (*realloc)(struct cpon_state *state);
};

ssize_t cpon_pack(FILE *, struct cpon_state *, const struct cpitem *)
	__attribute__((nonnull));
size_t cpon_unpack(FILE *, struct cpitem *) __attribute__((nonnull));


#endif

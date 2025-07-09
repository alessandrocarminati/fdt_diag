#include <stdarg.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <openssl/md5.h>
#include "unique_printf.h"

static unsigned char seen_hashes[PRINTF_BUFFER_MD5_POS][MD5_DIGEST_LENGTH];
static int seen_count = 0;

int is_md5_seen(const unsigned char* md5) {
	for (int i = 0; i < seen_count; ++i) {
		if (memcmp(seen_hashes[i], md5, MD5_DIGEST_LENGTH) == 0)
			return 1;
	}
	return 0;
}

void add_md5(const unsigned char* md5) {
	if (seen_count < PRINTF_BUFFER_MD5_POS) {
		memcpy(seen_hashes[seen_count], md5, MD5_DIGEST_LENGTH);
		seen_count++;
	}
}

int unique_printf_impl(const char *format, ...) {
	char buffer[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5((const unsigned char*)buffer, strlen(buffer), digest);

	if (!is_md5_seen(digest)) {
		add_md5(digest);
		return printf("%s", buffer);
	}
	return 0;
}


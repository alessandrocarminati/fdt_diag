#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fdt.h>
#include <stdarg.h>
#include <libfdt.h>
#include <openssl/md5.h>

#define PRINTF_BUFFER_MD5_POS 1024
#define SUPPLY_SUFFIX "-supply"
#define SUPPLY_SUFFIX_LEN strlen(SUPPLY_SUFFIX)
#define HAS_SUPPLY_SUFFIX_LEN(x)  strcmp((strlen(x)-SUPPLY_SUFFIX_LEN)+x, SUPPLY_SUFFIX)
#define UNIQUE_PRINTF(...) unique_printf_impl(__VA_ARGS__)
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
const char *get_property_phandle_name(const void *fdt, const struct fdt_property *prop){
	uint32_t phandle_be;
	memcpy(&phandle_be, prop->data, sizeof(uint32_t));
	uint32_t phandle = fdt32_to_cpu(phandle_be);
	int phandle_offset = fdt_node_offset_by_phandle(fdt, phandle);
	const char *phandle_name = fdt_get_name(fdt, phandle_offset, NULL);
	return phandle_name;
}

void process_node(const void *fdt, int nodeoffset) {
	const char *name = fdt_get_name(fdt, nodeoffset, NULL);
	int prop_len;


	const char *reg_name = fdt_getprop(fdt, nodeoffset, "regulator-name", &prop_len);
	if (reg_name && prop_len > 0) {
		UNIQUE_PRINTF("\"%s\" [label=\"%s\", shape=box, fillcolor=lightgrey, style=filled];\n", name, reg_name);

		int prop_offset;
		fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
			const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
			const char *prop_name = fdt_get_string(fdt, fdt32_to_cpu(prop->nameoff), NULL);

			if (strcmp("regulator-coupled-with", prop_name)==0) {
				const char *coupled_phandle_name = get_property_phandle_name(fdt, prop);
				UNIQUE_PRINTF("\"%s\" -> \"%s\" [style=dashed, label=\"coupled\"];\n", coupled_phandle_name, name);
			}
			if (strcmp("vin-supply", prop_name)==0) {
				const char *vin_supply_phandle_name = get_property_phandle_name(fdt, prop);
				UNIQUE_PRINTF("\"%s\" -> \"%s\" [style=bold, label=\"supply\"];\n", vin_supply_phandle_name, name);
			}

		}
	} else {
		const char *this_name =fdt_get_name(fdt, nodeoffset, &prop_len);
//		printf("# ignore %s\n", this_name);
		int prop_offset;
		fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
			const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
			const char *prop_name = fdt_get_string(fdt, fdt32_to_cpu(prop->nameoff), NULL);
//			printf("# ignore %s\n", prop_name);
			if (HAS_SUPPLY_SUFFIX_LEN(prop_name)==0) {
				const char *device_phandle_name = get_property_phandle_name(fdt, prop);
				UNIQUE_PRINTF("\"%s\" [shape=ellipse, fillcolor=lightblue, style=filled];\n", this_name);
				const char *phandle_name = get_property_phandle_name(fdt, prop);

				UNIQUE_PRINTF("\"%s\" -> \"%s\" [style=bold, label=\"supplies\"];\n", phandle_name, this_name);
			}
		}
	}
}

void walk_nodes(const void *fdt, int parent_offset) {
	int nodeoffset;
	const char *model = fdt_getprop(fdt, 0, "model", NULL);

	if (model) {
		UNIQUE_PRINTF("labelloc=\"t\";\nlabel=\"%s\";\n", model);
	} else {
	    UNIQUE_PRINTF("labelloc=\"t\";\nlabel=\"Unknown\";\n");
	}
	fdt_for_each_subnode(nodeoffset, fdt, parent_offset) {
		process_node(fdt, nodeoffset);
		walk_nodes(fdt, nodeoffset);
	}
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <dtb file>\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		perror("File open failed");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	void *fdt = malloc(size);
	fread(fdt, 1, size, f);
	fclose(f);

	if (fdt_check_header(fdt)) {
		fprintf(stderr, "Invalid FDT header\n");
		free(fdt);
		return 1;
	}

	printf("digraph regulators {\n");
	walk_nodes(fdt, 0);
	printf("}\n");
	free(fdt);
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fdt.h>
#include <stdarg.h>
#include <libfdt.h>
#include <openssl/md5.h>
#include "unique_printf.h"

#define UNUSED(x) (void)(x)
#define SUPPLY_SUFFIX "-supply"
#define SUPPLY_SUFFIX_LEN strlen(SUPPLY_SUFFIX)
#define HAS_SUPPLY_SUFFIX_LEN(x)  strcmp((strlen(x)-SUPPLY_SUFFIX_LEN)+x, SUPPLY_SUFFIX)

#define HELP_CMD_STR "Usage: %s <dtb file> (-hint)\n\t-h (--help)\tregulator shapes chart.\n\t-hint\t\tadds a legend for symbols shapes.\n"

#define ENTRY(type, shape) "\"" type "\" [shape=" shape ", fillcolor=lightblue, style=filled];\n"

#define SHAPE_CASES \
    ENTRY("regulator-fixed", "ellipse") \
    ENTRY("regulator-gpio", "box") \
    ENTRY("pwm-regulator", "octagon")

#define shape(x) ( \
    !strcmp(x, "regulator-fixed") ? "ellipse" : \
    !strcmp(x, "regulator-gpio") ? "box" : \
    !strcmp(x, "pwm-regulator") ? "octagon" : \
    "hexagon" \
)

const char* HELP_BODY_STR =
    "subgraph cluster_hint {\n"
    "\"User Device\" [shape=box, fillcolor=orange, style=filled];\n"
    "\"in-Device Regulator\" [shape=box, fillcolor=lightgreen, style=filled];\n"
    SHAPE_CASES
    "\"other type regulator\" [shape=hexagon, fillcolor=lightblue, style=filled];\n"
    "style=filled;\n"
    "color=cyan;\n"
    "label = \"Hints\";\n"
    "}\n"
    ;

const char *HELP_HEADER = "digraph G {\n";
const char *HELP_FOOTER = "}\n";

const char *NICER = "ranksep=1.5;\nnodesep=0.1;\nsplines=true;\n";

#define DEBUG

#ifdef DEBUG
#define PRINTDBG(...) printf(__VA_ARGS__)
#else
#define PRINTDBG(...)
#endif

bool want_hint = false;

char *sanitize(const char *input) {
	static char map[96] = {
		' ', '!', '\'', '#', '$', '%', '&', '\'', '(', ')', '*', '+', '_', '_', '.', '/',
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
		'_', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
		'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
		'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
		'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '_'
		};

	PRINTDBG("# sanitize: input='%s'\n", input);
	if (!input) return NULL;

	char *output = strdup(input);
	if (!output) return NULL;
	PRINTDBG("# sanitize: duplicated newptr=0x%08lx\n", (long)output);

	for (char *p = output; *p; ++p) {
		unsigned char c = (unsigned char)*p;
		if (c < 32 || c > 127) {
			*p = '_';
		} else {
			*p = map[c - 32];
		}
	}

	PRINTDBG("# sanitize: done\n");
	return output;
}

const char *is_regulator(const void *fdt, int nodeoffset) {
	int prop_offset;

	PRINTDBG("# is_regulator: start search for regulator-name\n");
	const char *name = fdt_getprop(fdt, nodeoffset, "regulator-name", NULL);
	if (name) {
		PRINTDBG("# is_regulator: found '%s' using regulator-name\n", name);
		return name;
	}

	PRINTDBG("# is_regulator: no luck, trying backup\n");
	fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
		const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
		if (!prop)
			continue;

		const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		PRINTDBG("# is_regulator: Property %s\n", name);
		size_t len = strlen(name);
		if (len >= 11 && strncmp(name, "regulator-m", 11) == 0){
			const char *tmp = fdt_get_name(fdt, nodeoffset, NULL);
			PRINTDBG("# is_regulator: found! Return '%s'\n", tmp);
			return tmp;
		}
	}
	PRINTDBG("# is_regulator: No Luck :(\n");
	return NULL;
}

const char *get_property_phandle_name(const void *fdt, const struct fdt_property *prop){
	uint32_t phandle_be;
	memcpy(&phandle_be, prop->data, sizeof(uint32_t));
	uint32_t phandle = fdt32_to_cpu(phandle_be);
	int phandle_offset = fdt_node_offset_by_phandle(fdt, phandle);
	const char *phandle_name = fdt_get_name(fdt, phandle_offset, NULL);
	return phandle_name;
}
const char *get_property_phandle_reg_name(const void *fdt, const struct fdt_property *prop){
	uint32_t phandle_be;
	memcpy(&phandle_be, prop->data, sizeof(uint32_t));
	uint32_t phandle = fdt32_to_cpu(phandle_be);
	int phandle_offset = fdt_node_offset_by_phandle(fdt, phandle);
	const char *regname = fdt_getprop(fdt, phandle_offset, "regulator-name", NULL);
	return regname;
}

char *remove_suffix(const char *c) {
	const char *suffix = "-supply";
	size_t suffix_len = strlen(suffix);

	if (!c) {
		return NULL;
	}

	size_t len = strlen(c);

	if (len >= suffix_len && strcmp(c + len - suffix_len, suffix) == 0) {
		char *result = (char *)malloc(len - suffix_len + 1);
		if (!result) {
			return NULL;
		}
		strncpy(result, c, len - suffix_len);
		result[len - suffix_len] = '\0';
		return result;
	}

	return NULL;
}

const char *find_upstream_source_seq(const void *fdt, int nodeoffset, int seqno) {
	int prop_offset;
	int len;
	const struct fdt_property *prop;
	int match_index = 0;

	PRINTDBG("# find_upstream_source_seq: l4=%d\n", seqno);
	fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
		PRINTDBG("# find_upstream_source_seq: iteration = %d\n", match_index);
		prop = fdt_get_property_by_offset(fdt, prop_offset, &len);
		if (!prop)
			continue;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 1\n", match_index);
		const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		if (!name)
			continue;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 2 (%s)\n", match_index, name);
		size_t name_len = strlen(name);
		if (name_len < 7 || strcmp(name + name_len - 7, "-supply") != 0)
			continue;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 3\n", match_index);
		if (match_index++ != seqno)
			continue;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 4\n", match_index);
		if (len < sizeof(fdt32_t))
			return NULL;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 5\n", match_index);
		int phandle = fdt32_to_cpu(*(const fdt32_t *)prop->data);
		if (!phandle)
			return NULL;

		PRINTDBG("# find_upstream_source_seq: iteration = %d checkpoint 6\n", match_index);
		int ref_offset = fdt_node_offset_by_phandle(fdt, phandle);
		if (ref_offset < 0)
			return NULL;

		PRINTDBG("# find_upstream_source_seq: iteration = %d - success\n", match_index);
		const char *regname = is_regulator(fdt, ref_offset);
//fdt_getprop(fdt, ref_offset, "regulator-name", &len);
//		if (!regname) {
//			PRINTDBG("# find_upstream_source_seq: container has not regulator-name, \n", match_index);
//			regname = fdt_get_name(fdt, nodeoffset, NULL);
//		}
		return regname;
	}

	return NULL;
}
const char *find_upstream_source(const void *fdt, int nodeoffset) {
	int prop_offset;
	int phandle = -1;
	int len;
	const struct fdt_property *prop;

	PRINTDBG("# find_upstream_source:start\n");
	fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
		PRINTDBG("# find_upstream_source: iteration start\n");
		prop = fdt_get_property_by_offset(fdt, prop_offset, &len);
		if (!prop)
			continue;

		const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		if (!name)
			continue;
		PRINTDBG("# find_upstream_source: property %s\n", name);
		size_t name_len = strlen(name);
		if (name_len < 7 || strcmp(name + name_len - 7, "-supply") != 0)
			continue;

		PRINTDBG("# find_upstream_source: property %s is a supply\n", name);

		if (len < sizeof(fdt32_t))
			continue;

		int current_phandle = fdt32_to_cpu(*(const fdt32_t *)prop->data);
		if (current_phandle == 0)
			continue;

		PRINTDBG("# find_upstream_source: property %s is referring to %d\n", name, phandle);
		if (phandle == -1) {
			phandle = current_phandle;
		} else if (phandle != current_phandle) {
			PRINTDBG("# find_upstream_source: pHANDLE MISMATCH\n");
			return NULL;
		}
	}

	if (phandle == -1)
		return NULL;

	int target_offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (target_offset < 0)
		return NULL;

	const char *regname = fdt_getprop(fdt, target_offset, "regulator-name", &len);
	return regname ? regname : NULL;
}

int next_supply_property_offset(const void *fdt, int nodeoffset, int last_offset) {
	int prop_offset;

	fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
		if (prop_offset <= last_offset)
			continue;

		const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
		if (!prop)
			continue;

		const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		size_t len = strlen(name);

		if (len >= 7 && strcmp(name + len - 7, "-supply") == 0) {
			return prop_offset;
		}
	}

	return -1;
}

void process_node(const void *fdt, int nodeoffset, int depth, int seq, int reg_in_branch);

void walk_nodes(const void *fdt) {
	char *model_s;
	const char *model = fdt_getprop(fdt, 0, "model", NULL);

	int root = fdt_path_offset(fdt, "/");
	if (root < 0) {
		fprintf(stderr, "Can't find root node.\n");
		return;
	}

	if (want_hint)
		printf("%s", HELP_BODY_STR);
	printf("%s", NICER);

	model_s = sanitize(model);
	if (model) {
			printf("labelloc=\"t\";\nlabel=\"%s\";\n", model_s);
	} else {
			printf("labelloc=\"t\";\nlabel=\"Unknown\";\n");
	}
	free(model_s);

	process_node(fdt, root, 0, 0, 0);
}

int resolve_phandle_from_property(const void *fdt, int nodeoffset, const char *propname) {
	int len;
	const fdt32_t *phandle_ptr = fdt_getprop(fdt, nodeoffset, propname, &len);

	if (!phandle_ptr || len < sizeof(fdt32_t)) {
		return -1;
	}

	uint32_t phandle = fdt32_to_cpu(*phandle_ptr);
	int target_offset = fdt_node_offset_by_phandle(fdt, phandle);

	return target_offset;
}

static int count_supply_properties(const void *fdt, int nodeoffset) {
	int count = 0;
	int prop_offset;

	fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
		const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
		if (!prop)
			continue;

		const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		size_t len = strlen(name);
		if (len >= 7 && strcmp(name + len - 7, "-supply") == 0)
			count++;
	}

	return count;
}

int get_parent_device_offs(const void *fdt, int nodeoffset) {
	int parent_offset = fdt_parent_offset(fdt, nodeoffset);
	int last_resort_offs = parent_offset;

	PRINTDBG("# get_parent_device_offs: start\n");
	for (int i = 0; i < 2 && parent_offset >= 0; ++i) {
		PRINTDBG("# get_parent_device_offs: Rule 1: Check for system-power-controller\n");
		if (fdt_get_property(fdt, parent_offset, "system-power-controller", NULL))
			return parent_offset;

		PRINTDBG("# get_parent_device_offs: Rule 2: Node name starts with pmic@\n");
		const char *name = fdt_get_name(fdt, parent_offset, NULL);
		if (name && strncmp(name, "pmic@", 5) == 0)
			return parent_offset;

		PRINTDBG("# get_parent_device_offs: Rule 3: Heuristic - has multiple *-supply properties\n");
		if (count_supply_properties(fdt, parent_offset) > 0)
			return parent_offset;

		PRINTDBG("# get_parent_device_offs: Presence of reg property\n");
		if (fdt_get_property(fdt, parent_offset, "reg", NULL))
			return parent_offset;

		PRINTDBG("# get_parent_device_offs: Climb up the tree\n");
		parent_offset = fdt_parent_offset(fdt, parent_offset);
	}

	PRINTDBG("# get_parent_device_offs: No Luck! just return the first parent offset.\n");
	return last_resort_offs;
}

const char *resolve_regulator_name(const void *fdt, int nodeoffset, const char *propname) {
	int len;

	PRINTDBG("# resolve_regulator_name: l4=%s\n", propname);
	const fdt32_t *phandle_ptr = fdt_getprop(fdt, nodeoffset, propname, &len);
	if (!phandle_ptr || len < sizeof(fdt32_t)) {
		PRINTDBG("# resolve_regulator_name: ERROR -> %s not found!\n", propname);
		return NULL;
	}

	uint32_t phandle = fdt32_to_cpu(*phandle_ptr);

	int target_offset = fdt_node_offset_by_phandle(fdt, phandle);
	if (target_offset < 0) {
		PRINTDBG("# resolve_regulator_name: ERROR -> cant solve phandle\n");
		return NULL;
	}

	const char *reg_name = fdt_getprop(fdt, target_offset, "regulator-name", NULL);
	return reg_name;
}

void process_node(const void *fdt, int nodeoffset, int depth, int seq, int reg_in_branch) {
	char path[256];
	int prop_offset;
	char *new_name;
	const struct fdt_property *node;
	const char *node_name;
	unsigned char digest[MD5_DIGEST_LENGTH];

	// explore first
	int child;
	fdt_for_each_subnode(child, fdt, nodeoffset) {
		process_node(fdt, child, depth + 1, seq, reg_in_branch);
	}

	if (!reg_in_branch) {
		fdt_get_path(fdt, nodeoffset, path, sizeof(path));
		PRINTDBG("# process_node: current node: %s (offset: %d) depth=%d\n", path, nodeoffset, depth);
		node_name = fdt_get_name(fdt, nodeoffset, NULL);

//		const char *regname = fdt_getprop(fdt, nodeoffset, "regulator-name", NULL);
		const char *regname = is_regulator(fdt, nodeoffset);
		if (regname) {
			PRINTDBG("# process_node: regname=0x%08lx is a regulator\n", (long)regname);
			reg_in_branch = 1;
			const char *comp = fdt_getprop(fdt, nodeoffset, "compatible", NULL);
			if (!comp) {
				if (depth>1) {
					int parent_offs = get_parent_device_offs(fdt, nodeoffset);

					const char *parent_name = fdt_get_name(fdt, parent_offs, NULL);
					char *tmp = sanitize(parent_name);
					PRINTDBG("# process_node: parent_name=0x%08lx, parent_offset=%d\n", (long)tmp, parent_offs);
//					printf("subgraph cluster_%s {\nstyle=filled;\ncolor=lightgrey;\nlabel = \"%s\";\n", tmp, parent_name);
					printf("subgraph cluster_%s {\nstyle=filled;\ncolor=lightgrey;\nlabel = \"%s\";\n", tmp, parent_name);
					printf("\"%s:Input\" [shape=diamond, fillcolor=lightgreen, style=filled];\n", parent_name);
					printf("\"%s\" [shape=box, fillcolor=lightgreen, style=filled];\n", regname);
					printf("}\n");
					MD5((const unsigned char*)parent_name, strlen(parent_name), digest);
					if (!is_md5_seen(digest)) {
						add_md5(digest);
					}
					int i =0;
					const char *source;
					while (source=find_upstream_source_seq(fdt, parent_offs, i++)) {
//						UNIQUE_PRINTF("\"%s\" -> \"%s\" [style=bold, label=\"supply\"];\n", source, regname);
						UNIQUE_PRINTF("\"%s\" -> \"%s:Input\" [style=bold, label=\"supply\"];\n", source, parent_name, parent_name);
					}
					free(tmp);

				}
			} else {
				printf("\"%s\" [shape=%s, fillcolor=lightblue, style=filled];\n", regname, shape(comp));
			}
			prop_offset = -1;
			while ((prop_offset = next_supply_property_offset(fdt, nodeoffset, prop_offset)) != -1) {
				char *s;
				const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
				const char *name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
				PRINTDBG("# process_node: Found supply: %s (offset %d)\n", name, prop_offset);
				const char *ref = get_property_phandle_reg_name(fdt, prop);

				PRINTDBG("# process_node: %s points to node: %s\n", name, ref);

				printf("\"%s\" -> \"%s\" [style=bold, label=\"supply\"];\n", ref, regname);
			}
		return;
		}
		PRINTDBG("# process_node: check if current is a device using a regulator\n");
		if (count_supply_properties(fdt, nodeoffset) > 0) {
			PRINTDBG("# process_node: %s: current is a device using a regulator, count_supply_properties = %d\n", node_name, count_supply_properties(fdt, nodeoffset));
			MD5((const unsigned char*)node_name, strlen(node_name), digest);
			if (!is_md5_seen(digest)) {
				char *tmp = sanitize(node_name);
				printf("subgraph cluster_%s {\nstyle=filled;\ncolor=lightgrey;\nlabel = \"%s\";\n", tmp, node_name);
				printf("\"%s\" [shape=box, fillcolor=orange, style=filled];\n", node_name);
				printf("}\n");
				free(tmp);
				int i =0;
				const char *source;
				while (source = find_upstream_source_seq(fdt, nodeoffset, i++)) {
					PRINTDBG("# process_node: emitting arch: %s -> %s\n", source, node_name);
					UNIQUE_PRINTF("\"%s\" -> \"%s\" [style=bold, label=\"supply\"];\n", source, node_name);
				}
			}
		}

	} // !reg_in_branch
}

int main(int argc, char *argv[]) {
	if ( (argc != 2) && (argc != 3) ) {
		fprintf(stderr, HELP_CMD_STR, argv[0]);
		return 1;
	}
	if ( (argc == 3) && (strcmp(argv[2], "-hint") != 0) ) {
		fprintf(stderr, HELP_CMD_STR, argv[0]);
		return 1;
	}
	if (argc == 3) want_hint = true;

	if ((strcmp(argv[1],"-h") == 0)||(strcmp(argv[1],"--help") == 0)) {
		printf("%s%s%s", HELP_HEADER, HELP_BODY_STR, HELP_FOOTER);
		return 0;
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
	size_t result = fread(fdt, 1, size, f);
	UNUSED(result);
	fclose(f);

	if (fdt_check_header(fdt)) {
		fprintf(stderr, "Invalid FDT header\n");
		free(fdt);
		return 1;
	}

	printf("digraph regulators {\n");
	walk_nodes(fdt);
	printf("}\n");
	free(fdt);
	return 0;
}


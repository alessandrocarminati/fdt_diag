#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fdt.h>
#include <libfdt.h>



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
		printf("\"%s\" [label=\"%s\", fillcolor=lightgrey];\n", name, reg_name);

		int prop_offset;
		fdt_for_each_property_offset(prop_offset, fdt, nodeoffset) {
			const struct fdt_property *prop = fdt_get_property_by_offset(fdt, prop_offset, NULL);
			const char *prop_name = fdt_get_string(fdt, fdt32_to_cpu(prop->nameoff), NULL);

			if (strcmp("regulator-coupled-with", prop_name)==0) {
				const char *coupled_phandle_name = get_property_phandle_name(fdt, prop);
				printf("\"%s\" -> \"%s\" [style=dashed, label=\"coupled\"];\n", coupled_phandle_name, name);
			}
			if (strcmp("vin-supply", prop_name)==0) {
				const char *vin_supply_phandle_name = get_property_phandle_name(fdt, prop);
				printf("\"%s\" -> \"%s\" [style=dashed, label=\"supply\"];\n", vin_supply_phandle_name, name);
			}

		}
	} else {
		const char *this_name =fdt_get_name(fdt, nodeoffset, &prop_len);
		printf("# ignore %s\n", this_name);
		
	}
}

//bool is_regulator(const void *fdt, int nodeoffset) {
	


void walk_nodes(const void *fdt, int parent_offset) {
	int nodeoffset;

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

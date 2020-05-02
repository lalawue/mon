//
// config.h
//
// Copyright (c) 2020 lalawue
//

#include "json_file.h"
#include <stdlib.h>
#include <string.h>

int
json_file_load(const char *file_path, json_file_t *jf) {

	if (file_path == NULL || jf == NULL) {
		fprintf(stderr, "Invalid file path\n");
        return 0;
	}

    memset(jf, 0, sizeof(*jf));

	if (stat(file_path, &jf->file_status) != 0) {
		fprintf(stderr, "File '%s' not found\n", file_path);
        goto fail_exit;
	}

	jf->fp = fopen(file_path, "rb");
	if (jf->fp == NULL) {
		fprintf(stderr, "Unable to open %s\n", file_path);
        goto fail_exit;
	}

	const int file_size = jf->file_status.st_size;
	jf->file_content = (char*)malloc(file_size);
	if (jf->file_content == NULL) {
		fprintf(stderr, "Memory error: unable to allocate %d bytes\n", file_size);
        goto fail_exit;
	}

	if (fread(jf->file_content, file_size, 1, jf->fp) != 1) {
		fprintf(stderr, "Unable to read content of %s\n", file_path);
        goto fail_exit;
	}

	jf->json = json_parse((json_char*)jf->file_content, file_size);
    if (jf->json == NULL) {
        fprintf(stderr, "Unable to parse content\n");        
        goto fail_exit;
	}

    return 1;

    fail_exit:
    if (jf->file_content) {
		free(jf->file_content);
    }
    if (jf->fp) {
        fclose(jf->fp);
    }
    return 0;
}

void
json_file_destroy(json_file_t *jf) {
    if (jf) {
        if (jf->file_content) {
            free(jf->file_content);
        }
        if (jf->fp) {
            fclose(jf->fp);
        }
        if (jf->json) {
            json_value_free(jf->json);
        }
    }
}
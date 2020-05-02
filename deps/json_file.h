//
// config.h
//
// Copyright (c) 2020 lalawue
//

#ifndef _JSON_FILE
#define _JSON_FILE

#include <stdio.h>
#include <sys/stat.h>
#include "json.h"

typedef struct {
    FILE *fp;
	struct stat file_status;    
    char *file_content;
    json_value *json;
} json_file_t;

int json_file_load(const char *file_path, json_file_t *jf);

void json_file_destroy(json_file_t *jf);

#endif
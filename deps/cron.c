//
// config.h
//
// Copyright (c) 2020 lalawue
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "cron.h"

typedef enum {
	T_UNKNOW = 0,	// other char
	T_END	 = 1,	// '\0'
	T_BLANK	 = 2,	// [space|tab]
	T_DIGIT  = 3,	// numer
	T_COMMA  = 4,	// ,
	T_LINE   = 5,	// -
	T_STAR   = 6,	// *
	T_SLASH  = 7,	// /
} TOKEN_T;

// return next token, skip same, move current index to next-position
static TOKEN_T
_next_token(const char *buf, int *is, const int ie, char *number) {
	if (*is > ie) {
		return T_END;
	}
	if (buf[*is] == ' ' || buf[*is] == '\t') {
		while ((buf[*is] == ' ' || buf[*is] == '\t') && *is <= ie) {
			*is += 1;
		}
		return T_BLANK;
	}
	if (buf[*is] >= '0' && buf[*is] <= '9') {
		*number = 0;
		while (buf[*is] >= '0' && buf[*is] <= '9' && *is <= ie) {
			*number *= 10;
			*number += buf[*is] - '0';
			*is += 1;
		}
		return T_DIGIT;
	}
	if (buf[*is] == ',') {
		while (buf[*is] == ',' && *is <= ie) {
			*is += 1;
		}
		return T_COMMA;
	}
	if (buf[*is] == '-') {
		while (buf[*is] == '-' && *is <= ie) {
			*is += 1;
		}
		return T_LINE;
	}
	if (buf[*is] == '*') {
		while (buf[*is] == '*' && *is <= ie) {
			*is += 1;
		}
		return T_STAR;
	}
	if (buf[*is] == '/') {
		while (buf[*is] == '/' && *is <= ie) {
			*is += 1;
		}
		return T_SLASH;
	}
	return T_UNKNOW;
}

// value for special number
static const int kUnknow = -2;
static const int kStar = -1; // *

//#define _LOG(...) printf(__VA_ARGS__) // debug
#define _LOG(...)

// if nend non-exist, set to kUnknow
static void
_fill_bits(char nstart, char nend, unsigned char *pbits, int bytes, char nmin, char nmax) {

	if (nstart == kStar) {
		// *
		if (nend == kUnknow) {
			memset(pbits, 0xff, bytes);
		}
		// */5
		if (nend > 0) {
			for (int i=0; nmin+i<=nmax; i++) {
				char nnum = i % nend;
				if (nnum == 0) {
					pbits[i/8] |= 1 << (i % 8);
				}
			}
		}
	}

	// 3
	if (nstart >= 0 && nend == kUnknow) {
		if (nstart >= nmin && nstart <= nmax) {
			pbits[nstart/8] |= 1 << (nstart % 8) ;
		}
	}

	// 3-5
	if (nstart >= 0 && nend >= 0 && nstart != nend) {
		if (nstart >= nmin &&
			nstart <= nmax &&
			nend >= nmin &&
			nend <= nmax)
		{
			for (int i=nstart; i<=nend; i++) {
				pbits[i/8] |= 1 << (i % 8);
			}
		}
	}
}

// 1. T_START, T_DIGIT is valid single token, update last_token
// 2. T_COMMA, T_LINE, T_SLASH is conjunction token, keep last_token 
// 3. T_UNKONW, T_BLANK, T_END is finite state
// 4. return finite state, continue keep last_token
static bool
_parse_bits(const char *buf, int *is, int ie, unsigned char *pbits, int bytes, char nmin, char nmax, const char *tag) {	
	TOKEN_T token = T_UNKNOW, last_token = T_UNKNOW;
	char nstart = kUnknow; // number before conjunction token
	char nend; // number after conjunction token
	char nnumber; // temp number
	//_LOG("buf %s, %d, %d\n", buf, *is, ie);
	while ((token = _next_token(buf, is, ie, &nnumber))) {
		_LOG("%s: token %d, %d\n", tag, token, nnumber);
		if (token == T_UNKNOW) {
			return false;
		}
		if (token == T_END) {
			bool is_valid = (nstart != kUnknow);
			if (is_valid && (last_token == T_DIGIT || last_token == T_STAR)) {
				_LOG("%s: end number = %d\n", tag, nstart);
				_fill_bits(nstart, kUnknow, pbits, bytes, nmin, nmax);
			}
			return is_valid;
		}
		if (token == T_BLANK) {
			if (last_token == T_UNKNOW) {
				// trim prefix blank
				continue;
			} else {
				bool is_valid = (nstart != kUnknow);
				if (is_valid && (last_token == T_DIGIT || last_token == T_STAR)) {
					_LOG("%s: blank number = %d\n", tag, nstart);
					_fill_bits(nstart, kUnknow, pbits, bytes, nmin, nmax);
				}
				return is_valid;
			}
		}
		if (token == T_STAR) {
			//_LOG("%s: star\n", tag);
			nstart = kStar;
		}
		if (token == T_DIGIT) {
			if (last_token == T_LINE) {
				nend = nnumber;
				_LOG("%s: from %d - %d\n", tag, nstart, nend);
				_fill_bits(nstart, nend, pbits, bytes, nmin, nmax);
				continue;
			} else if (last_token == T_SLASH) {
				nend = nnumber;
				_LOG("%s: slash */%d\n", tag, nend);
				_fill_bits(kStar, nend, pbits, bytes, nmin, nmax);
				continue;
			} else {
				nstart = nnumber;
			}
		}
		if (token == T_COMMA) {
			if (last_token == T_DIGIT) {
				_LOG("%s: number %d\n", tag, nstart);
				_fill_bits(nstart, kUnknow, pbits, bytes, nmin, nmax);
			}
		}
		last_token = token;
	}
	return false;
}

cron_t* 
cron_create(const char *entry, int entry_len)
{
	if (!entry) {
		fprintf(stderr, "Empty cron entry.\n");
		return NULL;
	}

	const char *s = entry;
	const char *e = entry + entry_len - 1;
	while ((*s == ' ' || *s == '\t') && s < e) { s++; }
	while ((*e == ' ' || *s == '\t') && e > s) { e--; }

	int len = e - s + 1;
	if (len < 9) {
		fprintf(stderr, "Need more cron desc.\n");
		return NULL;
	}

	cron_t *cron = malloc(sizeof(cron_t));
	memset(cron, 0, sizeof(cron_t));

	int is = 0;

	if (!_parse_bits(s, &is, e - s, cron->min, sizeof(cron->min), 0, 59, "minutes")) {
		fprintf(stderr, "Invalid cron minutes.\n");
		goto error;
	}

	if (!_parse_bits(s, &is, e - s, cron->hour, sizeof(cron->hour), 0, 23, "hours")) {
		fprintf(stderr, "Invalid cron hours.\n");
		goto error;
	}

	if (!_parse_bits(s, &is, e - s, cron->mdays, sizeof(cron->mdays), 1, 31, "mday")) {
		fprintf(stderr, "Invalid cron month day.\n");
		goto error;
	}

	if (!_parse_bits(s, &is, e - s, cron->mon, sizeof(cron->mon), 0, 11, "month")) {
		fprintf(stderr, "Invalid cron month.\n");
		goto error;
	}

	if (!_parse_bits(s, &is, e - s, cron->wdays, sizeof(cron->wdays), 0, 6, "wday")) {
		fprintf(stderr, "Invalid cron week day.\n");
		goto error;
	}

	return cron;

error:
	if (cron) {
		free(cron);
	}
	return NULL;
}

void
cron_destroy(cron_t *cron) {
	if (cron) {
		free(cron);
	}
}

static inline bool
_is_bit_true(unsigned char *pbits, int bytes, int bits_index) {
	int index = bits_index / 8;
	if (index < bytes) {
		return pbits[index] & (1 << (bits_index % 8));
	}
	return false;
}

bool
cron_in_timearea(cron_t *cron, struct tm *ptm) {
	if (cron && ptm) {
		bool in_min = _is_bit_true(cron->min, sizeof(cron->min), ptm->tm_min);
		bool in_hour = _is_bit_true(cron->hour, sizeof(cron->hour), ptm->tm_hour);
		bool in_mday = _is_bit_true(cron->mdays, sizeof(cron->mdays), ptm->tm_mday - 1);
		bool in_mon = _is_bit_true(cron->mon, sizeof(cron->mon), ptm->tm_mon);
		bool in_wday = _is_bit_true(cron->wdays, sizeof(cron->wdays), ptm->tm_wday % 7);
		return in_min && in_hour && in_mday && in_mon && in_wday;
	}
	return false;
}

void
cron_set_has_running(cron_t *cron, bool has_running) {
	if (cron) {
		cron->has_running = has_running;
	}
}

#ifdef CRON_TEST
#include <assert.h>
void
_dump_bits(unsigned char *pbits, int bytes) {
	int i = 0;
	for (; i<bytes; i++) {
		printf("%x ", pbits[i]);
		if (i && i % 10 == 0) {
			printf("\n");
		}
	}
	if (i % 10 != 0) {
		printf("\n");
	}
}
bool
cron_should_invoke(cron_t *cron, struct tm *ptm) {
	if (cron && ptm) {				
		return !cron->has_running && cron_in_timearea(cron, ptm);
	}
	return true;
}
#define _TM(T, A, B, C, D, E) struct tm T = { \
			.tm_min = A,	\
			.tm_hour = B,	\
			.tm_mday = C,	\
			.tm_mon = D,	\
			.tm_wday = E	\
		}
// gcc cron.c -DCRON_TEST
int main(int arg, char *argv[]) {
	//"2,9-15,22 * */5 3,9,18 *"
	//"2,,,9---15, 22 * 3,9,10  */5 *"
	char *str_cron = NULL; //cron_create(argv[1], strlen(argv[1]));
	cron_t *cron = NULL;
	//_dump_bits(cron->min, sizeof(cron->min));
	{
		str_cron = "* 1,8,12 * * *";
		cron = cron_create(str_cron, strlen(str_cron));
		_TM(t, 0, 1, 1, 0, 0);
		cron->has_running = false;
		assert( cron_should_invoke(cron, &t) );
		_TM(t1, 0, 8, 1, 0, 0);
		cron->has_running = false;		
		assert( cron_should_invoke(cron, &t1) );
		_TM(t2, 0, 12, 1, 0, 0);
		cron->has_running = false;		
		assert( cron_should_invoke(cron, &t2) );
		_TM(t3, 0, 11, 1, 0, 0);
		cron->has_running = false;
		assert( !cron_should_invoke(cron, &t3) );
		cron_destroy(cron);
	}
	{
		str_cron = "* */2 */4 11 *";
		cron = cron_create(str_cron, strlen(str_cron));
		_TM(t, 0, 2, 5, 11, 0);
		cron->has_running = false;
		assert( cron_should_invoke(cron, &t) );
		_TM(t1, 0, 6, 9, 11, 0);
		cron->has_running = false;
		assert( cron_should_invoke(cron, &t1) );
		_TM(t2, 0, 6, 6, 11, 0);
		cron->has_running = false;
		assert( !cron_should_invoke(cron, &t2) );
		cron_destroy(cron);
	}
	{
		str_cron = "* 2-4 * * *";
		cron = cron_create(str_cron, strlen(str_cron));
		_TM(t, 0, 1, 1, 0, 0);
		cron->has_running = false;
		assert( !cron_should_invoke(cron, &t) );
		_TM(t1, 0, 2, 1, 0, 0);
		cron->has_running = false;
		assert( cron_should_invoke(cron, &t1) );
		cron_destroy(cron);
	}
	{
		str_cron = "2-4,6-8 * * * *";
		cron = cron_create(str_cron, strlen(str_cron));
		_TM(t, 0, 1, 1, 0, 0);
		cron->has_running = false;
		printf("cron: %s\n", str_cron);
		for (int i=0; i<10; i++) {
			t.tm_min = i;
			printf("min %d should invoke\n", cron_should_invoke(cron, &t));
		}
		cron_destroy(cron);
	}
	return 0;
}
#endif
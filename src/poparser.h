#ifndef POPARSER_H
#define POPARSER_H

#include <iconv.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_NPLURALS 6

enum sysdep_types {
	st_priu32 = 0,
	st_priu64,
	st_priumax,
	st_max
};

// make sure out has equal or more space than in
// this add the NULL terminator, but do not count it in size
size_t poparser_sysdep(const char *in, char *out, int num);

struct po_header {
	char charset[12];
	unsigned nplurals;
	// maybe parse the header later
};

#define PO_FUZZY 1u

struct po_message {
	char *ctxt;
	char *id;
	char *plural;
	char* str[MAX_NPLURALS];

	int sysdep;
	size_t ctxt_len;
	size_t id_len;
	size_t plural_len;
	size_t strlen[MAX_NPLURALS];
	// h.......1.0 
	// |-------|a| 
	// |.......|a| 
	int flags;
};
typedef struct po_message *po_message_t;

typedef int (*poparser_callback)(po_message_t msg, void* user);

enum po_stage {
	// collect size of every msg
	ps_size = 0,
	// parse 
	ps_parse,
	ps_max = ps_parse,
};

enum po_entry {
	po_ctxt = 0,
	po_id,
	po_plural,
	po_str,
};

struct po_parser {
	struct po_header hdr;
	struct po_message msg;
	enum po_stage stage;

	// private parts
	bool first;
	bool strict;
	iconv_t cd;
	enum po_entry previous;
	unsigned strcnt;
	size_t max_ctxt_len;
	size_t max_id_len;
	size_t max_plural_len;
	size_t max_strlen[MAX_NPLURALS];
	char *buf;
	size_t bufsize;
	poparser_callback cb;
	void *cbdata;
};

enum po_error {
	po_success = 0,
	po_unsupported_charset,
	po_failed_iconv,
	po_excepted_token,
	po_plurals_overflow,
	po_invalid_entry,
	po_internal,
	po_error_last = po_internal,
};

void poparser_init(struct po_parser *p, char* workbuf, size_t bufsize, poparser_callback cb, void* cbdata);
enum po_error poparser_feed_line(struct po_parser *p, char* line, size_t buflen);
enum po_error poparser_finish(struct po_parser *p);

#endif

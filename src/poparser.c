#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "poparser.h"

#define streq(A, B) (!strcmp(A, B))
#define strstarts(S, W) (memcmp(S, W, sizeof(W) - 1) ? NULL : (S + (sizeof(W) - 1)))

static size_t convertbuf(char* in, char *out) {
	size_t l = 0;
	while(*in) {
		switch (*in) {
			case '\\':
				++in;
				assert(*in);
				switch(*in) {
					case 'n':
						*out='\n';
						break;
					case 'r':
						*out='\r';
						break;
					case 't':
						*out='\t';
						break;
					case '\\':
						*out='\\';
						break;
					case '"':
						*out='"';
						break;
					case 'v':
						*out='\v';
						break;
					case '\?':
						*out = '\?';
						break;
					case 'f':
						*out = '\f';
						break;
					case '\'':
						*out = '\'';
						break;
					// FIXME add handling of hex and octal
					default:
						abort();
				}
				break;
			default:
				*out=*in;
		}
		in++;
		out++;
		l++;
	}
	*out = 0;
	return l;
}

static enum po_entry get_type_and_start(char* lp, char* end, size_t *stringstart) {
	enum po_entry result_type;
	char *x, *y;
	size_t start = (size_t) lp;
	while(isspace(*lp) && lp < end) lp++;
	if(lp[0] == '#') {
		inv:
		*stringstart = 0;
		return pe_invalid;
	}
	if((y = strstarts(lp, "msg"))) {
		if((x = strstarts(y, "id")) && (isspace(*x) || ((x = strstarts(x, "_plural")) && isspace(*x))))
			result_type = pe_msgid;
		else if ((x = strstarts(y, "str")) && (isspace(*x) || 
			(x[0] == '[' && (x[1] == '0' || x[1] == '1') && x[2] == ']' && (x += 3) && isspace(*x))))
			result_type = pe_msgstr;
		else 
			goto inv;
		while(isspace(*x) && x < end) x++;
		if(*x != '"') abort();
		conv:
		*stringstart = ((size_t) x - start) + 1;
	} else if(*lp == '"') {
		result_type = pe_str;
		x = lp;
		goto conv;
	} else {
		goto inv;
	}
	return result_type;
}

/* expects a pointer to the first char after a opening " in a string, 
 * converts the string into convbuf, and returns the length of that string */
static size_t get_length_and_convert(char* x, char* end, char* convbuf) {
	size_t result = 0;
	char* e = x + strlen(x);
	assert(e > x && e < end && *e == 0);
	e--;
	while(isspace(*e)) e--;
	if(*e != '"') abort();
	*e = 0;
	result = convertbuf(x, convbuf);
	return result;
}


void poparser_init(struct po_parser *p, char* workbuf, size_t bufsize, poparser_callback cb, void* cbdata) {
	p->buf = workbuf;
	p->bufsize = bufsize;
	p->cb = cb;
	p->prev_type = pe_invalid;
	p->curr_len = 0;
	p->cbdata = cbdata;
}

enum lineactions {
	la_incr,
	la_proc,
	la_abort,
	la_nop,
	la_max,
};

/* return 0 on success */
int poparser_feed_line(struct po_parser *p, char* line, size_t buflen) {
	char *lp = line;
	char *convbuf = p->buf;
	size_t strstart;
	
	static const enum lineactions action_tbl[pe_max][pe_max] = {
		// pe_str will never be set as curr_type
		[pe_str] = { 
			[pe_str] = la_abort,
			[pe_msgid] = la_abort,
			[pe_msgstr] = la_abort,
			[pe_invalid] = la_abort, 
		},
		[pe_msgid] = { 
			[pe_str] = la_incr,
			[pe_msgid] = la_proc,
			[pe_msgstr] = la_proc,
			[pe_invalid] = la_proc, 
		},
		[pe_msgstr] = { 
			[pe_str] = la_incr,
			[pe_msgid] = la_proc,
			[pe_msgstr] = la_proc,
			[pe_invalid] = la_proc, 
		},
		[pe_invalid] = { 
			[pe_str] = la_nop, // this can happen when we have msgstr[2] "" ... "foo", since we only parse msgstr[0] and [1]
			[pe_msgid] = la_incr,
			[pe_msgstr] = la_incr,
			[pe_invalid] = la_nop, 
		},
	};
	
	enum po_entry type;
	
	type = get_type_and_start(lp, line + buflen, &strstart);
	switch(action_tbl[p->prev_type][type]) {
		case la_incr:
			assert(type == pe_msgid || type == pe_msgstr || type == pe_str);
			p->curr_len += get_length_and_convert(lp + strstart, line + buflen - p->curr_len, convbuf + p->curr_len);
			break;
		case la_proc:
			assert(p->prev_type == pe_msgid || p->prev_type == pe_msgstr);
			p->info.text = convbuf;
			p->info.textlen = p->curr_len;
			p->info.type = p->prev_type;
			p->cb(&p->info, p->cbdata);
			if(type != pe_invalid)
				p->curr_len = get_length_and_convert(lp + strstart, line + buflen, convbuf);
			else 
				p->curr_len = 0;
			break;
		case la_nop:
			break;
		case la_abort:
		default:
			abort();
			// todo : return error code
	}
	if(type != pe_str) {
		p->prev_type = type;
	}
	return 0;
}

int poparser_finish(struct po_parser *p) {
	char empty[4] = "";
	return poparser_feed_line(p, empty, sizeof(empty));
}

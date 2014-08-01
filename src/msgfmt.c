/* msgfmt utility (C) 2012 rofl0r
 * released under the MIT license, see LICENSE for details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "poparser.h"

// in DO_NOTHING mode, we simply write the msgid twice, once for msgid, once for msgstr.
// TODO: maybe make it write "" instead of echoing the msgid.
//#define DO_NOTHING

__attribute__((noreturn))
static void syntax(void) {
	fprintf(stdout,
	"Usage: msgfmt [OPTION] filename.po ...\n");
	exit(1);
}

__attribute__((noreturn))
static void version(void) {
	fprintf(stdout,
		"these are not (GNU gettext-tools) 99.9999.9999\n");
	exit(0);
}

#define streq(A, B) (!strcmp(A, B))
#define strstarts(S, W) (memcmp(S, W, sizeof(W) - 1) ? NULL : (S + (sizeof(W) - 1)))

struct mo_hdr {
	unsigned magic;
	int rev;
	unsigned numstring;
	unsigned off_tbl_org;
	unsigned off_tbl_trans;
	unsigned hash_tbl_size;
	unsigned off_tbl_hash;
};

/* file layout:
	header
	strtable (lenghts/offsets)
	transtable (lenghts/offsets)
	[hashtable]
	strings section
	translations section */

const struct mo_hdr def_hdr = {
	0x950412de,
	0,
	0,
	sizeof(struct mo_hdr),
	0,
	0,
	0,
};


// pass 0: collect numbers of strings, calculate size and offsets for tables
// print header
// pass 1: create in-memory string tables
enum passes {
	pass_first = 0,
	pass_collect_sizes = pass_first,
	pass_second,
	pass_max,
};

struct strtbl {
	unsigned len, off;
};

struct strmap {
	struct strtbl str, *trans;
};

struct callbackdata {
	enum passes pass;
	unsigned off;
	FILE* out;
	unsigned num[pe_maxstr];
	unsigned len[pe_maxstr];
	struct strmap *strlist;
	struct strtbl *translist;
	char *strbuffer[pe_maxstr];
	unsigned stroff[pe_maxstr];
	unsigned curr[pe_maxstr];
};

static struct callbackdata *cb_for_qsort;
int strmap_comp(const void *a_, const void *b_) {
	const struct strmap *a = a_, *b = b_;
	return strcmp(cb_for_qsort->strbuffer[0] + a->str.off, cb_for_qsort->strbuffer[0] + b->str.off);
}


int process_line_callback(struct po_info* info, void* user) {
	struct callbackdata *d = (struct callbackdata *) user;
	assert(info->type == pe_msgid || info->type == pe_msgstr);
	switch(d->pass) {
		case pass_collect_sizes:
			d->num[info->type] += 1;
			d->len[info->type] += info->textlen +1;
			break;
		case pass_second:
			memcpy(d->strbuffer[info->type] + d->stroff[info->type], info->text, info->textlen+1);
			if(info->type == pe_msgid)
				d->strlist[d->curr[info->type]].str = (struct strtbl){.len=info->textlen, .off=d->stroff[info->type]};
			else {
				assert(d->curr[pe_msgid] == d->curr[pe_msgstr]+1);
				d->translist[d->curr[info->type]] = (struct strtbl){.len=info->textlen, .off=d->stroff[info->type]};
				d->strlist[d->curr[info->type]].trans = &d->translist[d->curr[info->type]];
			}
			d->curr[info->type]++;
			d->stroff[info->type]+=info->textlen+1;
			break;
		default:
			abort();
	}
	return 0;
}

int process(FILE *in, FILE *out) {
	struct mo_hdr mohdr = def_hdr;
	char line[4096]; char *lp;
	char convbuf[16384];

	struct callbackdata d = {
		.num = {
			[pe_msgid] = 0,
			[pe_msgstr] = 0,
		},
		.len = {
			[pe_msgid] = 0,
			[pe_msgstr] = 0,
		},
		.off = 0,
		.out = out,
		.pass = pass_first,
	};

	struct po_parser pb, *p = &pb;
	int invalid_file = 0;

	mohdr.off_tbl_trans = mohdr.off_tbl_org;
	for(d.pass = pass_first; d.pass <= pass_second; d.pass++) {
		if(d.pass == pass_second) {
			// start of second pass:
			// check that data gathered in first pass is consistent
			if(d.num[pe_msgid] != d.num[pe_msgstr]) {
				// one should actually abort here,
				// but gnu gettext simply writes an empty .mo and returns success.
				//abort();
				d.num[pe_msgid] = 0;
				invalid_file = 1;
			}

			// calculate header fields from len and num arrays
			mohdr.numstring = d.num[pe_msgid];
			mohdr.off_tbl_org = sizeof(struct mo_hdr);
			mohdr.off_tbl_trans = mohdr.off_tbl_org + d.num[pe_msgid] * (sizeof(unsigned)*2);
			// print header
			fwrite(&mohdr, sizeof(mohdr), 1, out);
			// set offset startvalue
			d.off = mohdr.off_tbl_trans + d.num[pe_msgid] * (sizeof(unsigned)*2);
			if(invalid_file) return 0;

			d.strlist = malloc(d.num[pe_msgid] * sizeof(struct strmap));
			d.translist = malloc(d.num[pe_msgstr] * sizeof(struct strtbl));
			d.strbuffer[pe_msgid] = malloc(d.len[pe_msgid]);
			d.strbuffer[pe_msgstr] = malloc(d.len[pe_msgstr]);
			d.stroff[pe_msgid] = d.stroff[pe_msgstr] = 0;
			assert(d.strlist && d.translist && d.strbuffer[0] && d.strbuffer[1]);
		}
		poparser_init(p, convbuf, sizeof(convbuf), process_line_callback, &d);

		while((lp = fgets(line, sizeof(line), in))) {
			poparser_feed_line(p, lp, sizeof(line));
		}

		poparser_finish(p);

		fseek(in, 0, SEEK_SET);
	}

	cb_for_qsort = &d;
	qsort(d.strlist, d.num[pe_msgid], sizeof (struct strmap), strmap_comp);
	unsigned i;
	for(i = 0; i < d.num[0]; i++) {
		d.strlist[i].str.off += d.off;
		fwrite(&d.strlist[i].str, sizeof(struct strtbl), 1, d.out);
	}
	for(i = 0; i < d.num[1]; i++) {
		d.strlist[i].trans->off += d.off + d.len[0];
		fwrite(d.strlist[i].trans, sizeof(struct strtbl), 1, d.out);
	}
	fwrite(d.strbuffer[0], d.len[0], 1, d.out);
	fwrite(d.strbuffer[1], d.len[1], 1, d.out);

	return 0;
}


void set_file(int out, char* fn, FILE** dest) {
	if(streq(fn, "-")) {
		*dest = out ? stdout : stdin;
	} else {
		*dest = fopen(fn, out ? "w" : "r");
	}
	if(!*dest) {
		perror("fopen");
		exit(1);
	}
}

int main(int argc, char**argv) {
	if(argc == 1) syntax();
	int arg = 1;
	FILE *out = NULL;
	FILE *in = NULL;
	int expect_out_fn = 0;
	int expect_in_fn = 1;
	char* dest;
#define A argv[arg]
	for(; arg < argc; arg++) {
		if(expect_out_fn) {
			set_file(1, A, &out);
			expect_out_fn = 0;
		} else if(A[0] == '-') {
			if(A[1] == '-') {
				if(
					streq(A+2, "java") ||
					streq(A+2, "java2") ||
					streq(A+2, "csharp") ||
					streq(A+2, "csharp-resources") ||
					streq(A+2, "tcl") ||
					streq(A+2, "qt") ||
					streq(A+2, "strict") ||
					streq(A+2, "properties-input") ||
					streq(A+2, "stringtable-input") ||
					streq(A+2, "use-fuzzy") ||
					strstarts(A+2, "alignment=") ||
					streq(A+2, "check") ||
					streq(A+2, "check-format") ||
					streq(A+2, "check-header") ||
					streq(A+2, "check-domain") ||
					streq(A+2, "check-compatibility") ||
					streq(A+2, "check-accelerators") ||
					streq(A+2, "no-hash") ||
					streq(A+2, "verbose") ||
					streq(A+2, "statistics") ||
					strstarts(A+2, "check-accelerators=") ||
					strstarts(A+2, "resource=") ||
					strstarts(A+2, "locale=")

				) {
				} else if((dest = strstarts(A+2, "output-file="))) {
					set_file(1, dest, &out);
				} else if(streq(A+2, "version")) {
					version();
				} else if(streq(A+2, "help")) syntax();

			} else if(streq(A + 1, "o")) {
				expect_out_fn = 1;
			} else if(
				streq(A+1, "j") ||
				streq(A+1, "r") ||
				streq(A+1, "l") ||
				streq(A+1, "P") ||
				streq(A+1, "f") ||
				streq(A+1, "a") ||
				streq(A+1, "c") ||
				streq(A+1, "C")
			) {
			} else if (streq(A+1, "v")) {
				version();
			} else if (streq(A+1, "d")) {
				// no support for -d at this time
				fprintf(stderr, "EINVAL\n");
				exit(1);
			} else if (streq(A+1, "h")) syntax();
		} else if (expect_in_fn) {
			set_file(0, A, &in);
		}
	}
	if(in == NULL || out == NULL) syntax();
	int ret = process(in, out);
	fflush(in); fflush(out);
	if(in != stdin) fclose(in);
	if(out != stdout) fclose(out);
	return ret;
}

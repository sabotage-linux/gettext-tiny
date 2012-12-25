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
// pass 1: print string table [lengths/offsets]
// pass 2: print translation table [lengths/offsets]
// pass 3: print strings
// pass 4: print translations
enum passes {
	pass_first = 0,
	pass_collect_sizes = pass_first,
	pass_second,
	pass_print_string_offsets = pass_second,
	pass_print_translation_offsets,
	pass_print_strings,
	pass_print_translations,
	pass_max,
};

struct callbackdata {
	enum passes pass;
	unsigned off;
	FILE* out;
	unsigned num[pe_maxstr];
	unsigned len[pe_maxstr];
};


int process_line_callback(struct po_info* info, void* user) {
	struct callbackdata *d = (struct callbackdata *) user;
	assert(info->type == pe_msgid || info->type == pe_msgstr);
	switch(d->pass) {
		case pass_collect_sizes:
			d->num[info->type] += 1;
			d->len[info->type] += info->textlen;
			break;
		case pass_print_string_offsets:
			if(info->type == pe_msgstr) break;
			write_offsets:
			// print length of current string
			fwrite(&info->textlen, sizeof(unsigned), 1, d->out);
			// print offset of current string
			fwrite(&d->off, sizeof(unsigned), 1, d->out);
			d->off += info->textlen + 1;
			break;
		case pass_print_translation_offsets:
#ifndef DO_NOTHING
			if(info->type == pe_msgid) break;
#else
			if(info->type != pe_msgid) break;
#endif
			goto write_offsets;
		case pass_print_strings:
			if(info->type == pe_msgstr) break;
			write_string:
			fwrite(info->text, info->textlen + 1, 1, d->out);
			break;
		case pass_print_translations:
#ifndef DO_NOTHING
			if(info->type == pe_msgid) break;
#else
			if(info->type != pe_msgid) break;
#endif
			goto write_string;
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
	for(d.pass = pass_first; d.pass < pass_max; d.pass++) {
		if(d.pass == pass_second) {
			// start of second pass:
			// check that data gathered in first pass is consistent
#ifndef DO_NOTHING
			if(d.num[pe_msgid] != d.num[pe_msgstr]) {
				// one should actually abort here, 
				// but gnu gettext simply writes an empty .mo and returns success.
				//abort();
				d.num[pe_msgid] = 0;
				invalid_file = 1;
			}
#endif
			
			// calculate header fields from len and num arrays
			mohdr.numstring = d.num[pe_msgid];
			mohdr.off_tbl_org = sizeof(struct mo_hdr);
			mohdr.off_tbl_trans = mohdr.off_tbl_org + d.num[pe_msgid] * (sizeof(unsigned)*2);
			// print header
			fwrite(&mohdr, sizeof(mohdr), 1, out);				
			// set offset startvalue
			d.off = mohdr.off_tbl_trans + d.num[pe_msgid] * (sizeof(unsigned)*2);
			if(invalid_file) return 0;
		}
		poparser_init(p, convbuf, sizeof(convbuf), process_line_callback, &d);
		
		while((lp = fgets(line, sizeof(line), in))) {
			poparser_feed_line(p, lp, sizeof(line));
		}
		
		poparser_finish(p);
		
		fseek(in, 0, SEEK_SET);
	}
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

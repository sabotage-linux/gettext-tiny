/* msgfmt utility (C) 2012 rofl0r
 * released under the MIT license, see LICENSE for details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/* TODO: handle multiline strings
msgid ""
"next line\n"
"and another line"
*/

void syntax(void) {
	fprintf(stdout,
	"Usage: msgfmt [OPTION] filename.po ...\n");
	exit(1);
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
};

size_t convertbuf(char* in, char *out) {
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

enum plr_type {
	plr_msgid = 0,
	plr_msgstr,
	plr_invalid,
	plr_max = plr_invalid,
};

struct plr {
	unsigned len;
	enum plr_type type;
};

void process_line(char *lp, char* end, char* convbuf, struct plr *result) {
	char *x, *y, *e;
	while(isspace(*lp) && lp < end) lp++;
	if(lp[0] == '#') goto inv;
	if((y = strstarts(lp, "msg"))) {
		if((x = strstarts(y, "id")) && (isspace(*x) || ((x = strstarts(x, "_plural")) && isspace(*x))))
			result->type = plr_msgid;
		else if ((x = strstarts(y, "str")) && (isspace(*x) || 
			(x[0] == '[' && (x[1] == '0' || x[1] == '1') && x[2] == ']' && (x += 3) && isspace(*x))))
			result->type = plr_msgstr;
		else {
			inv:
			result->type = plr_invalid;
			return;
		}
		while(isspace(*x) && x < end) x++;
		if(*x != '"') abort();
		e = x + strlen(x);
		assert(e > x && *e == 0);
		e--;
		while(isspace(*e)) e--;
		if(*e != '"') abort();
		*e = 0;
		result->len = convertbuf(x + 1, convbuf);
	} else goto inv;
}

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

int process(FILE *in, FILE *out) {
	struct mo_hdr mohdr = def_hdr;
	char line[4096]; char *lp;
	char convbuf[4096];
	unsigned off;
	
	unsigned num[plr_max] = {
		[plr_msgid] = 0,
		[plr_msgstr] = 0,
	};
	unsigned len[plr_max] = {
		[plr_msgid] = 0,
		[plr_msgstr] = 0,
	};
	
	// increased in pass 0 to point to the strings section
	// increased in pass 1 to point to the translation section
	enum passes pass;
	struct plr lineresult;
	
	mohdr.off_tbl_trans = mohdr.off_tbl_org;
	for(pass = pass_first; pass < pass_max; pass++) {
		switch(pass) {
			case pass_second:
				// start of second pass:
				// check that data gathered in first pass is consistent
				if(num[plr_msgid] != num[plr_msgstr]) abort();
				// calculate header fields from len and num arrays
				mohdr.numstring = num[plr_msgid];
				mohdr.off_tbl_org = sizeof(struct mo_hdr);
				mohdr.off_tbl_trans = mohdr.off_tbl_org + num[plr_msgid] * (sizeof(unsigned)*2);
				// print header
				fwrite(&mohdr, sizeof(mohdr), 1, out);				
				// set offset startvalue
				off = mohdr.off_tbl_trans + num[plr_msgstr] * (sizeof(unsigned)*2);
				break;
		}
		while((lp = fgets(line, sizeof(line), in))) {
			process_line(lp, line + sizeof(line), convbuf, &lineresult);
			if(lineresult.type == plr_invalid) continue;
			switch(pass) {
				case pass_collect_sizes:
					num[lineresult.type] += 1;
					len[lineresult.type] += lineresult.len;
					break;
				case pass_print_string_offsets:
					if(lineresult.type == plr_msgstr) break;
					write_offsets:
					// print length of current string
					fwrite(&lineresult.len, sizeof(unsigned), 1, out);
					// print offset of current string
					fwrite(&off, sizeof(unsigned), 1, out);
					off += lineresult.len + 1;
					break;
				case pass_print_translation_offsets:
					if(lineresult.type == plr_msgid) break;
					goto write_offsets;
				case pass_print_strings:
					if(lineresult.type == plr_msgstr) break;
					write_string:
					fwrite(convbuf, lineresult.len + 1, 1, out);
					break;
				case pass_print_translations:
					if(lineresult.type == plr_msgid) break;
					goto write_string;
					break;
			}
		}
		fseek(in, 0, SEEK_SET);
	}
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
					streq(A+2, "version") ||
					strstarts(A+2, "check-accelerators=") ||
					strstarts(A+2, "resource=") ||
					strstarts(A+2, "locale=")
					
				) {
				} else if((dest = strstarts(A+2, "output-file="))) {
					set_file(1, dest, &out);
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
				streq(A+1, "C") ||
				streq(A+1, "v")
			) {
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

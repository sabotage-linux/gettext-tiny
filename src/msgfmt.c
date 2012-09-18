/* msgfmt utility (C) 2012 rofl0r
 * released under the MIT license, see LICENSE for details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

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
	plr_str,
	plr_invalid,
	plr_max,
};

enum plr_type get_type_and_start(char* lp, char* end, size_t *stringstart) {
	enum plr_type result_type;
	char *x, *y;
	size_t start = (size_t) lp;
	while(isspace(*lp) && lp < end) lp++;
	if(lp[0] == '#') {
		inv:
		*stringstart = 0;
		return plr_invalid;
	}
	if((y = strstarts(lp, "msg"))) {
		if((x = strstarts(y, "id")) && (isspace(*x) || ((x = strstarts(x, "_plural")) && isspace(*x))))
			result_type = plr_msgid;
		else if ((x = strstarts(y, "str")) && (isspace(*x) || 
			(x[0] == '[' && (x[1] == '0' || x[1] == '1') && x[2] == ']' && (x += 3) && isspace(*x))))
			result_type = plr_msgstr;
		else 
			goto inv;
		while(isspace(*x) && x < end) x++;
		if(*x != '"') abort();
		conv:
		*stringstart = ((size_t) x - start) + 1;
	} else if(*lp == '"') {
		result_type = plr_str;
		x = lp;
		goto conv;
	} else {
		goto inv;
	}
	return result_type;
}

/* expects a pointer to the first char after a opening " in a string, 
 * converts the string into convbuf, and returns the length of that string */
size_t get_length_and_convert(char* x, char* end, char* convbuf) {
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

enum lineactions {
	la_incr,
	la_proc,
	la_abort,
	la_nop,
	la_max,
};

int process(FILE *in, FILE *out) {
	struct mo_hdr mohdr = def_hdr;
	char line[4096]; char *lp;
	char convbuf[4096];
	unsigned off;
	enum plr_type prev_type = plr_invalid;
	unsigned curr_len = 0;
	size_t strstart;
	
	unsigned num[plr_max] = {
		[plr_msgid] = 0,
		[plr_msgstr] = 0,
	};
	unsigned len[plr_max] = {
		[plr_msgid] = 0,
		[plr_msgstr] = 0,
	};
	static const enum lineactions action_tbl[plr_max][plr_max] = {
		// plr_str will never be set as curr_type
		[plr_str] = { 
			[plr_str] = la_abort,
			[plr_msgid] = la_abort,
			[plr_msgstr] = la_abort,
			[plr_invalid] = la_abort, 
		},
		[plr_msgid] = { 
			[plr_str] = la_incr,
			[plr_msgid] = la_proc,
			[plr_msgstr] = la_proc,
			[plr_invalid] = la_proc, 
		},
		[plr_msgstr] = { 
			[plr_str] = la_incr,
			[plr_msgid] = la_proc,
			[plr_msgstr] = la_proc,
			[plr_invalid] = la_proc, 
		},
		[plr_invalid] = { 
			[plr_str] = la_abort,
			[plr_msgid] = la_incr,
			[plr_msgstr] = la_incr,
			[plr_invalid] = la_nop, 
		},
	};
	
	// increased in pass 0 to point to the strings section
	// increased in pass 1 to point to the translation section
	enum passes pass;
	enum plr_type type;
	int finished;
	
	mohdr.off_tbl_trans = mohdr.off_tbl_org;
	for(pass = pass_first; pass < pass_max; pass++) {
		if(pass == pass_second) {
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
		}
		finished = 0;
		while((lp = fgets(line, sizeof(line), in))) {
			doline:
			type = get_type_and_start(lp, line + sizeof(line), &strstart);
			switch(action_tbl[prev_type][type]) {
				case la_incr:
					assert(type == plr_msgid || type == plr_msgstr || type == plr_str);
					curr_len += get_length_and_convert(lp + strstart, line + sizeof(line) - curr_len, convbuf + curr_len);
					break;
				case la_proc:
					assert(prev_type == plr_msgid || prev_type == plr_msgstr);
					switch(pass) {
						case pass_collect_sizes:
							num[prev_type] += 1;
							len[prev_type] += curr_len;
							break;
						case pass_print_string_offsets:
							if(prev_type == plr_msgstr) break;
							write_offsets:
							// print length of current string
							fwrite(&curr_len, sizeof(unsigned), 1, out);
							// print offset of current string
							fwrite(&off, sizeof(unsigned), 1, out);
							off += curr_len + 1;
							break;
						case pass_print_translation_offsets:
							if(prev_type == plr_msgid) break;
							goto write_offsets;
						case pass_print_strings:
							if(prev_type == plr_msgstr) break;
							write_string:
							fwrite(convbuf, curr_len + 1, 1, out);
							break;
						case pass_print_translations:
							if(prev_type == plr_msgid) break;
							goto write_string;
							break;
						default:
							abort();
					}
					if(type != plr_invalid)
						curr_len = get_length_and_convert(lp + strstart, line + sizeof(line), convbuf);
					else 
						curr_len = 0;
					break;
				case la_nop:
					break;
				case la_abort:
				default:
					abort();
			}
			if(type != plr_str) {
				prev_type = type;
			}
		}
		if(!finished) {
			// we need to make an extra pass of type invalid to trigger
			// processing of the last string.
			lp = line;
			*lp = 0;
			finished = 1;
			goto doline;
		}
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

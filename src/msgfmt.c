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
	char msgidbuf1[4096];
	unsigned milen1;
	char msgidbuf2[4096];
	unsigned milen2;
	char pluralbuf1[4096];
	unsigned pllen1;
	char pluralbuf2[4096];
	unsigned pllen2;
	char msgctxtbuf[4096];
	unsigned ctxtlen;
	char msgstrbuf1[4096];
	unsigned mslen1;
	char msgstrbuf2[4096];
	unsigned mslen2;
	unsigned msc;
	unsigned priv_type;
	unsigned priv_len;
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

enum sysdep_types {
	st_priu32 = 0,
	st_priu64,
	st_priumax,
	st_max
};

static const char sysdep_str[][10]={
	[st_priu32]  = "\x08<PRIu32>",
	[st_priu64]  = "\x08<PRIu64>",
	[st_priumax] = "\x09<PRIuMAX>",
};
static const char sysdep_repl[][8]={
	[st_priu32]  = "\x02lu\0u",
	[st_priu64]  = "\x02lu\0llu",
	[st_priumax] = "\x01ju"
};
static const char *get_repl(enum sysdep_types type, unsigned nr) {
	assert(nr < (unsigned)sysdep_repl[type][0]);
	const char* p = sysdep_repl[type]+1;
	while(nr--) p+=strlen(p)+1;
	return p;
}
static void replace(char* text, unsigned textlen, const char* what, const char * with) {
	char*p = text;
	size_t la = strlen(what), li=strlen(with);
	assert(la >= li);
	for(p=text;textlen >= la;) {
		if(!memcmp(p,what,la)) {
			memcpy(p, with, li);
			textlen -= la;
			memmove(p+li,p+la,textlen+1);
			p+=li;
		} else {
			p++;
			textlen--;
		}
	}
}
static unsigned get_form(enum sysdep_types type, unsigned no, unsigned occurences[st_max]) {
	unsigned i,divisor = 1;
	for(i=type+1;i<st_max;i++) if(occurences[i]) divisor *= sysdep_repl[i][0];
	return (no/divisor)%sysdep_repl[type][0];
}
static char** sysdep_transform(const char* text, unsigned textlen, unsigned *len, unsigned *count, int simulate) {
	unsigned occurences[st_max] = {0};
	const char *p=text,*o;
	unsigned i,j, l = textlen;
	while(l && (o=strchr(p, '<'))) {
		l-=o-p;p=o;
		unsigned f = 0;
		for(i=0;i<st_max;i++)
		if(l>=(unsigned)sysdep_str[i][0] && !memcmp(p,sysdep_str[i]+1,sysdep_str[i][0])) {
			occurences[i]++;
			f=1;
			p+=sysdep_str[i][0];
			l-=sysdep_str[i][0];
			break;
		}
		if(!f) p++,l--;
	}
	*count = 1;
	for(i=0;i<st_max;i++) if(occurences[i]) *count *= sysdep_repl[i][0];
	l = textlen * *count;
	for(i=0;i<*count;i++) for(j=0;j<st_max;j++)
	if(occurences[j]) l-= occurences[j] * (sysdep_str[j][0] - strlen(get_repl(j, get_form(j, i, occurences))));
	*len = l+*count-1;

	char **out = 0;
	if(!simulate) {
		out = malloc((sizeof(char*)+textlen+1) * *count);
		assert(out);
		char *p = (void*)(out+*count);
		for(i=0;i<*count;i++) {
			out[i]=p;
			memcpy(p, text, textlen+1);
			p+=textlen+1;
		}
		for(i=0;i<*count;i++) for(j=0;j<st_max;j++)
		if(occurences[j])
			replace(out[i], textlen, sysdep_str[j]+1, get_repl(j, get_form(j, i, occurences)));
	}

	return out;
}

static void error(const char* msg) {
	fprintf(stderr, msg);
	exit(1);
}

static inline void writemsg(struct callbackdata *d) {
	if(d->milen1 != 0) {
		if(!d->strlist[d->curr[pe_msgid]].str.off)
			d->strlist[d->curr[pe_msgid]].str.off=d->stroff[pe_msgid];

		if(d->ctxtlen != 0) {
			memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->msgctxtbuf, d->ctxtlen);
			d->strlist[d->curr[pe_msgid]].str.len+=d->ctxtlen;
			d->stroff[pe_msgid]+=d->ctxtlen;
		}
		memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->msgidbuf1, d->milen1);
		d->stroff[pe_msgid]+=d->milen1;
		d->strlist[d->curr[pe_msgid]].str.len+=d->milen1-1;
		if(d->pllen1 != 0) {
			memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->pluralbuf1, d->pllen1);
			d->strlist[d->curr[pe_msgid]].str.len+=d->pllen1;
			d->stroff[pe_msgid]+=d->pllen1;
		}
		d->curr[pe_msgid]++;
	}
	if(d->milen2 != 0) {
		if(!d->strlist[d->curr[pe_msgid]].str.off)
			d->strlist[d->curr[pe_msgid]].str.off=d->stroff[pe_msgid];

		if(d->ctxtlen != 0) {
			memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->msgctxtbuf, d->ctxtlen);
			d->strlist[d->curr[pe_msgid]].str.len+=d->ctxtlen;
			d->stroff[pe_msgid]+=d->ctxtlen;
		}
		memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->msgidbuf2, d->milen2);
		d->stroff[pe_msgid]+=d->milen2;
		d->strlist[d->curr[pe_msgid]].str.len+=d->milen2-1;
		if(d->pllen2 != 0) {
			memcpy(d->strbuffer[pe_msgid] + d->stroff[pe_msgid], d->pluralbuf2, d->pllen2);
			d->strlist[d->curr[pe_msgid]].str.len+=d->pllen2;
			d->stroff[pe_msgid]+=d->pllen2;
		}
		d->curr[pe_msgid]++;
	}

	d->pllen2=d->pllen1=d->ctxtlen=d->milen1=d->milen2=0;
}

static inline void writestr(struct callbackdata *d, struct po_info *info) {
	// msgid xx; msgstr ""; is widely happened, it's invalid
	if(d->curr[pe_msgstr] 
		// we do not check the 1st msgid/str
		&& ((d->curr[pe_msgid] - d->curr[pe_msgstr]) == 0)
		// in case curr[id] - curr[str] = 2(when sysdeps), .len == 0 is always true
		&& d->translist[d->curr[pe_msgstr]-1].len == 0) {
		// str empty, invalid, set back everything, so they will be overrided

		d->len[pe_msgid] -= d->strlist[d->curr[pe_msgstr]-1].str.len + 1;
		d->strlist[d->curr[pe_msgstr]-1].str.len=0;
		d->len[pe_msgstr]--;
		d->stroff[pe_msgid] = d->strlist[d->curr[pe_msgstr]-1].str.off;
		d->stroff[pe_msgstr] = d->translist[d->curr[pe_msgstr]-1].off;
		d->num[pe_msgid]--;
		d->curr[pe_msgid]--;
		d->num[pe_msgstr]--;
		d->curr[pe_msgstr]--;
		d->mslen1=d->mslen2=d->msc=0;
		return;
	}

	if(d->msc && d->msc <= info->nplurals) {
		// plural <= nplurals is allowed
		d->translist[d->curr[pe_msgstr]].len=d->mslen1-1;
		d->translist[d->curr[pe_msgstr]].off=d->stroff[pe_msgstr];
		d->strlist[d->curr[pe_msgstr]].trans = &d->translist[d->curr[pe_msgstr]];

		memcpy(d->strbuffer[pe_msgstr] + d->stroff[pe_msgstr], d->msgstrbuf1, d->mslen1);
		d->stroff[pe_msgstr]+=d->mslen1;
		d->curr[pe_msgstr]++;

		if(d->mslen2 != 0) {
			d->translist[d->curr[pe_msgstr]].len=d->mslen2-1;
			d->translist[d->curr[pe_msgstr]].off=d->stroff[pe_msgstr];
			d->strlist[d->curr[pe_msgstr]].trans = &d->translist[d->curr[pe_msgstr]];

			memcpy(d->strbuffer[pe_msgstr] + d->stroff[pe_msgstr], d->msgstrbuf2, d->mslen2);
			d->stroff[pe_msgstr]+=d->mslen2;
			d->curr[pe_msgstr]++;
		}

		d->mslen1=d->mslen2=d->msc=0;
	}
}

int process_line_callback(struct po_info* info, void* user) {
	struct callbackdata *d = (struct callbackdata *) user;
	assert(info->type == pe_msgid || info->type == pe_ctxt || info->type == pe_msgstr || info->type == pe_plural);
	char **sysdeps;
	unsigned len, count, i, l;
	switch(d->pass) {
		case pass_collect_sizes:
			sysdep_transform(info->text, info->textlen, &len, &count, 1);
			d->num[info->type] += count;
			if(info->type == pe_msgid && count == 2 && d->priv_type == pe_ctxt) {
				// ctxt meets msgid with sysdeps, multiply num and len to suit it
				d->len[pe_ctxt] += d->priv_len +1;
				d->num[pe_ctxt]++;
			}
			if(count != 1 && info->type == pe_ctxt) {
				// except msgid, str, plural, all other types should not have sysdeps
				abort();
			}

			d->priv_type = info->type;
			d->priv_len = len;
			d->len[info->type] += len +1;
			break;
		case pass_second:
			sysdeps = sysdep_transform(info->text, info->textlen, &len, &count, 0);
			for(i=0;i<count;i++) {
				l = strlen(sysdeps[i]);
				if(info->type == pe_msgid) {
					// after str, it's msgid or msgctxt
					writestr(d, info);
					// just copy, it's written down when writemsg()
					if(i==0) {
						assert(l+1 <= sizeof(d->msgidbuf1));
						memcpy(d->msgidbuf1, sysdeps[i], l+1);
						d->milen1 = l+1;
					} else {
						assert(l+1 <= sizeof(d->msgidbuf2));
						memcpy(d->msgidbuf2, sysdeps[i], l+1);
						d->milen2 = l+1;
					}
				} else if(info->type == pe_plural) {
					if(i==0) {
						assert(l+1 <= sizeof(d->pluralbuf1));
						memcpy(d->pluralbuf1, sysdeps[i], l+1);
						d->pllen1 = l+1;
					} else {
						assert(l+1 <= sizeof(d->pluralbuf2));
						memcpy(d->pluralbuf2, sysdeps[i], l+1);
						d->pllen2 = l+1;
					}
				} else if(info->type == pe_ctxt) {
					writestr(d, info);
					assert(l+1 <= sizeof(d->msgctxtbuf));
					d->ctxtlen = l+1;
					memcpy(d->msgctxtbuf, sysdeps[i], l);
					d->msgctxtbuf[l] = 0x4;//EOT
				} else {
					writemsg(d);
					// just copy, it's written down when writestr()
					if(i==0) {
						assert(l+1 <= sizeof(d->msgstrbuf1)/info->nplurals);
						memcpy(&d->msgstrbuf1[d->mslen1], sysdeps[i], l+1);
						d->mslen1 += l+1;
						d->msc++;
					} else {
						// sysdeps exist
						assert(l+1 <= sizeof(d->msgstrbuf2)/info->nplurals);
						memcpy(&d->msgstrbuf2[d->mslen2], sysdeps[i], l+1);
						d->mslen2 += l+1;
					}
				}
			}
			free(sysdeps);
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
			[pe_plural] = 0,
			[pe_ctxt] = 0,
		},
		.len = {
			[pe_msgid] = 0,
			[pe_msgstr] = 0,
			[pe_plural] = 0,
			[pe_ctxt] = 0,
		},
		.off = 0,
		.out = out,
		.pass = pass_first,
		.ctxtlen = 0,
		.pllen1 = 0,
		.pllen2 = 0,
		.milen1 = 0,
		.milen2 = 0,
		.mslen1 = 0,
		.mslen2 = 0,
		.msc = 0,
	};

	struct po_parser pb, *p = &pb;

	mohdr.off_tbl_trans = mohdr.off_tbl_org;
	for(d.pass = pass_first; d.pass <= pass_second; d.pass++) {
		if(d.pass == pass_second) {
			// start of second pass:
			// check that data gathered in first pass is consistent
			if((d.num[pe_msgid] + d.num[pe_plural] * (p->info.nplurals - 1)) < d.num[pe_msgstr]) {
				// one should actually abort here,
				// but gnu gettext simply writes an empty .mo and returns success.
				//abort();
				fprintf(stderr, "warning: mismatch of msgid/msgstr count, writing empty .mo file\n");
				d.num[pe_msgid] = 0;
				return 0;
			}

			d.strlist = calloc(d.num[pe_msgid] * sizeof(struct strmap), 1);
			d.translist = calloc(d.num[pe_msgstr] * sizeof(struct strtbl), 1);
			d.strbuffer[pe_msgid] = calloc(d.len[pe_msgid]+d.len[pe_plural]+d.len[pe_ctxt], 1);
			d.strbuffer[pe_msgstr] = calloc(d.len[pe_msgstr], 1);
			d.stroff[pe_msgid] = d.stroff[pe_msgstr] = 0;
			assert(d.strlist && d.translist && d.strbuffer[pe_msgid] && d.strbuffer[pe_msgstr]);
		}

		poparser_init(p, convbuf, sizeof(convbuf), process_line_callback, &d);

		while((lp = fgets(line, sizeof(line), in))) {
			poparser_feed_line(p, lp, sizeof(line));
		}
		poparser_finish(p);
		writestr(&d, &p->info);

		if(d.pass == pass_second) {
			// calculate header fields from len and num arrays
			mohdr.numstring = d.num[pe_msgid];
			mohdr.off_tbl_org = sizeof(struct mo_hdr);
			mohdr.off_tbl_trans = mohdr.off_tbl_org + d.num[pe_msgid] * (sizeof(unsigned)*2);
			// set offset startvalue
			d.off = mohdr.off_tbl_trans + d.num[pe_msgid] * (sizeof(unsigned)*2);
		}
		fseek(in, 0, SEEK_SET);
	}

	cb_for_qsort = &d;
	qsort(d.strlist, d.num[pe_msgid], sizeof (struct strmap), strmap_comp);
	unsigned i;

	// print header
	fwrite(&mohdr, sizeof(mohdr), 1, out);
	for(i = 0; i < d.num[pe_msgid]; i++) {
		d.strlist[i].str.off += d.off;
		fwrite(&d.strlist[i].str, sizeof(struct strtbl), 1, d.out);
	}
	for(i = 0; i < d.num[pe_msgid]; i++) {
		d.strlist[i].trans->off += d.off + d.len[pe_msgid] + d.len[pe_plural] + d.len[pe_ctxt];
		fwrite(d.strlist[i].trans, sizeof(struct strtbl), 1, d.out);
	}
	fwrite(d.strbuffer[pe_msgid], d.len[pe_msgid]+d.len[pe_plural]+d.len[pe_ctxt], 1, d.out);
	fwrite(d.strbuffer[pe_msgstr], d.len[pe_msgstr], 1, d.out);

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
	int statistics = 0;
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
					streq(A+2, "v") ||
					strstarts(A+2, "check-accelerators=") ||
					strstarts(A+2, "resource=") ||
					strstarts(A+2, "locale=")

				) {
				} else if((dest = strstarts(A+2, "output-file="))) {
					set_file(1, dest, &out);
				} else if(streq(A+2, "statistics")) {
					statistics = 1;
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
			} else if (streq(A+1, "V")) {
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
	if(in == NULL || out == NULL) {
		if(!statistics) syntax();
		else return 0;
	}
	int ret = process(in, out);
	fflush(in); fflush(out);
	if(in != stdin) fclose(in);
	if(out != stdout) fclose(out);
	return ret;
}

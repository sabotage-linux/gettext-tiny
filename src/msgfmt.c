/* msgfmt utility (C) 2012 rofl0r
 * released under the MIT license, see LICENSE for details */
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include "poparser.h"

static void syntax(void) {
	fprintf(stdout, "Usage: msgfmt [OPTION] filename.po ...\n");
}

static void version(void) {
	fprintf(stdout, "msgfmt (GNU gettext-tools compatible) 99.9999.9999\n");
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

struct strtbl {
	uint32_t len, off;
};

struct strmap {
	struct strtbl str;
	struct strtbl trans;
};

struct callbackdata {
	FILE* out;
	enum po_stage stage;
	size_t cnt;
	size_t len[2];
	char* buf[2];
	struct strmap *list;
};

static struct callbackdata *cb_for_qsort;
int strtbl_cmp(const void *a_, const void *b_) {
	const struct strmap *a = a_, *b = b_;
	return strcmp(cb_for_qsort->buf[0] + a->str.off, cb_for_qsort->buf[0] + b->str.off);
}

int process_line_callback(po_message_t msg, void* user) {
	struct callbackdata *d = (struct callbackdata *) user;
	struct strtbl *str, *trans;
	size_t m;
	int i, j, k = msg->sysdep;

	if (msg->flags & PO_FUZZY) return 0;
	if (msg->strlen[0] == 0) return 0;

	switch(d->stage) {
	case ps_size:
		m = 0;
		m += msg->id_len + 1;

		if (msg->plural_len)
			m += msg->plural_len + 1;

		if (msg->ctxt_len)
			m += msg->ctxt_len + 1;

		d->len[0] += m * k;

		m = 0;
		for (i=0; msg->strlen[i]; i++) {
			m += msg->strlen[i] + 1;
		}
		d->len[1] += m * k;

		d->cnt += k;
		break;
	case ps_parse:
		for (j=0; j < k; j++) {
			str = &d->list[d->cnt].str;
			trans = &d->list[d->cnt].trans;

			str->off = d->len[0];
			str->len = 0;

			if (msg->ctxt_len) {
				m = poparser_sysdep(msg->ctxt, &d->buf[0][d->len[0]], j);
				str->len += m;
				d->buf[0][d->len[0]+m-1] = 0x4;
				d->len[0] += m;
			}

			m = poparser_sysdep(msg->id, &d->buf[0][d->len[0]], j);
			str->len += m;
			d->len[0] += m;

			if (msg->plural_len) {
				m = poparser_sysdep(msg->plural, &d->buf[0][d->len[0]], j);
				str->len += m;
				d->len[0] += m;
			}

			trans->off = d->len[1];
			trans->len = 0;
			for (i=0; msg->strlen[i]; i++) {
				m = poparser_sysdep(msg->str[i], &d->buf[1][d->len[1]], j);
				trans->len += m;
				d->len[1] += m;
			}
		}

		break;
	default:
		abort();
	}
	return 0;
}

int process(FILE *in, FILE *out) {
	struct mo_hdr mohdr = def_hdr;
	char line[8192]; char *lp;
	size_t off, i;
	enum po_error t;
	char convbuf[32768];

	struct callbackdata d = {
		.len = {0, 0},
		.cnt = 0,
		.out = out,
	};

	struct po_parser pb, *p = &pb;

	mohdr.off_tbl_trans = mohdr.off_tbl_org;

	poparser_init(p, convbuf, sizeof(convbuf), process_line_callback, &d);
	d.stage = p->stage;

	while((lp = fgets(line, sizeof(line), in))) {
		if ((t = poparser_feed_line(p, lp, strlen(line))) != po_success)
			return t;
	}
	if ((t = poparser_finish(p)) != po_success)
		return t;

	if (d.cnt == 0) return -1;

	d.list = (struct strmap*)malloc(sizeof(struct strmap)*d.cnt);
	d.buf[0] = (char*)malloc(d.len[0]);
	d.buf[1] = (char*)malloc(d.len[1]);
	d.len[0] = 0;
	d.len[1] = 0;
	d.cnt = 0;
	d.stage = p->stage;

	fseek(in, 0, SEEK_SET);
	while ((lp = fgets(line, sizeof(line), in))) {
		if ((t = poparser_feed_line(p, lp, strlen(line))) != po_success) {
			free(d.list);
			free(d.buf[0]);
			free(d.buf[1]);
			return t;
		}
	}
	if ((t = poparser_finish(p)) != po_success) {
		free(d.list);
		free(d.buf[0]);
		free(d.buf[1]);
		return t;
	}

	cb_for_qsort = &d;
	qsort(d.list, d.cnt, sizeof(struct strmap), strtbl_cmp);
	cb_for_qsort = NULL;

	// print header
	mohdr.numstring = d.cnt;
	mohdr.off_tbl_org = sizeof(struct mo_hdr);
	mohdr.off_tbl_trans = mohdr.off_tbl_org + d.cnt * sizeof(struct strtbl);
	fwrite(&mohdr, sizeof(mohdr), 1, out);

	off = mohdr.off_tbl_trans + d.cnt * sizeof(struct strtbl);
	for (i = 0; i < d.cnt; i++) {
		d.list[i].str.off += off;
		fwrite(&d.list[i].str, sizeof(struct strtbl), 1, d.out);
	}

	off += d.len[0];
	for (i = 0; i < d.cnt; i++) {
		d.list[i].trans.off += off;
		fwrite(&d.list[i].trans, sizeof(struct strtbl), 1, d.out);
	}

	fwrite(d.buf[0], d.len[0], 1, d.out);
	fwrite(d.buf[1], d.len[1], 1, d.out);

	free(d.list);
	free(d.buf[0]);
	free(d.buf[1]);

	return 0;
}

void set_file(int out, char* fn, FILE** dest) {
	if(streq(fn, "-")) {
		if(out) {
			*dest = stdout;
		} else {
			char b[4096];
			size_t n=0;
			FILE* tmpf = tmpfile();
			if(!tmpf)
				perror("tmpfile");

			while((n=fread(b, sizeof(*b), sizeof(b), stdin)) > 0)
				fwrite(b, sizeof(*b), n, tmpf);

			fseek(tmpf, 0, SEEK_SET);
			*dest = tmpf;
		}
	} else {
		*dest = fopen(fn, out ? "w" : "r");
	}
	if(!*dest) {
		perror("fopen");
		exit(1);
	}
}

int main(int argc, char**argv) {
	if (argc == 1) {
		syntax();
		return 0;
	}

	int arg = 1;
	FILE *out = NULL;
	FILE *in = NULL;
	int expect_in_fn = 1;
	char path[PATH_MAX];
	char* locale = NULL;
	char* dest = NULL;
#define A argv[arg]
	for(; arg < argc; arg++) {
		if(A[0] == '-') {
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
					strstarts(A+2, "resource=")
					) {
					} else if((locale = strstarts(A+2, "locale="))) {
					} else if((dest = strstarts(A+2, "output-file="))) {
						set_file(1, dest, &out);
					} else if(streq(A+2, "version")) {
						version();
						return 0;
					} else if(streq(A+2, "help")) {
						syntax();
						return 0;
					} else if (expect_in_fn) {
						set_file(0, A, &in);
						expect_in_fn = 0;
					}
			} else if(streq(A + 1, "o")) {
				arg++;
				dest = A;
				set_file(1, A, &out);
			} else if(
				streq(A+1, "j") ||
				streq(A+1, "r") ||
				streq(A+1, "P") ||
				streq(A+1, "f") ||
				streq(A+1, "a") ||
				streq(A+1, "c") ||
				streq(A+1, "v") ||
				streq(A+1, "C")
			) {
			} else if (streq(A+1, "V")) {
				version();
				return 0;
			} else if (streq(A+1, "h")) {
				syntax();
				return 0;
			} else if (streq(A+1, "l")) {
				arg++;
				locale = A;
			} else if (streq(A+1, "d")) {
				arg++;
				dest = A;
			} else if (expect_in_fn) {
				set_file(0, A, &in);
				expect_in_fn = 0;
			}
		} else if (expect_in_fn) {
			set_file(0, A, &in);
			expect_in_fn = 0;
		}
	}

	if (locale != NULL && dest != NULL) {
		snprintf(path, sizeof(path), "%s/%s.msg", dest, locale);
		FILE *fp = fopen(path, "w");
		if (fp) {
			fclose(fp);
			return 0;
		}

		return -1;
	}

	if(out == NULL) {
		dest = "messages.mo";
		set_file(1, "messages.mo", &out);
	}

	if(in == NULL || out == NULL) {
		return -1;
	}

	int ret = process(in, out);
	fflush(in); fflush(out);

	if(in != stdin) fclose(in);
	if(out != stdout) fclose(out);

	if (ret < 0) {
		return remove(dest);
	}
	return ret;
}

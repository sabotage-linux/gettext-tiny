#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>
#include "poparser.h"
#include "StringEscape.h"

#define strstarts(S, W) (memcmp(S, W, sizeof(W) - 1) ? NULL : (S + (sizeof(W) - 1)))

#define PO_SYSDEP_PRIU32 (1 << st_priu32)
#define PO_SYSDEP_PRIU64 (1 << st_priu64)
// for complement, no usage
#define PO_SYSDEP_PRIUMAX 0

static const char* sysdep_str[st_max]={
	[st_priu32] = "<PRIu32>",
	[st_priu64] = "<PRIu64>",
	[st_priumax] = "<PRIuMAX>",
};

static const char* sysdep_repl[st_max][3]={
	[st_priu32]  = {"u", "lu", "u"},
	[st_priu64]  = {"lu", "llu", "llu"},
	[st_priumax]  = {"ju", "ju", "ju"},
};

static const int sysdep[st_max]={
	[st_priu32]  = PO_SYSDEP_PRIU32,
	[st_priu64]  = PO_SYSDEP_PRIU64,
	[st_priumax]  = PO_SYSDEP_PRIUMAX,
};

void poparser_init(struct po_parser *p, poparser_callback cb, void* cbdata) {
	int cnt;
	memset(p, 0, sizeof(struct po_parser));
	p->bufsize = 4096;
	p->buf = malloc(p->bufsize);
	p->iconv_bufsize = 4096;
	p->iconv_buf = malloc(p->iconv_bufsize);
	p->bufpos = 0;
	p->cb = cb;
	p->cbdata = cbdata;
	p->hdr.nplurals = MAX_NPLURALS;
	p->max_ctxt_len = 1;
	p->max_id_len = 1;
	p->max_plural_len = 1;
	for (cnt = 0; cnt < MAX_NPLURALS; cnt++)
		p->max_strlen[cnt] = 1;
	p->current_strcnt = 0;
	p->current_trans_index = 0;
}

static inline enum po_error poparser_feed_hdr(struct po_parser *p, char* msg) {
	char *x, *y;
	if (!msg || p->stage != ps_parse || p->current_trans_index != 0)
		return po_success;

	if ((x = strstr(msg, "charset="))) {
		for (y = x; *y && *y != '\\' && !isspace(*y); y++);

		if ((uintptr_t)(y-x-7) > sizeof(p->hdr.charset))
			return -po_unsupported_charset;

		memcpy(p->hdr.charset, x+8, y-x-8);
		p->hdr.charset[y-x-8] = 0;

		p->cd = iconv_open("UTF-8", p->hdr.charset);
		if (p->cd == (iconv_t)-1) {
			p->cd = 0;
			if (p->strict) return -po_unsupported_charset;
		}
	}

	if ((x = strstr(msg, "nplurals="))) {
		p->hdr.nplurals = *(x+9) - '0';
	}

	return po_success;
}

static inline enum po_error poparser_clean(struct po_parser *p, po_message_t msg) {
	if (p->current_strcnt) {
		// PO_SYSDEP_PRIUMAX == 0, it has no effects to our codes
		switch (msg->sysdep) {
		case PO_SYSDEP_PRIU32:
		case PO_SYSDEP_PRIU64:
			msg->sysdep = 2;
			break;
		case PO_SYSDEP_PRIU32|PO_SYSDEP_PRIU64:
			msg->sysdep = 3;
			break;
		default:
			msg->sysdep = 1;
			break;
		}

		// met a new block starting with msgid
		if (p->cb)
			p->cb(msg, p->cbdata);

		msg->sysdep = 0;
		msg->ctxt_len = 0;
		msg->id_len = 0;
		msg->plural_len = 0;
		msg->flags = 0;
		p->current_strcnt = 0;
		p->current_trans_index++;
	}

	return po_success;
}

enum po_error poparser_feed(struct po_parser *p, char* in, size_t in_len) {
	enum po_error t = po_success;
	po_message_t msg = &p->msg;
	int cnt = 0;
	size_t len;
	char *x, *y, *z;

	if ( (p->bufsize - p->bufpos) < in_len ) {
		p->bufsize += + in_len;
		p->buf = realloc(p->buf, p->bufsize);
	}
	memcpy(&p->buf[p->bufpos], in, in_len);
	p->bufpos += in_len;
	p->buf[p->bufpos] = 0;

	x = p->buf;
	for (;(z = strpbrk(x, "\r\n")) != NULL;x=z) {
		// make null terminator for the current line, then point to next line
		*z++ = 0;

		// if we need to conv encodings
		if (p->cd) {
			in_len = z - x;
			y = p->iconv_buf;
			len = p->iconv_bufsize;

			errno = 0;
			while (iconv(p->cd, &x, &in_len, &y, &len) == (size_t)-1) {
				if (errno == E2BIG) {
					// not big enough buffer
					len = y - p->iconv_buf;
					p->iconv_buf = realloc(p->iconv_buf, p->iconv_bufsize + in_len * 4);
					p->iconv_bufsize += in_len * 4;
					y = &p->iconv_buf[len];
					len = p->iconv_bufsize - len;
				} else {
					t = -po_failed_iconv;
					goto exit;
				}
			}

			x = p->iconv_buf;
		}

		while (x && *x != 0) {
			switch (*x++) {
			case ' ':
			case '\t':
				break;
			case '#':
				if (p->current_entry == po_str) {
					if ( (t = poparser_clean(p, msg)) != po_success) {
						goto exit;
					}
				}

				switch (*x) {
				case ',':
					for (x = strtok(x, " ,"); x; x = strtok(NULL, " ")) {
						if (!strcmp(x, "fuzzy")) {
							msg->flags |= PO_FUZZY;
						}
					}
					break;
				case '.':
					// extracted comments for translators, ignore
				case ':':
					// reference comments for translators, ignore
				case '|':
					// current_entry untranslated strings for translators, ignore
				default:
					// ignore normal comments
					break;
				}

				x = NULL;
				break;
			case '"':
				y = x;
				while (true) {
					if ( (y = strchr(y, '"')) == NULL) {
						t = -po_excepted_token;
						goto exit;
					}

					// only if it's not an escaped "
					if (*(y-1) != '\\') break;
					else y++;
				}

				len = y - x;
				*y = 0;

				for (cnt = 0; cnt < st_max; cnt++) {
					if (strstr(x, sysdep_str[cnt])) {
						msg->sysdep |= sysdep[cnt];
					}
				}

				switch (p->current_entry) {
				case po_str:
					if (p->stage == ps_parse) {
						if (msg->str[p->current_strcnt - 1] == NULL) {
							t = -po_internal;
							goto exit;
						}

						len = unescape(x, &msg->str[p->current_strcnt - 1][msg->strlen[p->current_strcnt - 1]], p->max_strlen[p->current_strcnt - 1]);
						msg->strlen[p->current_strcnt - 1] += len;

						if ((t = poparser_feed_hdr(p, x)) != po_success) {
							goto exit;
						}
					} else if (p->stage == ps_size) {

						msg->strlen[p->current_strcnt - 1] += len;
						if (p->max_strlen[p->current_strcnt - 1] < msg->strlen[p->current_strcnt - 1])
							p->max_strlen[p->current_strcnt - 1] = msg->strlen[p->current_strcnt - 1] + 1;
					}
					break;
				case po_plural:
					if (p->stage == ps_parse) {
						if (msg->plural == NULL) {
							t = -po_internal;
							goto exit;
						}

						len = unescape(x, &msg->plural[msg->plural_len], p->max_plural_len);
						msg->plural_len += len;
					} else if (p->stage == ps_size) {
						msg->plural_len += len;
						if (p->max_plural_len < msg->plural_len)
							p->max_plural_len = msg->plural_len + 1;
					}
					break;
				case po_id:
					if (p->stage == ps_parse) {
						if (msg->id == NULL) {
							t = -po_internal;
							goto exit;
						}

						len = unescape(x, &msg->id[msg->id_len], p->max_id_len);
						msg->id_len += len;
					} else if (p->stage == ps_size) {
						msg->id_len += len;
						if (p->max_id_len < msg->id_len)
							p->max_id_len = msg->id_len + 1;
					}
					break;
				case po_ctxt:
					if (p->stage == ps_parse) {
						if (msg->ctxt == NULL) {
							t = -po_internal;
							goto exit;
						}

						len = unescape(x, &msg->ctxt[msg->ctxt_len], p->max_ctxt_len);
						msg->ctxt_len += len;
					} else if (p->stage == ps_size) {
						msg->ctxt_len += len;
						if (p->max_ctxt_len < msg->ctxt_len)
							p->max_ctxt_len = msg->ctxt_len + 1;
					}
					break;
				}

				x = y + 1;
				break;
			case 'm':
				if ((y = strstarts(x, "sgctxt"))) {
					if ( (t = poparser_clean(p, msg)) != po_success) {
						goto exit;
					}

					if (msg->id_len || msg->plural_len) {
						t = -po_invalid_entry;
						goto exit;
					}

					msg->ctxt_len = 0;
					p->current_entry = po_ctxt;
				} else if ((y = strstarts(x, "sgid_plural"))) {
					if (!msg->id_len || p->current_strcnt) {
						t = -po_invalid_entry;
						goto exit;
					}

					msg->plural_len = 0;
					p->current_entry = po_plural;
				} else if ((y = strstarts(x, "sgid"))) {
					if ( (t = poparser_clean(p, msg)) != po_success) {
						goto exit;
					}

					if (msg->plural_len) {
						t = -po_invalid_entry;
						goto exit;
					}

					msg->id_len = 0;
					p->current_entry = po_id;
				} else if ((y = strstarts(x, "sgstr"))) {
					if (p->current_trans_index != 0 && !msg->id_len) {
						t = -po_invalid_entry;
						goto exit;
					}

					if (*y == '[') {
						if (!msg->plural_len) {
							t = -po_invalid_entry;
							goto exit;
						}

						if (y[2] != ']') {
							t = -po_excepted_token;
							goto exit;
						}

						p->current_strcnt = y[1] - '0' + 1;

						if (p->strict && p->current_strcnt > p->hdr.nplurals) {
							t = -po_plurals_overflow;
							goto exit;
						}

						y += 3; // skip [n]
					} else {
						if (p->current_strcnt || msg->plural_len) {
							t = -po_invalid_entry;
							goto exit;
						}

						p->current_strcnt = 1;
					}

					msg->strlen[p->current_strcnt - 1] = 0;
					p->current_entry = po_str;
				} else {
					t = -po_invalid_entry;
					goto exit;
				}

				x = y;
				break;
			default:
				t = -po_invalid_entry;
				goto exit;
			}
		}
	}

exit:
	// if loop done normally, use x
	if (z == 0) z = x;

	// if goto(in case z!=0), use z
	if (z != p->buf) {
		// drop consumed lines
		len = p->buf + p->bufpos - z;
		memcpy(p->buf, z, len);
		p->bufpos = len;
	}

	return t;
}

enum po_error poparser_finish(struct po_parser *p) {
	size_t len;
	int cnt;
	enum po_error t;
	po_message_t msg = &p->msg;

	if (p->stage == ps_size) {
		if ( (t = poparser_feed(p, "\n", 1)) != po_success)
			return t;
		if ( (t = poparser_clean(p, msg)) != po_success)
			return t;

		len = p->max_ctxt_len;
		len += p->max_id_len;
		len += p->max_plural_len;
		for (cnt = 0; cnt < MAX_NPLURALS; cnt++)
			len += p->max_strlen[cnt];

		memset(msg, 0, sizeof(struct po_message));
		msg->ctxt = malloc(p->max_ctxt_len);
		msg->id = malloc(p->max_id_len);
		msg->plural = malloc(p->max_plural_len);

		for (cnt = 0; cnt < MAX_NPLURALS; cnt++)
			msg->str[cnt] = malloc(p->max_strlen[cnt]);

		p->hdr.nplurals = 2;
		p->current_trans_index = 0;
		p->bufpos = 0;
		p->iconv_bufpos = 0;
	} else {
		if ( (t = poparser_feed(p, "\n", 1)) != po_success)
			return t;
		if ( (t = poparser_clean(p, msg)) != po_success)
			return t;

		if (msg->ctxt) free(msg->ctxt);
		if (msg->id) free(msg->id);
		if (msg->plural) free(msg->plural);

		for (cnt = 0; cnt < MAX_NPLURALS; cnt++)
			if (msg->str[cnt]) free(msg->str[cnt]);

		if (p->buf) free(p->buf);
		if (p->iconv_buf) free(p->iconv_buf);
		if (p->cd) iconv_close(p->cd);
	}

	if (p->stage < ps_parse) p->stage++;

	return po_success;
}

size_t poparser_sysdep(const char *in, char *out, int num) {
	const char *x, *y, *outs;
	size_t m;
	int n;
	outs = out;
	x = in;

	while ((y = strchr(x, '%'))) {
		y++;
		if (outs)
			memcpy(out, x, y-x);
		out += y-x;
		x = y;

		for (n=0; n < st_max; n++) {
			m = strlen(sysdep_str[n]);
			if (!strncmp(y, sysdep_str[n], m)) {
				x = y + m;

				y = sysdep_repl[n][num];
				m = strlen(y);
				if (outs)
					memcpy(out, y, m);
				out += m;

				break;
			}
		}
	}

	m = strlen(x);
	if (outs)
		memcpy(out, x, m+1);
	out += m;
	return out - outs;
}

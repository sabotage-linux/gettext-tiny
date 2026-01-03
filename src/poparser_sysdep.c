#include "poparser.h"

const sysdep_case_t sysdep_cases[MAX_SYSDEP] = {
	{
		.format = "<PRIu32>",
		.repl = {"u", NULL},
	},
	{
		.format = "<PRIu64>",
		.repl = {"lu", "llu", NULL},
	},
	{
		.format = "<PRIuMAX>",
		.repl = {"lu", "llu", NULL},
	},
	{
		.format = "<PRIuPTR>",
		.repl = {"u", "lu", NULL},
	},
	{
		.format = "<PRId32>",
		.repl = {"d", NULL},
	},
	{
		.format = "<PRId64>",
		.repl = {"ld", "lld", NULL},
	},
	{
		.format = "<PRIdMAX>",
		.repl = {"ld", "lld", NULL},
	},
	{
		.format = "<PRIdPTR>",
		.repl = {"d", "ld", NULL},
	},
	{
		.format = "<PRIx32>",
		.repl = {"x", NULL},
	},
	{
		.format = "<PRIx64>",
		.repl = {"lx", "llx", NULL},
	},
	{
		.format = "<PRIxMAX>",
		.repl = {"lx", "llx", NULL},
	},
	{
		.format = "<PRIxPTR>",
		.repl = {"x", "lx", NULL},
	},
	{
		.format = "<PRIX32>",
		.repl = {"X", NULL},
	},
	{
		.format = "<PRIX64>",
		.repl = {"lX", "llX", NULL},
	},
	{
		.format = "<PRIXMAX>",
		.repl = {"lX", "llX", NULL},
	},
	{
		.format = "<PRIXPTR>",
		.repl = {"X", "lX", NULL},
	},
	{
		.format = "<PRIo32>",
		.repl = {"o", NULL},
	},
	{
		.format = "<PRIo64>",
		.repl = {"lo", "llo", NULL},
	},
	{
		.format = "<PRIoMAX>",
		.repl = {"lo", "llo", NULL},
	},
	{
		.format = "<PRIoPTR>",
		.repl = {"o", "lo", NULL},
	},
	{
		.format = "<PRIi32>",
		.repl = {"i", NULL},
	},
	{
		.format = "<PRIi64>",
		.repl = {"li", "lli", NULL},
	},
	{
		.format = "<PRIiMAX>",
		.repl = {"li", "lli", NULL},
	},
	{
		.format = "<PRIiPTR>",
		.repl = {"i", "li", NULL},
	},
	{0},
};

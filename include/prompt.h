/* (c) FRINKnet & Friends - MIT License */

#ifndef PROMPT_H
#define PROMPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static const char *VAMP_DEFAULT_TEMPLATE = "{instructions}\n/* READ: {filename} */\n{file}\n/* WRITE: {filename} */\n";
static const char *VAMP_DEFAULT_INSTRUCTIONS = "Update this file according to the instructions and code context.";

/* Read entire file into NUL-terminated buffer, return malloc'd string or NULL */
static char *  prompt_read_file(const char *path) {
	FILE *f;
	long sz;
	size_t nread;
	char *buf;

	f = fopen(path, "rb");

	if (!f) return NULL;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);

		return NULL;
	}

	sz = ftell(f);

	if (sz < 0) {
		fclose(f);

		return NULL;
	}

	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);

		return NULL;
	}

	buf = (char *)malloc((size_t)sz + 1);

	if (!buf) {
		fclose(f);

		return NULL;
	}

	nread = fread(buf, 1, (size_t)sz, f);

	fclose(f);

	if (nread != (size_t)sz) {
		free(buf);

		return NULL;
	}

	buf[sz] = '\0';

	return buf;
}

/* Load template: from section's template key or default */
static char *prompt_load_template(const struct vamp_section *sec) {
	const char *path;
	char *t;
	size_t n;

	path = config_get_value(sec, "template");

	if (path && path[0]) {
		t = prompt_read_file(path);

		return t; /* may be NULL on error */
	}

	/* duplicate default template */
	n = strlen(VAMP_DEFAULT_TEMPLATE);
	t = (char *)malloc(n + 1);

	if (!t) return NULL;

	memcpy(t, VAMP_DEFAULT_TEMPLATE, n + 1);

	return t;
}

/* Simple placeholder replace: replace all occurrences of needle with repl */
static char *prompt_replace_all(const char *input, const char *needle, const char *repl) {
	size_t in_len, needle_len, repl_len, count, out_len, chunk, tail;
	const char *p, *src;
	char *out, *dst;

	in_len = strlen(input);
	needle_len = strlen(needle);
	repl_len = strlen(repl);

	if (needle_len == 0) {
		out = (char *)malloc(in_len + 1);

		if (!out) return NULL;

		memcpy(out, input, in_len + 1);

		return out;
	}

	/* First pass: count occurrences */
	count = 0;
	p = input;

	while ((p = strstr(p, needle)) != NULL) {
		count++;
		p += needle_len;
	}

	if (count == 0) {
		out = (char *)malloc(in_len + 1);

		if (!out) return NULL;

		memcpy(out, input, in_len + 1);

		return out;
	}

	out_len = in_len + count * (repl_len - needle_len);
	out = (char *)malloc(out_len + 1);

	if (!out) return NULL;

	src = input;
	dst = out;

	while ((p = strstr(src, needle)) != NULL) {
		chunk = (size_t)(p - src);

		memcpy(dst, src, chunk);

		dst += chunk;

		memcpy(dst, repl, repl_len);

		dst += repl_len;
		src = p + needle_len;
	}

	tail = strlen(src);

	memcpy(dst, src, tail);

	dst += tail;
	*dst = '\0';

	return out;
}

/* Build final prompt: template with {instructions}, {filename}, {file} */
static char *prompt_build_prompt(const struct vamp_section *sec, const char *filename, const char *file_contents) {
	const char *instr;
	char *tmpl, *s1, *s2, *s3;

	instr = config_get_value(sec, "instructions");

	if (!instr || !instr[0]) instr = VAMP_DEFAULT_INSTRUCTIONS;

	tmpl = prompt_load_template(sec);

	if (!tmpl) return NULL;

	s1 = prompt_replace_all(tmpl, "{instructions}", instr);

	free(tmpl);

	if (!s1) return NULL;

	s2 = prompt_replace_all(s1, "{filename}", filename);

	free(s1);

	if (!s2) return NULL;

	s3 = prompt_replace_all(s2, "{file}", file_contents);

	free(s2);

	if (!s3) return NULL;

	return s3;
}

#endif /* PROMPT_H */

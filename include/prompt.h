/* (c) FRINKnet & Friends - MIT License */

#ifndef PROMPT_H
#define PROMPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static const char *VAMP_DEFAULT_TEMPLATE = "{instructions}\n/* TAGS: \n{tags}\n*/\n/* READ: {filename} */\n{file}\n/* WRITE: {filename} */\n";
static const char *VAMP_DEFAULT_INSTRUCTIONS = "Update the file fixing the FIXME, TODO and REMOVE comments and output everything else unchanged.";

/* Read entire file into NUL-terminated buffer, return malloc'd string or NULL */
static char *prompt_read_file(const char *path) {
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

/* Internal helper: run ctags -x on filename, return malloc'd buffer or NULL */
static char *prompt_build_tags(const char *filename) {
	char cmd[4096];
	FILE *fp;
	char *buf = NULL;
	size_t cap = 0, len = 0;
	int c;

	/* no language forcing; let ctags decide based on filename */
	snprintf(cmd, sizeof(cmd), "ctags -x --_xformat=\"%%{pattern}\" '%s' 2>/dev/null", filename);

	fp = popen(cmd, "r");

	if (!fp) return NULL;

	while ((c = fgetc(fp)) != EOF) {
		if (len + 1 >= cap) {
			size_t ncap = cap ? cap * 2 : 4096;
			char *nbuf = (char *)realloc(buf, ncap);

			if (!nbuf) {
				free(buf);
				pclose(fp);

				return NULL;
			}

			buf = nbuf;
			cap = ncap;
		}

		buf[len++] = (char)c;
	}

	pclose(fp);

	if (!buf) return NULL;

	buf[len] = '\0';

	/* treat empty output as failure so we can say NO CTAGS */
	if (len == 0) {
		free(buf);

		return NULL;
	}

	return buf;
}

/* Build final prompt: template with {instructions}, {filename}, {file}, {tags} */
static char *prompt_build_prompt(const struct vamp_section *sec, const char *filename, const char *file_contents) {
	const char *instr;
	char *tmpl, *s1, *s2, *s3, *s4, *tags = NULL;
	const char *tags_repl;

	instr = config_get_value(sec, "instructions");

	if (!instr || !instr[0]) instr = VAMP_DEFAULT_INSTRUCTIONS;

	tmpl = prompt_load_template(sec);

	if (!tmpl) return NULL;

	/* instructions */
	s1 = prompt_replace_all(tmpl, "{instructions}", instr);

	free(tmpl);

	if (!s1) return NULL;

	/* filename */
	s2 = prompt_replace_all(s1, "{filename}", filename);

	free(s1);

	if (!s2) return NULL;

	/* file contents */
	s3 = prompt_replace_all(s2, "{file}", file_contents);

	free(s2);

	if (!s3) return NULL;

	/* tags: only do work if the template actually mentions {tags} */
	if (strstr(s3, "{tags}") != NULL) {

		tags = prompt_build_tags(filename);

		if (!tags || !tags[0]) {
			if (tags) free(tags);

			tags = NULL;
			tags_repl = "NO CTAGS";
		} else {
			tags_repl = tags;
		}

		s4 = prompt_replace_all(s3, "{tags}", tags_repl);

		free(s3);

		if (tags) free(tags);
		if (!s4) return NULL;

		return s4;
	}

	/* no {tags} in template, we're done */
	return s3;
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

#endif /* PROMPT_H */

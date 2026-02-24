/* (c) FRINKnet & Friends - MIT License */

#ifndef FETCH_H
#define FETCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "prompt.h"

static char *fetch_json_escape(const char *s) {
	size_t cap, len;
	char *out;
	const unsigned char *p;

	if (!s) {
		out = (char *)malloc(1);

		if (out) out[0] = '\0';

		return out;
	}

	cap = strlen(s) + 16;
	len = 0;
	out = (char *)malloc(cap);

	if (!out) return NULL;

	p = (const unsigned char *)s;

	while (*p) {
		unsigned char c;

		c = *p++;

		if (len + 6 >= cap) {
			size_t newcap;
			char *tmp;

			newcap = cap * 2;
			tmp = (char *)realloc(out, newcap);
			if (!tmp) {
				free(out);
				return NULL;
			}

			out = tmp;
			cap = newcap;
		}

		switch (c) {
		case '\"':
			out[len++] = '\\';
			out[len++] = '\"';

			break;
		case '\\':
			out[len++] = '\\';
			out[len++] = '\\';

			break;
		case '\b':
			out[len++] = '\\';
			out[len++] = 'b';

			break;
		case '\f':
			out[len++] = '\\';
			out[len++] = 'f';

			break;
		case '\n':
			out[len++] = '\\';
			out[len++] = 'n';

			break;
		case '\r':
			out[len++] = '\\';
			out[len++] = 'r';

			break;
		case '\t':
			out[len++] = '\\';
			out[len++] = 't';

			break;
		default:
			if (c < 0x20) {
				static const char hex[] = "0123456789abcdef";

				out[len++] = '\\';
				out[len++] = 'u';
				out[len++] = '0';
				out[len++] = '0';
				out[len++] = hex[(c >> 4) & 0xF];
				out[len++] = hex[c & 0xF];
			} else {
				out[len++] = (char)c;
			}

			break;
		}
	}

	out[len] = '\0';

	return out;
}

static char *fetch_build_json(const struct vamp_section *sec, const char *prompt) {
	const char *model, *maxtokens, *temperature;
	char *prompt_esc, *body;
	size_t needed;

	model = config_get_value(sec, "model");

	if (!model || !model[0]) model = "vine-omni";

	maxtokens = config_get_value(sec, "maxtokens");
	temperature = config_get_value(sec, "temperature");
	prompt_esc = fetch_json_escape(prompt);

	if (!prompt_esc) return NULL;

	needed = strlen(model) + strlen(prompt_esc) + 128;

	if (maxtokens) needed += strlen(maxtokens) + 32;
	if (temperature) needed += strlen(temperature) + 32;

	body = (char *)malloc(needed);

	if (!body) {
		free(prompt_esc);

		return NULL;
	}

	if (maxtokens && temperature) {
		snprintf(body, needed,  "{\"model\":\"%s\",\"prompt\":\"%s\",\"max_tokens\":%s,\"temperature\":%s}", model, prompt_esc, maxtokens, temperature);
	} else if (maxtokens) {
		snprintf(body, needed, "{\"model\":\"%s\",\"prompt\":\"%s\",\"max_tokens\":%s}", model, prompt_esc, maxtokens);
	} else if (temperature) {
		snprintf(body, needed, "{\"model\":\"%s\",\"prompt\":\"%s\",\"temperature\":%s}", model, prompt_esc, temperature);
	} else {
		snprintf(body, needed, "{\"model\":\"%s\",\"prompt\":\"%s\"}", model, prompt_esc);
	}

	free(prompt_esc);

	return body;
}

static int fetch_have_posturl(const struct vamp_section *sec, const char **out_url) {
	const char *url;

	url = config_get_value(sec, "posturl");

	if (!url || !url[0]) {
		fprintf(stderr, "vamp: missing 'posturl' in config\n");

		return 0;
	}

	*out_url = url;

	return 1;
}

static char *fetch_build_cmd(const char *url, const char *json_body) {
	size_t cmd_len;
	char *cmd;

	cmd_len = strlen(url) + strlen(json_body) + 128;
	cmd = (char *)malloc(cmd_len);

	if (!cmd) {
		fprintf(stderr, "vamp: out of memory building curl command\n");

		return NULL;
	}

	snprintf(cmd, cmd_len, "curl -sS -X POST -H 'Content-Type: application/json' --data '%s' '%s'", json_body, url);

	return cmd;
}

static char *fetch_read_stream(FILE *fp) {
	char buf[4096];
	size_t cap, len;
	char *result;

	cap = 4096;
	len = 0;
	result = (char *)malloc(cap);

	if (!result) {
		fprintf(stderr, "vamp: out of memory reading curl output\n");

		return NULL;
	}

	for (;;) {
		size_t nread;

		nread = fread(buf, 1, sizeof(buf), fp);

		if (nread == 0) break;

		if (len + nread + 1 > cap) {
			size_t newcap;
			char *tmp;

			newcap = cap * 2 + nread;
			tmp = (char *)realloc(result, newcap);

			if (!tmp) {
				fprintf(stderr, "vamp: out of memory growing curl output\n");
				free(result);

				return NULL;
			}

			result = tmp;
			cap = newcap;
		}

		memcpy(result + len, buf, nread);

		len += nread;
	}

	result[len] = '\0';

	return result;
}

/* Run curl, capture stdout into *out_body. Return 0 on success, non-zero on error. */
static int fetch_completion(const struct vamp_section *sec, const char *prompt, char **out_body) {
	const char *url;
	char *json_body, *cmd, *result;
	FILE *fp;
	int status;

	*out_body = NULL;

	if (!fetch_have_posturl(sec, &url)) return -1;

	json_body = fetch_build_json(sec, prompt);

	if (!json_body) {
		fprintf(stderr, "vamp: failed to build JSON body\n");

		return -1;
	}

	cmd = fetch_build_cmd(url, json_body);

	free(json_body);

	if (!cmd) return -1;

	fp = popen(cmd, "r");

	if (!fp) {
		fprintf(stderr, "vamp: failed to run curl\n");
		free(cmd);

		return -1;
	}

	result = fetch_read_stream(fp);
	status = pclose(fp);

	free(cmd);

	if (!result) return -1;

	if (status != 0) {
		fprintf(stderr, "vamp: curl failed with status %d\n", status);
		free(result);

		return -1;
	}

	*out_body = result;

	return 0;
}

#endif /* FETCH_H */

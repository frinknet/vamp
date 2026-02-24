/* (c) FRINKnet & Friends - MIT License */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct vamp_kv {
	char *key;
	char *value;
} vamp_kv;

struct vamp_section {
	char *name;
	struct vamp_kv *pairs;
	size_t count;
	size_t cap;
} vamp_section;

struct vamp_config {
	vamp_section *sections;
	size_t count;
	size_t cap;
} vamp_config;

static char *config_strdup(const char *s) {
	size_t n;
	char *p;

	if (!s) return NULL;

	n = strlen(s);
	p = (char *)malloc(n + 1);

	if (!p) return NULL;

	memcpy(p, s, n + 1);

	return p;
}

static char *config_strndup(const char *s, size_t n) {
	char *p;

	p = (char *)malloc(n + 1);

	if (!p) return NULL;

	memcpy(p, s, n);

	p[n] = '\0';

	return p;
}

static char *config_trim(char *s) {
	char *end;

	while (*s && isspace((unsigned char)*s)) s++;

	if (*s == '\0') return s;

	end = s + strlen(s) - 1;

	while (end > s && isspace((unsigned char)*end)) end--;

	end[1] = '\0';

	return s;
}

static void config_free_section(vamp_section *sec) {
	size_t i;

	if (!sec) return;

	free(sec->name);

	for (i = 0; i < sec->count; ++i) {
		free(sec->pairs[i].key);
		free(sec->pairs[i].value);
	}

	free(sec->pairs);

	sec->name = NULL;
	sec->pairs = NULL;
	sec->count = 0;
	sec->cap = 0;
}

static void config_init(vamp_config *cfg) {
	cfg->sections = NULL;
	cfg->count = 0;
	cfg->cap = 0;
}

static void config_free(vamp_config *cfg) {
	size_t i;

	if (!cfg) return;

	for (i = 0; i < cfg->count; ++i) config_free_section(&cfg->sections[i]);

	free(cfg->sections);

	cfg->sections = NULL;
	cfg->count = 0;
	cfg->cap = 0;
}

static vamp_section *config_find_section(vamp_config *cfg, const char *name) {
	size_t i;

	for (i = 0; i < cfg->count; ++i) {
		if (cfg->sections[i].name && strcmp(cfg->sections[i].name, name) == 0) return &cfg->sections[i];
	}

	return NULL;
}

static vamp_section *config_add_section(vamp_config *cfg, const char *name) {
	vamp_section *sec, *tmp;
	size_t newcap;

	sec = config_find_section(cfg, name);

	if (sec) return sec;

	if (cfg->count == cfg->cap) {
		newcap = cfg->cap ? cfg->cap * 2 : 4;
		tmp = (vamp_section *)realloc(cfg->sections, newcap * sizeof(vamp_section));

		if (!tmp) return NULL;

		cfg->sections = tmp;
		cfg->cap = newcap;
	}

	sec = &cfg->sections[cfg->count++];
	sec->name = config_strdup(name);
	sec->pairs = NULL;
	sec->count = 0;
	sec->cap = 0;

	if (!sec->name) return NULL;

	return sec;
}

static int config_set_kv(vamp_section *sec, const char *key, const char *value) {
	size_t i, newcap;
	vamp_kv *tmp;

	for (i = 0; i < sec->count; ++i) {
		if (strcmp(sec->pairs[i].key, key) == 0) {
			char *nv;

			nv = config_strdup(value);

			if (!nv) return -1;

			free(sec->pairs[i].value);

			sec->pairs[i].value = nv;

			return 0;
		}
	}

	if (sec->count == sec->cap) {
		newcap = sec->cap ? sec->cap * 2 : 4;
		tmp = (vamp_kv *)realloc(sec->pairs, newcap * sizeof(vamp_kv));

		if (!tmp) return -1;

		sec->pairs = tmp;
		sec->cap = newcap;
	}

	sec->pairs[sec->count].key = config_strdup(key);
	sec->pairs[sec->count].value = config_strdup(value);

	if (!sec->pairs[sec->count].key || !sec->pairs[sec->count].value) return -1;

	sec->count++;

	return 0;
}

/* Load and overlay from one file path (ignore if missing) */
static int config_load_file(vamp_config *cfg, const char *path) {
	FILE *f;
	char line[4096];
	vamp_section *current;

	f = fopen(path, "r");

	if (!f) return 0;

	current = NULL;

	while (fgets(line, sizeof(line), f)) {
		char *p, *end, *name, *eq, *key, *val;

		p = config_trim(line);

		if (*p == '\0') continue;
		if (*p == ';' || *p == '#') continue;

		if (*p == '[') {
			end = strchr(p, ']');
			if (!end) continue;

			name = config_strndup(p + 1, (size_t)(end - (p + 1)));
			if (!name) {
				fclose(f);
				return -1;
			}

			name = config_trim(name);
			current = config_add_section(cfg, name);
			free(name);

			if (!current) {
				fclose(f);
				return -1;
			}
		} else {
			eq = strchr(p, '=');

			if (!eq) continue;

			*eq = '\0';
			key = config_trim(p);
			val = config_trim(eq + 1);

			if (!current) continue;

			if (config_set_kv(current, key, val) != 0) {
				fclose(f);

				return -1;
			}
		}
	}

	fclose(f);

	return 0;
}

static vamp_section *config_get_section(vamp_config *cfg, const char *name) {
	return config_find_section(cfg, name);
}

static const char *config_get_value(const vamp_section *sec, const char *key) {
	size_t i;

	if (!sec) return NULL;

	for (i = 0; i < sec->count; ++i) {
		if (strcmp(sec->pairs[i].key, key) == 0) return sec->pairs[i].value;
	}

	return NULL;
}

#endif /* CONFIG_H */

/* (c) FRINKnet & Friends - MIT License */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "prompt.h"
#include "fetch.h"

static void die(const char *msg) {
	fprintf(stderr, "vamp: %s\n", msg);
	exit(1);
}

static int write_file_all(const char *path, const char *data) {
	FILE *f;
	size_t len;
	size_t nw;

	f = fopen(path, "wb");

	if (!f) {
		fprintf(stderr, "vamp: cannot write file '%s'\n", path);

		return -1;
	}

	len = strlen(data);
	nw = fwrite(data, 1, len, f);

	if (nw < len) {
		fprintf(stderr, "vamp: short write to file '%s'\n", path);
		fclose(f);

		return -1;
	}

	fclose(f);

	return 0;
}

static void setup_config(vamp_config *cfg, const char *mode, vamp_section **out_sec) {
	const char *home;
	vamp_section *sec;

	config_init(cfg);
	config_load_file(cfg, "/etc/vamp/vamprc");

	home = getenv("HOME");

	if (home && home[0]) {
		char path[4096];

		snprintf(path, sizeof(path), "%s/%s", home, ".vamprc");
		config_load_file(cfg, path);
	}

	sec = config_get_section(cfg, mode);

	if (!sec) {
		char buf[256];

		snprintf(buf, sizeof(buf), "missing config section [%s]", mode);
		config_free(cfg);
		die(buf);
	}

	*out_sec = sec;
}

static int handle_stdin_mode(vamp_section *sec, vamp_config *cfg) {
	char *stdin_contents, *prompt, *out;
	const char *filename;

	stdin_contents = prompt_read_all(stdin);

	if (!stdin_contents) {
		config_free(cfg);
		die("failed to read stdin");
	}

	filename = "<stdin>";
	prompt = prompt_build_prompt(sec, filename, stdin_contents);

	if (!prompt) {
		free(stdin_contents);
		config_free(cfg);
		die("failed to build prompt");
	}

	if (fetch_completion(sec, prompt, &out) != 0) {
		free(stdin_contents);
		free(prompt);
		config_free(cfg);

		return 1;
	}

	fputs(out, stdout);
	free(out);
	free(stdin_contents);
	free(prompt);
	config_free(cfg);

	return 0;
}

static int handle_file_mode(int argi, int argc, char **argv, vamp_section *sec, vamp_config *cfg) {
	for (; argi < argc; ++argi) {
		const char *filename;
		char *file_contents, *prompt, *out;

		filename = argv[argi];
		file_contents = prompt_read_file(filename);

		if (!file_contents) {
			fprintf(stderr, "vamp: cannot read file '%s'\n", filename);
			config_free(cfg);

			return 1;
		}

		prompt = prompt_build_prompt(sec, filename, file_contents);

		if (!prompt) {
			free(file_contents);
			config_free(cfg);
			die("failed to build prompt");
		}

		if (fetch_completion(sec, prompt, &out) != 0) {
			free(file_contents);
			free(prompt);
			config_free(cfg);

			return 1;
		}

		if (write_file_all(filename, out) != 0) {
			free(out);
			free(file_contents);
			free(prompt);
			config_free(cfg);

			return 1;
		}

		free(out);
		free(file_contents);
		free(prompt);
	}

	config_free(cfg);

	return 0;
}

int main(int argc, char **argv) {
	const char *mode;
	int argi;
	vamp_config cfg;
	vamp_section *sec;

	mode = "vamp";
	argi = 1;

	if (argc < 2 && isatty(STDIN_FILENO)) {
		fprintf(stderr, "usage: %s [--mode] file1 [file2 ...]\n", argv[0]);

		return 1;
	}

	if (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
		mode = argv[argi] + 2;

		argi++;

		if (argi >= argc && isatty(STDIN_FILENO)) die("no files specified");
	}

	setup_config(&cfg, mode, &sec);

	if (!isatty(STDIN_FILENO)) return handle_stdin_mode(sec, &cfg);

	return handle_file_mode(argi, argc, argv, sec, &cfg);
}

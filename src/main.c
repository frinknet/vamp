/* (c) FRINKnet & Friends - MIT License */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* isatty, STDIN_FILENO */

#include "config.h"
#include "prompt.h"

static void die(const char *msg) {
	fprintf(stderr, "vamp: %s\n", msg);
	exit(1);
}

int main(int argc, char **argv) {
	const char *mode, *home;
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

	config_init(&cfg);
	config_load_file(&cfg, "/etc/vamp/vamprc");

	home = getenv("HOME");

	if (home && home[0]) {
		char path[4096];

		snprintf(path, sizeof(path), "%s/%s", home, ".vamprc");
		config_load_file(&cfg, path);
	}

	sec = config_get_section(&cfg, mode);

	if (!sec) {
		char buf[256];

		snprintf(buf, sizeof(buf), "missing config section [%s]", mode);
		config_free(&cfg);
		die(buf);
	}

	if (!isatty(STDIN_FILENO)) {
		char *stdin_contents, *prompt;
		const char *filename;

		stdin_contents = prompt_read_all(stdin);

		if (!stdin_contents) {
			config_free(&cfg);
			die("failed to read stdin");
		}

		filename = "<stdin>";
		prompt = prompt_build_prompt(sec, filename, stdin_contents);

		if (!prompt) {
			free(stdin_contents);
			config_free(&cfg);
			die("failed to build prompt");
		}

		fputs(prompt, stdout);
		free(stdin_contents);
		free(prompt);
		config_free(&cfg);

		return 0;
	}

	for (; argi < argc; ++argi) {
		const char *filename;
		char *file_contents;
		char *prompt;

		filename = argv[argi];
		file_contents = prompt_read_file(filename);

		if (!file_contents) {
			fprintf(stderr, "vamp: cannot read file '%s'\n", filename);
			config_free(&cfg);

			return 1;
		}

		prompt = prompt_build_prompt(sec, filename, file_contents);

		if (!prompt) {
			free(file_contents);
			config_free(&cfg);
			die("failed to build prompt");
		}

		fputs(prompt, stdout);

		if (argi + 1 < argc) fputs("\n", stdout);

		free(file_contents);
		free(prompt);
	}

	config_free(&cfg);

	return 0;
}

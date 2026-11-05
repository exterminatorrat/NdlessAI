/*
 * Host driver for the on-device pipeline: one question per line on stdin,
 * one "answer<TAB>call" line per question on stdout.
 *
 * The pipeline itself lives in pipeline.c and is shared with the calculator
 * app, so what is tested here is what runs on the device.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"

int main(int argc, char **argv)
{
	static pipeline_t p;
	char line[512];
	char answer[256];
	char call[256];

	if (argc < 3) {
		fprintf(stderr, "usage: %s weights.bin <seq>\n", argv[0]);
		return 2;
	}
	if (pipeline_init(&p, argv[1], atoi(argv[2])) < 0) {
		fprintf(stderr, "cannot load %s\n", argv[1]);
		return 1;
	}

	while (fgets(line, sizeof line, stdin)) {
		size_t len = strlen(line);
		int rc;

		while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';
		if (!len)
			continue;

		rc = pipeline_answer(&p, line, answer, (int)sizeof answer,
		                     call, (int)sizeof call);
		if (rc == PIPELINE_OK)
			printf("%s\t%s\n", answer, call);
		else if (rc == PIPELINE_UNCERTAIN)
			printf("!uncertain\t%s\n", call);
		else
			printf("!error\t%s\n", call);
	}

	pipeline_free(&p);
	return 0;
}

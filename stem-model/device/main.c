/*
 * STEM helper for the TI-Nspire CX II under Ndless.
 *
 * Asks for a problem, runs the on-device pipeline (BPE -> fixed-point
 * transformer -> call -> C solver) and shows the answer. The pipeline is the
 * same code the host tests exercise, so behaviour here matches what was
 * measured off-device.
 */
#include <libndls.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline.h"

#define WEIGHTS_FILE "weights.bin"
#define SEQ 160

int main(void)
{
	static pipeline_t p;
	char path[256];
	char answer[256];
	char call[256];

	assert_ndless_rev(801);

	if (locate(WEIGHTS_FILE, path, sizeof path) != 0) {
		show_msgbox("STEM helper",
		            "weights.bin was not found.\n\n"
		            "Copy it into the same folder as this program.");
		return 1;
	}

	if (pipeline_init(&p, path, SEQ) < 0) {
		show_msgbox("STEM helper",
		            "Could not load the model.\n\n"
		            "The calculator may be low on memory.");
		return 1;
	}

	for (;;) {
		char *input = NULL;
		int len = show_msg_user_input("STEM helper", "Enter a problem:", "", &input);
		int rc;

		if (len < 0 || !input)
			break;

		rc = pipeline_answer(&p, input, answer, (int)sizeof answer,
		                     call, (int)sizeof call);
		free(input);

		if (rc == PIPELINE_OK) {
			show_msgbox("Answer", answer);
		} else if (rc == PIPELINE_UNCERTAIN) {
			show_msgbox("Not sure",
			            "I could not match the numbers in your\n"
			            "question, so I will not guess.");
		} else {
			show_msgbox("Sorry",
			            "I could not work that one out.\n\n"
			            "Try rephrasing it.");
		}
	}

	pipeline_free(&p);
	return 0;
}

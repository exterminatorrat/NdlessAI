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

#define SEQ 160

/*
 * TI's transfer software only accepts files ending in .tns, so the model
 * ships as weights.bin.tns. The plain name is still tried so the app also
 * works with a file copied by other means, such as USB mass storage or a
 * tool that does not filter by extension.
 */
static const char *const weight_names[] = {
	"weights.bin.tns",
	"weights.bin",
	NULL
};

int main(void)
{
	static pipeline_t p;
	char path[256];
	char answer[256];
	char call[256];
	int found = 0;

	assert_ndless_rev(801);

	for (int i = 0; weight_names[i] && !found; i++)
		found = locate(weight_names[i], path, sizeof path) == 0;

	if (!found) {
		show_msgbox("STEM helper",
		            "The model file was not found.\n\n"
		            "Copy weights.bin.tns into the same\n"
		            "folder as this program.");
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
			/*
			 * Show what the model produced. A malformed call usually means
			 * the compiled tokenizer does not match the one the model was
			 * trained with, and the call text is the only way to see that
			 * from the calculator.
			 */
			char detail[384];

			snprintf(detail, sizeof detail,
			         "I could not work that one out.\n\n"
			         "It produced:\n%s\n\n"
			         "saturations: %lu",
			         call, (unsigned long)fixed_sat_count);
			show_msgbox("Sorry", detail);
		}
	}

	pipeline_free(&p);
	return 0;
}

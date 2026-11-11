/*
 * STEM helper for the TI-Nspire CX II under Ndless.
 *
 * Two modes, chosen from the screen's own menu:
 *
 *   Ask  - a problem in words runs the on-device pipeline (BPE ->
 *          fixed-point transformer -> call -> C solver); an expression is
 *          answered directly, without the model. The pipeline is the same
 *          code the host tests exercise, so behaviour here matches what was
 *          measured off-device.
 *   Graph - plots y = f(x). Pure C, no model and no weights file.
 *
 * This file only finds and loads the model; the screen is runtime/ui.c and
 * the keypad is device/ui_ndless.c.
 */
#include <libndls.h>

#include "pipeline.h"
#include "ui.h"

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
	int found = 0;

	assert_ndless_rev(801);

	for (int i = 0; weight_names[i] && !found; i++)
		found = locate(weight_names[i], path, sizeof path) == 0;

	/*
	 * Without the weights file there is no model, but typed arithmetic never
	 * needed one. The screen says so on its own; the app still starts, so a
	 * student can evaluate an expression on a calculator that has nothing
	 * copied onto it yet.
	 */
	if (!found)
		return ui_run(NULL, "weights.bin.tns is not beside this program.");

	if (pipeline_init(&p, path, SEQ) < 0)
		return ui_run(NULL, "weights.bin.tns would not load; the calculator may be low on memory.");

	ui_run(&p, NULL);
	pipeline_free(&p);
	return 0;
}

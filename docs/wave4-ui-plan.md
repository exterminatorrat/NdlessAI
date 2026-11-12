# Finishing wave 4: 4.5 and 4.6

Wave 4's remaining two workstreams are the product half of the project: an
interface that is not two message boxes, and graphing that plots the problem
rather than an expression the student had to derive first.

The plan said both were unbuildable here because "no machine in this project has
the Ndless toolchain, so their code could be written but not compiled". That was
wrong in a way worth recording. The runtime path had been cross-compiled in CI
all along, and `stem-model/device/*.c` compiles for `arm926ej-s` with nothing but
the vendored SDK headers:

    arm-none-eabi-gcc -mcpu=arm926ej-s -marm -O3 -std=c11 -Wall -Wextra -Werror \
        -Indless-sdk/include -Istem-model/runtime -c stem-model/device/main.c

`crosscheck.sh` now compiles the UI sources as part of the gate, so they are
checked on every run rather than assumed. What genuinely cannot happen here is
*running* the result: a `.tns` needs Ndless's own libraries and a calculator.
So the rule for both workstreams is the one `graph.c` already follows — all logic
in `runtime/`, host-tested; the Ndless file stays a binding with no decisions in
it.

## The pattern to follow

`runtime/graph.c` is 888 lines of layout, mapping, clipping and rendering, with
`device/graph_ui.c` (85 lines) doing nothing but reading keys and blitting. Every
behaviour in 4.6 can be asserted in `runtime/test_graph.c` without a screen.

4.5 gets the same treatment: a screen model in `runtime/ui.c`, a five-line-per-
function Ndless binding in `device/ui_ndless.c`, and `runtime/test_ui.c` driving
the model with synthetic key presses and asserting on the framebuffer.

## 4.5 — the interface

### 4.5a A real screen loop

`device/main.c` today is a chain of `show_msgbox` calls. It becomes:

| Where | What |
| --- | --- |
| `runtime/screen.c/.h` | the 5x7 font (moved out of `graph.c`), `screen_text`, `screen_fill`, `screen_rect`, `screen_clear` over a `uint16_t *fb` of 320x240 |
| `runtime/ui.c/.h` | the screen model: a menu, an input line with editing, a scrollable answer pane, and a renderer |
| `device/ui_ndless.c` | `isKeyPressed` → `UI_KEY_*`, `lcd_blit` of the framebuffer |
| `device/main.c` | `ui_run()` and nothing else |

The model is a state machine over `ui_key_t`:

```c
typedef enum {
        UI_KEY_NONE, UI_KEY_ACCEPT, UI_KEY_BACK, UI_KEY_UP, UI_KEY_DOWN,
        UI_KEY_LEFT, UI_KEY_RIGHT, UI_KEY_DELETE, UI_KEY_MODE
} ui_key_t;

void ui_init(ui_t *u, pipeline_t *p, const char *missing_model_note);
ui_key_t ui_key(ui_t *u, ui_key_t k);   /* feed one key, returns what the host should do */
int  ui_render(ui_t *u, uint16_t *fb);
```

`ui_key` returns `UI_KEY_BACK` when the user has asked to leave the screen, which
is how the Ndless loop knows to stop; everything else is a redraw.

**Done when** `test_ui` proves: a long answer wraps at the pane width and scrolls
one row per key with the offset clamped at both ends; the input line accepts,
edits and deletes without touching the answer pane; a mode change reaches the
menu and back; and the framebuffer changes in the places the assertions name.

### 4.5b Error explanations

`pipeline_answer` already distinguishes three failures. The screen shows the
reason in words a student can act on, not the code:

| Code | The screen says |
| --- | --- |
| `PIPELINE_OK` | the answer, with the working above it |
| `PIPELINE_UNCERTAIN` | "some numbers here are not in the question" |
| `PIPELINE_ERR` | "I could not read that as a problem", plus the call text it tried, since a tokenizer mismatch is the usual cause and the call is the only visible symptom |
| no model loaded | arithmetic still works; word problems need `weights.bin.tns` |

**Done when** `test_ui` sets each condition through the pipeline's return codes
and asserts the pane's first line against the table.

### 4.5c Step display

`pipeline_steps` already returns the rendered working line and `check_steps.py`
already proves its arithmetic on the host. The work is presentational: the
working appears above the answer in the same pane, dimmed in the palette, with
the formula line and the substituted line separated.

**Done when** `test_ui` shows a working line for a relation that has one, and
shows the answer alone for one that does not.

### 4.5d History

A ring of the last eight question/answer pairs in RAM, recalled with one key from
the input line. No disk, no allocation: a fixed array of fixed buffers, because
this target has 64 MB and no heap worth trusting mid-session.

**Done when** `test_ui` asks three questions, recalls twice, and gets the second
and first back in that order.

### 4.5e Keypad help

Two or three pages of syntax examples reachable from the menu, rendered through
the same pane so they scroll: `integrate(x^2,0,3)`, `d/dx(x^3)`, `series(sin(x),0,5)`,
`ode(2y,0,1,1,64)`, `desolve(2y,0,1)`, `desolve(x*y,0,2)`, and the fact that
arithmetic needs no model file.

**Done when** `test_ui` opens the pages and finds each of those strings in the
rendered text rows.

## 4.6 — graphing the problem

Six features over the existing `graph.c`. The first four share one new entry
point, so the mapping is written once:

```c
/* Plots the relation a call named, sweeping argument `sweep` across a range of
 * the student's choosing. Returns 0, or a negative code when the call has no
 * plottable form. */
int graph_from_call(gstate_t *g, const char *call, int sweep, char *err, int errlen);
```

### 4.6a Words to curve

`pipeline_answer` already returns the call. `graph_from_call` reads the relation
name and its argument values from the call text, looks the relation up in
`steptable.h`, and plots the relation's own formula against one swept argument —
so "what is the kinetic energy of a 5 kg object at 3 m/s" can be shown as KE
against speed, with the student's own point marked on it.

### 4.6b The answer drawn where it belongs

The solved value appears as a labelled point on the curve it came from: a filled
circle at the swept argument's own value, with the answer text beside it. This is
the cheapest trust win in the wave — the number and the picture agree visibly.

### 4.6c Why this formula

`pipeline.c` uses a bare `argmax32` over relation tokens. It keeps the top three
and their margins instead:

```c
typedef struct {
        uint16_t token;
        int32_t margin;   /* logit gap to the winner, >= 0 */
} pipeline_alt_t;

int pipeline_alternatives(const pipeline_t *p, pipeline_alt_t *out, int max);
```

The chosen relation draws solid; the runners-up draw dimmed with their names in
the legend. A near miss becomes visible instead of silent.

### 4.6d Parameter families

The same relation at several values of one argument — 15, 30, 45 and 60 degrees
for `proj_range`, or three values of a friction coefficient — on one screen.
It is `graph_from_call` in a loop, so the cost is one more argument to the
signature above.

### 4.6e Diagram answers

Per-subject drawings that a number cannot replace:

| Relation class | The drawing |
| --- | --- |
| force relations (`friction_force`, `newton2`, `torque`) | arrows from a body, labelled and to scale |
| projectile relations (`proj_range`, `kinematic_v`) | a strobe of positions along the trajectory |
| definite integrals (`integr_poly3`) | the curve with the region between the bounds shaded |

Lives in `runtime/diagram.c` so it is host-testable like everything else; the
only device work is deciding when to show it.

### 4.6f Slope fields

`gexpr_slope_field` already returns the segments and `test_ode` already checks
each one against the evaluator. This is the renderer: map the window, draw each
segment as a short line at its angle, and trace one solution from the initial
condition.

## Gates

| Gate | Proves |
| --- | --- |
| `test_screen` | the font and the primitives, including clipping at the framebuffer edge |
| `test_ui` | wrapping, scrolling, editing, history, the three error messages, step display, keypad pages |
| `test_graph` (extended) | mapping from a call to a curve, the answer marker's position, candidate dimming, a family of curves, a slope field's segments |
| `test_diagram` | arrow lengths and directions, strobe spacing, shaded region bounds |
| `ARM cross-compile` | every device source compiles for `arm926ej-s` under `-Werror` |

## Where it ended

Both workstreams landed on one branch, and the gate rows they added are the
argument that they are real: `screen primitives`, `interface model`, `diagram
answers`, `candidate margins` and the extended `graph` row, plus `ARM
cross-compile` covering every device source and `-Werror` over the two new
runtime modules.

Two keys reach the new screens from the answer:

| Key | What it does | Where the decision lives |
| --- | --- | --- |
| TAB | toggles the subject diagram over the answer, if the relation has one | `ui_key` → `diagram_render` in `runtime/` |
| VAR | leaves the model and hands the answer's call to the plotter | `ui_key` returns `UI_KEY_GRAPH`; `device/graph_ui.c` draws it |

Any key other than TAB puts the answer back, so the picture cannot eat the
answer a student asked for, and both keys are advertised on the answer screen's
own hint line — a key nobody can find is not a feature.

4.6c is data, not a picture yet: `pipeline_alternatives` returns the top three
calls the decoder weighed with their margins, and `test_alt` checks them, but
the dimmed overlay it exists for has nowhere to go while the plotter only ever
receives the one call that won.

One thing the plan asked for is deliberately still open: the plotter's *menu*
path still prompts for `y = f(x)` through a message box, because that is the
expression workflow the device already had. The answer path is the one that
skips the formula, and it is the one 4.6 is about.

## What this still cannot prove

Nothing here proves the screens *look* right on a 320x240 calculator, because
there is no calculator. Colour choices, font crowding and touch targets are
judged by eye on hardware. What the gates can prove is that every value the
screen shows is computed by code the host tests exercise, that the layout fits
the framebuffer, and that no device source has strayed outside the API the SDK
actually provides.

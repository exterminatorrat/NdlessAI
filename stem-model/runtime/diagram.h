#ifndef DIAGRAM_H
#define DIAGRAM_H

#include <stdint.h>

/*
 * The three answers a picture gives better than a number: a force with a
 * direction, a projectile's path, and the region a definite integral names.
 *
 * The graph screen hands this module the call text the pipeline produced and a
 * 565 framebuffer. Every decision about layout, scale, sign and label lives
 * here so the host tests can assert on pixels; nothing here reads a key, a
 * timer or the model.
 *
 * The colours are part of the contract: a caller drawing a caption beside a
 * diagram needs the background to match, and a test needs to count a region
 * without guessing at a pixel value.
 */

/* 1 when this call has a diagram, 0 when it does not. */
int diagram_supports(const char *relation);

/*
 * Draws the diagram for a call of the form "CALL name arg arg ..." into a
 * 320x240 RGB565 buffer, clearing it to DIAGRAM_BG first. Returns 0, or -1
 * with a short reason in err when the relation has no diagram or its
 * arguments do not describe one.
 */
int diagram_render(const char *call, uint16_t *fb, char *err, int errlen);

#define DIAGRAM_BG    0x0000u
#define DIAGRAM_TEXT  0xFFFFu
#define DIAGRAM_AXIS  0x8410u
#define DIAGRAM_CURVE 0x07E0u
#define DIAGRAM_BODY  0x7BEFu
#define DIAGRAM_HEAD  0xFFE0u
#define DIAGRAM_MARK  0xF800u
/* The shaded region is the one colour no other element uses, because its
 * pixel count is what the integral test measures. */
#define DIAGRAM_SHADE 0x0320u

#endif /* DIAGRAM_H */

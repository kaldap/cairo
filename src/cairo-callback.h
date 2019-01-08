/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2019 Petr Kalandra
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson
 *
 * Contributor(s):
 *	Petr Kalandra <kalandrap@gmail.com>
 */

#ifndef CAIRO_CALLBACK_H
#define CAIRO_CALLBACK_H

#include "cairo.h"

#if CAIRO_HAS_CALLBACK_SURFACE

CAIRO_BEGIN_DECLS

typedef enum _cairo_callback_path_node {
	CAIRO_PATH_NODE_MOVE_TO,
	CAIRO_PATH_NODE_LINE_TO,
	CAIRO_PATH_NODE_CURVE_TO,
	CAIRO_PATH_NODE_CLOSE_PATH,
} cairo_callback_path_node_t;

typedef struct _cairo_callback_path {
	cairo_callback_path_node_t node_type;
	cairo_point_t  points[3];
	struct _cairo_callback_path * next;
} cairo_callback_path_t;

typedef struct _cairo_callback_clip {
	cairo_callback_path_t * path;
	double tolerance;
	cairo_antialias_t antialias;
	cairo_fill_rule_t fill_rule;
	struct _cairo_callback_clip * next;
} cairo_callback_clip_t;

typedef struct _cairo_callback_paint {
	cairo_operator_t op;
	const cairo_pattern_t *source;
	const cairo_callback_clip_t *clip;
} cairo_callback_paint_t;

typedef struct _cairo_callback_mask {
	cairo_operator_t op;
	const cairo_pattern_t *source;
	const cairo_pattern_t *mask;
	const cairo_callback_clip_t *clip;
} cairo_callback_mask_t;

typedef struct _cairo_callback_stroke_style {
    double line_width;
    cairo_line_cap_t line_cap;
    cairo_line_join_t line_join;
    double miter_limit;
    const double * dash;
    unsigned int num_dashes;
    double dash_offset;
} cairo_callback_stroke_style_t;

typedef struct _cairo_callback_stroke {
	double tolerance;
	cairo_operator_t op;
	cairo_antialias_t antialias;
	cairo_callback_stroke_style_t style;
	const cairo_pattern_t * pattern;
	const cairo_callback_path_t * path;
	const cairo_callback_clip_t * clip;
	const cairo_matrix_t		* matrix;
	const cairo_matrix_t		* inv_matrix;
} cairo_callback_stroke_t;

typedef struct _cairo_callback_fill {
	double tolerance;
	cairo_operator_t op;
	cairo_fill_rule_t fill_rule;
	cairo_antialias_t antialias;
	const cairo_pattern_t * pattern;
	const cairo_callback_path_t * path;
	const cairo_callback_clip_t * clip;
	const cairo_glyph_t * glyph;
} cairo_callback_fill_t;

typedef void (*cairo_callback_mask_fcn_t)(const cairo_surface_t * source, const cairo_callback_mask_t * mask);
typedef void (*cairo_callback_paint_fcn_t)(const cairo_surface_t * source, const cairo_callback_paint_t * paint);
typedef void (*cairo_callback_fill_fcn_t)(const cairo_surface_t * source, const cairo_callback_fill_t * fill);
typedef void (*cairo_callback_stroke_fcn_t)(const cairo_surface_t * source, const cairo_callback_stroke_t * stroke);

typedef struct _cairo_callback_surface_interface {
	cairo_callback_mask_fcn_t mask;
	cairo_callback_paint_fcn_t paint;
	cairo_callback_fill_fcn_t fill;
	cairo_callback_stroke_fcn_t stroke;
} cairo_callback_surface_interface_t;

cairo_public cairo_surface_t *
cairo_callback_surface_create (
	const cairo_callback_surface_interface_t * interface,
	cairo_content_t	content
	);

CAIRO_END_DECLS

#else  /*CAIRO_HAS_CALLBACK_SURFACE*/
# error Cairo was not compiled with support for the callback backend
#endif /*CAIRO_HAS_CALLBACK_SURFACE*/

#endif /*CAIRO_CALLBACK_H*/

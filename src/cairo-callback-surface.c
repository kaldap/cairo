/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2019 Petr Kalandra
 * 
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
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributor(s):
 *	Petr Kalandra <kalandrap@gmail.com>
 *
 * Based on cairo-xml-surface.c by:
 *  Chris Wilson <chris@chris-wilson.co.uk>
 */

/* This surface is intended to report all significant drawing operations to the
 * upper layer by means of the callback functions. It is intended to be used
 * by debuggers and users requiring their custom drawing backend.
 */

#include "cairoint.h"

#include "cairo-callback.h"

#include "cairo-clip-private.h"
#include "cairo-device-private.h"
#include "cairo-default-context-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"
#include "cairo-recording-surface-inline.h"

#define static cairo_warn static

static const cairo_surface_backend_t _cairo_callback_surface_backend;

typedef struct _cairo_callback_surface {
    cairo_surface_t base;
	
	cairo_callback_surface_interface_t iface;	
} cairo_callback_surface_t;

/* Path dumping */
struct _path_iterator {
	cairo_callback_path_t * first;
	cairo_callback_path_t * prev;
};

static cairo_status_t
_cairo_callback_move_to (void *closure,
		    const cairo_point_t *p1)
{
	cairo_callback_path_t * path = _cairo_malloc(sizeof(cairo_callback_path_t));
	if (path == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	
	path->node_type = CAIRO_PATH_NODE_MOVE_TO;
	path->points[0] = *p1;
	path->next = NULL;
	
	if (((struct _path_iterator *)closure)->prev != NULL)
		((struct _path_iterator *)closure)->prev->next = path;
	else
		((struct _path_iterator *)closure)->first = path;
	((struct _path_iterator *)closure)->prev = path;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_callback_line_to (void *closure,
		    const cairo_point_t *p1)
{
	cairo_callback_path_t * path = _cairo_malloc(sizeof(cairo_callback_path_t));
	if (path == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	
	path->node_type = CAIRO_PATH_NODE_LINE_TO;
	path->points[0] = *p1;
	path->next = NULL;
	
	if (((struct _path_iterator *)closure)->prev != NULL)
		((struct _path_iterator *)closure)->prev->next = path;
	else
		((struct _path_iterator *)closure)->first = path;
	((struct _path_iterator *)closure)->prev = path;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_callback_curve_to (void *closure,
		     const cairo_point_t *p1,
		     const cairo_point_t *p2,
		     const cairo_point_t *p3)
{
	cairo_callback_path_t * path = _cairo_malloc(sizeof(cairo_callback_path_t));
	if (path == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	
	path->node_type = CAIRO_PATH_NODE_CURVE_TO;
	path->points[0] = *p1;
	path->points[1] = *p2;
	path->points[2] = *p3;
	path->next = NULL;
	
	if (((struct _path_iterator *)closure)->prev != NULL)
		((struct _path_iterator *)closure)->prev->next = path;
	else
		((struct _path_iterator *)closure)->first = path;
	((struct _path_iterator *)closure)->prev = path;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_callback_close_path (void *closure)
{
	cairo_callback_path_t * path = _cairo_malloc(sizeof(cairo_callback_path_t));
	if (path == NULL)
		return CAIRO_STATUS_NO_MEMORY;
	
	path->node_type = CAIRO_PATH_NODE_CLOSE_PATH;
	path->next = NULL;
	
	if (((struct _path_iterator *)closure)->prev != NULL)
		((struct _path_iterator *)closure)->prev->next = path;
	else
		((struct _path_iterator *)closure)->first = path;
	((struct _path_iterator *)closure)->prev = path;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_callback_destroy_path (cairo_callback_path_t * path)
{
	for (cairo_callback_path_t * next; path != NULL; path = next)
	{
		next = path->next;
		free(path);
	}	
}

static cairo_callback_path_t *
_cairo_callback_generate_path (const cairo_path_fixed_t * path)
{
    cairo_status_t status;
	struct _path_iterator it = { NULL, NULL };
	
    status = _cairo_path_fixed_interpret (path,
					_cairo_callback_move_to,
					_cairo_callback_line_to,
					_cairo_callback_curve_to,
					_cairo_callback_close_path,
					&it);
	
	if (status == CAIRO_STATUS_SUCCESS)
		return it->first;

	_cairo_callback_destroy_path(it->first);
	assert (status == CAIRO_STATUS_SUCCESS); // Just to fail
	return NULL;
}
			   
/* Backend Operations */
static cairo_surface_t *
_cairo_callback_surface_create_similar (void * abstract_surface,
				   cairo_content_t	 content,
				   int			 width,
				   int			 height)
{
    return cairo_recording_surface_create (content, NULL);
}

static cairo_bool_t
_cairo_callback_surface_get_extents (void * abstract_surface,
				cairo_rectangle_int_t *rectangle)
{
	return FALSE;
}

static cairo_int_status_t
_cairo_callback_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  const cairo_clip_t	*clip)
{
    cairo_callback_surface_t *surface = abstract_surface;
	cairo_callback_paint_t paint;
	
	if (surface->iface.paint == NULL)
		return CAIRO_INT_STATUS_SUCCESS;
	
	paint.op = op;
	paint.source = source_pat;
	paint.clip = NULL; /* ToDo: Clipping */
	
	surface->iface.paint(surface, &paint);
	return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_callback_surface_mask (void * abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source_pat,
			 const cairo_pattern_t	*mask_pat,
			 const cairo_clip_t	*clip)
{
    cairo_callback_surface_t *surface = abstract_surface;
	cairo_callback_mask_t mask;
	
	if (surface->iface.mask == NULL)
		return CAIRO_INT_STATUS_SUCCESS;
	
	mask.op = op;
	mask.source = source_pat;
	mask.mask = mask_pat;
	mask.clip = NULL; /* ToDo: Clipping */
	
	surface->iface.mask(surface, &mask);
	return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_callback_surface_stroke (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_path_fixed_t		*path,
			   const cairo_stroke_style_t		*style,
			   const cairo_matrix_t		*ctm,
			   const cairo_matrix_t		*ctm_inverse,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   const cairo_clip_t		*clip)
{
    cairo_callback_surface_t *surface = abstract_surface;
	cairo_callback_stroke_t stroke;
	
	if (surface->iface.stroke == NULL || path == NULL)
		return CAIRO_INT_STATUS_SUCCESS;
	
	stroke.style.line_width = style->line_width;
	stroke.style.line_cap = style->line_cap;
	stroke.style.line_join = style->line_join;
	stroke.style.miter_limit = style->miter_limit;
	stroke.style.dash_offset = style->dash_offset;	
	stroke.style.num_dashes = style->num_dashes;
	stroke.style.dash = NULL;
	if (style->num_dashes) {
		stroke.style.dash = (double *)malloc(stroke.style.num_dashes * sizeof(double));
		if (stroke.style.dash == NULL) 
			return CAIRO_INT_STATUS_NO_MEMORY;
		memcpy(stroke.style.dash, style->dash, stroke.style.num_dashes * sizeof(double));
	}
	
	stroke.tolerance = tolerance;
	stroke.op = op;
	stroke.fill_rule = fill_rule;
	stroke.antialias = antialias;
	stroke.pattern = source;
	stroke.path = _cairo_callback_generate_path(path);	
	stroke.matrix = ctm;
	stroke.inv_matrix = ctm_inverse;
	stroke.clip = NULL; /* ToDo: Clipping */
		
	surface->iface.stroke(surface, &stroke);
	
	if (stroke.path != NULL)
		_cairo_callback_destroy_path(stroke.path);
	
	if (stroke.style.dash != NULL)
		free(stroke.style.dash);
	
	return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_callback_surface_fill_int (cairo_callback_surface_t *surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_path_fixed_t * path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 const cairo_clip_t	*clip,
			 const cairo_glyph_t * glyph)
{    
	cairo_callback_fill_t fill;
	
	if (surface->iface.fill == NULL || path == NULL)
		return CAIRO_INT_STATUS_SUCCESS;
	
	fill.tolerance = tolerance;
	fill.op = op;
	fill.fill_rule = fill_rule;
	fill.antialias = antialias;
	fill.pattern = source;
	fill.path = _cairo_callback_generate_path(path);
	fill.glyph = glyph;
	fill.clip = NULL; /* ToDo: Clipping */
	
	surface->iface.fill(surface, &fill);
	
	if (fill.path != NULL)
		_cairo_callback_destroy_path(fill.path);
	
	return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_callback_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_path_fixed_t * path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 const cairo_clip_t	*clip)
{
	cairo_callback_surface_t *surface = abstract_surface;
	return _cairo_callback_surface_fill_int (
			surface,
			op,
			source,
			path,
			fill_rule,
			tolerance,
			antialias,
			clip,
			NULL	
		);
}				 

static cairo_int_status_t
_cairo_callback_surface_emit_outline_glyph (cairo_callback_surface_t *surface,
				cairo_scaled_font_t	*scaled_font,
				cairo_glyph_t	    *glyph,
				cairo_operator_t	     op,
				const cairo_clip_t       *clip,
				const cairo_pattern_t    *source)
{
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_int_status_t status;

    status = _cairo_scaled_glyph_lookup (scaled_font,
					 glyph->index,
					 CAIRO_SCALED_GLYPH_INFO_METRICS|
					 CAIRO_SCALED_GLYPH_INFO_PATH,
					 &scaled_glyph);
    if (unlikely (status))
		return status;

	return _cairo_callback_surface_fill_int (
			surface,
			op,
			source,
			scaled_glyph->path,
			CAIRO_FILL_RULE_WINDING,
			0.1,
			CAIRO_ANTIALIAS_DEFAULT,
			clip,
			glyph	
		);
}
				
static cairo_int_status_t
_cairo_callback_surface_emit_bitmap_glyph (cairo_callback_surface_t *surface,
				cairo_scaled_font_t	*scaled_font,
				cairo_glyph_t	    *glyph,
				cairo_operator_t	     op,
				const cairo_clip_t       *clip,
				const cairo_pattern_t    *source)
{
	/* ToDo: Bitmap glyph support */
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_callback_surface_glyphs (void			    *abstract_surface,
			   cairo_operator_t	     op,
			   const cairo_pattern_t    *source,
			   cairo_glyph_t	    *glyphs,
			   int			     num_glyphs,
			   cairo_scaled_font_t	    *scaled_font,
			   const cairo_clip_t       *clip)
{
    cairo_callback_surface_t *surface = abstract_surface;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    int i;
	   
	_cairo_scaled_font_freeze_cache (font_subset->scaled_font);
    for (i = 0; i < num_glyphs; i++) {
	    status = _cairo_callback_surface_emit_outline_glyph (surface, scaled_font, &glyphs[i], source, clip, op);							  
		if (status == CAIRO_INT_STATUS_UNSUPPORTED)
			status = _cairo_callback_surface_emit_bitmap_glyph (surface, scaled_font, &glyphs[i], source, clip, op);
		
		if (unlikely (status))
			break;
    }
	_cairo_scaled_font_thaw_cache (font_subset->scaled_font);

    return status;
}

static const cairo_surface_backend_t
_cairo_callback_surface_backend = {
    CAIRO_SURFACE_TYPE_CALLBACK,
    NULL,

    _cairo_default_context_create,

    _cairo_callback_surface_create_similar,
    NULL, /* create_similar_image */
    NULL, /* map_to_image */
    NULL, /* unmap_image */

    _cairo_surface_default_source,
    NULL, /* acquire source image */
    NULL, /* release source image */
    NULL, /* snapshot */

    NULL, /* copy page */
    NULL, /* show page */

    _cairo_callback_surface_get_extents,
    NULL, /* get_font_options */

    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */

    _cairo_callback_surface_paint,
    _cairo_callback_surface_mask,
    _cairo_callback_surface_stroke,
    _cairo_callback_surface_fill,
    NULL, /* fill_stroke */
    _cairo_callback_surface_glyphs,
};

static cairo_surface_t *
_cairo_callback_surface_create_internal (
	const cairo_callback_surface_interface_t * interface,
	cairo_content_t	content)
{
    cairo_callback_surface_t *surface;

    surface = _cairo_malloc (sizeof (cairo_callback_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_callback_surface_backend,
			 NULL,  /* device */
			 content,
			 TRUE); /* is_vector */

	if (interface != NULL)
		memcpy(&surface->iface, interface, sizeof(cairo_callback_surface_interface_t));
	else
		memset(&surface->iface, 0, sizeof(cairo_callback_surface_interface_t));
	
    return &surface->base;
}

cairo_surface_t *
cairo_callback_surface_create (
	const cairo_callback_surface_interface_t * interface,
	cairo_content_t content)
{
    return _cairo_callback_surface_create_internal (interface, content);
}

/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_brush_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_BRUSH_TYPES_H__
#define __DNA_BRUSH_TYPES_H__


#include "DNA_ID.h"
#include "DNA_texture_types.h" /* for MTex */
#include "DNA_curve_types.h"

//#ifndef MAX_MTEX // XXX Not used?
//#define MAX_MTEX	18
//#endif

struct CurveMapping;
struct MTex;
struct Image;
struct AnimData;
struct Image;

typedef struct BrushClone {
	struct Image *image;    /* image for clone tool */
	float offset[2];        /* offset of clone image from canvas */
	float alpha, pad;       /* transparency for drawing of clone image */
} BrushClone;

typedef struct Brush {
	ID id;

	struct BrushClone clone;
	struct CurveMapping *curve; /* falloff curve */
	struct MTex mtex;
	struct MTex mask_mtex;

	struct Brush *toggle_brush;

	struct ImBuf *icon_imbuf;
	PreviewImage *preview;
	struct ColorBand *gradient;	/* color gradient */
	struct PaintCurve *paint_curve;

	char icon_filepath[1024]; /* 1024 = FILE_MAX */

	float normal_weight;
	float rake_factor;  /* rake actual data (not texture), used for sculpt */

	short blend;        /* blend mode */
	short ob_mode;      /* eObjectMode: to see if the brush is compatible, use for display only. */
	float weight;       /* brush weight */
	int size;           /* brush diameter */
	int flag;           /* general purpose flag */
	int mask_pressure;  /* pressure influence for mask */
	float jitter;       /* jitter the position of the brush */
	int jitter_absolute;	/* absolute jitter in pixels */
	int overlay_flags;
	int spacing;        /* spacing of paint operations */
	int smooth_stroke_radius;   /* turning radius (in pixels) for smooth stroke */
	float smooth_stroke_factor; /* higher values limit fast changes in the stroke direction */
	float rate;         /* paint operations / second (airbrush) */

	float rgb[3];           /* color */
	float alpha;            /* opacity */

	float secondary_rgb[3]; /* background color */

	int sculpt_plane;       /* the direction of movement for sculpt vertices */

	float plane_offset;     /* offset for plane brushes (clay, flatten, fill, scrape) */

	int gradient_spacing;
	char gradient_stroke_mode; /* source for stroke color gradient application */
	char gradient_fill_mode;   /* source for fill tool color gradient application */

	char pad;
	char falloff_shape;     /* Projection shape (sphere, circle) */
	float falloff_angle;

	char sculpt_tool;       /* active sculpt tool */
	char vertexpaint_tool;  /* active vertex/weight paint blend mode (poorly named) */
	char imagepaint_tool;   /* active image paint tool */
	char mask_tool;         /* enum eBrushMaskTool, only used if sculpt_tool is SCULPT_TOOL_MASK */

	float autosmooth_factor;

	float crease_pinch_factor;

	float plane_trim;
	float height;           /* affectable height of brush (layer height for layer tool, i.e.) */

	float texture_sample_bias;

	/* overlay */
	int texture_overlay_alpha;
	int mask_overlay_alpha;
	int cursor_overlay_alpha;

	float unprojected_radius;

	/* soften/sharpen */
	float sharp_threshold;
	int blur_kernel_radius;
	int blur_mode;

	/* fill tool */
	float fill_threshold;

	float add_col[3];
	float sub_col[3];

	float stencil_pos[2];
	float stencil_dimension[2];

	float mask_stencil_pos[2];
	float mask_stencil_dimension[2];

	/* grease pencil drawing brush data */
	short thickness;          /* thickness to apply to strokes */
	short gp_flag;            /* internal grease pencil drawing flags */
	float draw_smoothfac;     /* amount of smoothing to apply to newly created strokes */
	short draw_smoothlvl;     /* number of times to apply smooth factor to new strokes */
	short draw_subdivide;     /* number of times to subdivide new strokes */

	float draw_sensitivity;   /* amount of sensivity to apply to newly created strokes */
	float draw_strength;      /* amount of alpha strength to apply to newly created strokes */
	float draw_jitter;        /* amount of jitter to apply to newly created strokes */
	float draw_angle;         /* angle when the brush has full thickness */
	float draw_angle_factor;  /* factor to apply when angle change (only 90 degrees) */
	float draw_random_press;  /* factor of randomness for pressure */
	float draw_random_strength;  /* factor of strength for strength */
	float draw_random_sub;    /* factor of randomness for subdivision */
	char pad2[4];

	struct CurveMapping *cur_sensitivity;
	struct CurveMapping *cur_strength;
	struct CurveMapping *cur_jitter;

	float gp_thick_smoothfac; /* amount of thickness smoothing to apply to newly created strokes */
	short gp_thick_smoothlvl; /* number of times to apply thickness smooth factor to new strokes */

	short gp_fill_leak;       /* number of pixel to consider the leak is too small (x 2) */
	float gp_fill_threshold;  /* factor for transparency */
	int   gp_fill_simplylvl;  /* number of simplify steps */
	int   gp_fill_draw_mode;  /* type of control lines drawing mode */
	int   gp_icon_id;         /* icon identifier */

	int   gp_lazy_radius;     /* distance to last point to create new point */
	float gp_lazy_factor;     /* factor of smooth */

	float gp_uv_random;       /* random factor for UV rotation */
	int   gp_input_samples;   /* maximum distance before generate new point for very fast mouse movements */
	int   gp_brush_type;      /* type of brush (draw, fill, erase, etc..) */
	int   gp_eraser_mode;     /* soft, hard or stroke */
	float gp_active_smooth;   /* smooth while drawing factor */
	char pad_[4];

	/* optional link of palette and color to replace default color in context */	
	struct Palette *palette;            /* palette linked */
	char           colorname[64];       /* color name */
} Brush;

/* Brush->gp_flag */
typedef enum eGPDbrush_Flag {
	/* brush use pressure */
	GP_BRUSH_USE_PRESSURE = (1 << 0),
	/* brush use pressure for alpha factor */
	GP_BRUSH_USE_STENGTH_PRESSURE = (1 << 1),
	/* brush use pressure for alpha factor */
	GP_BRUSH_USE_JITTER_PRESSURE = (1 << 2),
	/* enable screen cursor */
	GP_BRUSH_ENABLE_CURSOR = (1 << 5),
	/* fill hide transparent */
	GP_BRUSH_FILL_HIDE = (1 << 6),
	/* show fill help lines */
	GP_BRUSH_FILL_SHOW_HELPLINES = (1 << 7),
	/* lazy mouse */
	GP_BRUSH_STABILIZE_MOUSE = (1 << 8),
	/* lazy mouse override (internal only) */
	GP_BRUSH_STABILIZE_MOUSE_TEMP = (1 << 9),
	/* default eraser brush for quick switch */
	GP_BRUSH_DEFAULT_ERASER = (1 << 10),
	/* settings group */
	GP_BRUSH_GROUP_SETTINGS = (1 << 11),
	/* Random settings group */
	GP_BRUSH_GROUP_RANDOM = (1 << 12)
} eGPDbrush_Flag;

/* Brush->gp_fill_draw_mode */
typedef enum eGP_FillDrawModes {
	GP_FILL_DMODE_BOTH = 0,
	GP_FILL_DMODE_STROKE = 1,
	GP_FILL_DMODE_CONTROL = 2,
} eGP_FillDrawModes;

/* Brush->brush type */
typedef enum eGP_BrushType {
	GP_BRUSH_TYPE_DRAW = 0,
	GP_BRUSH_TYPE_FILL = 1,
	GP_BRUSH_TYPE_ERASE = 2,
} eGP_BrushType;

/* Brush->gp_eraser_mode */
typedef enum eGP_BrushEraserMode {
	GP_BRUSH_ERASER_SOFT = 0,
	GP_BRUSH_ERASER_HARD = 1,
	GP_BRUSH_ERASER_STROKE = 2,
} eGP_BrushEraserMode;

/* default brush icons */
typedef enum eGP_BrushIcons {
	GPBRUSH_PENCIL = 1,
	GPBRUSH_PEN = 2,
	GPBRUSH_INK = 3,
	GPBRUSH_INKNOISE = 4,
	GPBRUSH_BLOCK = 5,
	GPBRUSH_MARKER = 6,
	GPBRUSH_FILL = 7,
	GPBRUSH_ERASE_SOFT = 8,
	GPBRUSH_ERASE_HARD = 9,
	GPBRUSH_ERASE_STROKE = 10
} eGP_BrushIcons;

typedef struct PaletteColor {
	struct PaletteColor *next, *prev;
	struct Image *sima;      /* Texture image for strokes */
	struct Image *ima;       /* Texture image for filling */
	float rgb[4];            /* color for paint and strokes (alpha included) */
	float fill[4];           /* color that should be used for drawing "fills" for strokes (alpha included) */
	float scolor[4];         /* secondary color used for gradients and other stuff */
	char info[64];           /* color name. Must be unique. */
	float value;             /* sculpt/weight */
	short flag;              /* settings for palette color */
	short index;             /* custom index for passes */
	short stroke_style;      /* style for drawing strokes (used to select shader type) */
	short fill_style;        /* style for filling areas (used to select shader type) */
	float mix_factor;        /* factor used to define shader behavior (several uses) */
	float g_angle;           /* angle used for gradients orientation */
	float g_radius;          /* radius for radial gradients */
	float g_boxsize;         /* cheesboard size */
	float g_scale[2];        /* uv coordinates scale */
	float g_shift[2];        /* factor to shift filling in 2d space */
	float t_angle;           /* angle used for texture orientation */
	float t_scale[2];        /* texture scale (separated of uv scale) */
	float t_offset[2];       /* factor to shift texture in 2d space */
	float t_opacity;         /* texture opacity */
	float t_pixsize;         /* pixel size for uv along the stroke */
	int mode;                /* drawing mode (line or dots) */
	char pad[4];
} PaletteColor;

/* PaletteColor->flag (mainly used by grease pencil) */
typedef enum ePaletteColor_Flag {
	/* don't display color */
	PAC_COLOR_HIDE = (1 << 1),
	/* protected from further editing */
	PAC_COLOR_LOCKED = (1 << 2),
	/* do onion skinning */
	PAC_COLOR_ONIONSKIN = (1 << 3),
	/* clamp texture */
	PAC_COLOR_TEX_CLAMP = (1 << 4),
	/* mix texture */
	PAC_COLOR_TEX_MIX = (1 << 5),
	/* Flip fill colors */
	PAC_COLOR_FLIP_FILL = (1 << 6),
	/* Stroke use Dots */
	PAC_COLOR_DOT = (1 << 7), /* deprecated (only for old files) */
	/* Texture is a pattern */
	PAC_COLOR_PATTERN = (1 << 8)
} ePaletteColor_Flag;

typedef enum ePaletteColor_Mode {
	PAC_MODE_LINE = 0, /* line */
	PAC_MODE_DOTS = 1, /* dots */
	PAC_MODE_BOX  = 2, /* rectangles */
} ePaletteColor_Mode;

typedef struct Palette {
	ID id;
	struct AnimData *adt;   /* animation data - for animating draw settings */

	/* pointer to individual colours */
	ListBase colors;

	int active_color;
	int flag;
} Palette;

/* Palette->flag */
typedef enum ePalette_Flag {
	/* in Action Editor, show as expanded channel */
	PALETTE_DATA_EXPAND = (1 << 1)
} ePalette_Flag;

typedef struct PaintCurvePoint {
	BezTriple bez; /* bezier handle */
	float pressure; /* pressure on that point */
} PaintCurvePoint;

typedef struct PaintCurve {
	ID id;
	PaintCurvePoint *points; /* points of curve */
	int tot_points;
	int add_index; /* index where next point will be added */
} PaintCurve;

/* Brush.gradient_source */
typedef enum eBrushGradientSourceStroke {
	BRUSH_GRADIENT_PRESSURE = 0, /* gradient from pressure */
	BRUSH_GRADIENT_SPACING_REPEAT = 1, /* gradient from spacing */
	BRUSH_GRADIENT_SPACING_CLAMP = 2 /* gradient from spacing */
} eBrushGradientSourceStroke;

typedef enum eBrushGradientSourceFill {
	BRUSH_GRADIENT_LINEAR = 0, /* gradient from pressure */
	BRUSH_GRADIENT_RADIAL = 1 /* gradient from spacing */
} eBrushGradientSourceFill;

/* Brush.flag */
typedef enum eBrushFlags {
	BRUSH_AIRBRUSH = (1 << 0),
	BRUSH_FLAG_DEPRECATED_1 = (1 << 1),
	BRUSH_ALPHA_PRESSURE = (1 << 2),
	BRUSH_SIZE_PRESSURE = (1 << 3),
	BRUSH_JITTER_PRESSURE = (1 << 4),
	BRUSH_SPACING_PRESSURE = (1 << 5),
	BRUSH_FLAG_DEPRECATED_2 = (1 << 6),
	BRUSH_FLAG_DEPRECATED_3 = (1 << 7),
	BRUSH_ANCHORED = (1 << 8),
	BRUSH_DIR_IN = (1 << 9),
	BRUSH_SPACE = (1 << 10),
	BRUSH_SMOOTH_STROKE = (1 << 11),
	BRUSH_PERSISTENT = (1 << 12),
	BRUSH_ACCUMULATE = (1 << 13),
	BRUSH_LOCK_ALPHA = (1 << 14),
	BRUSH_ORIGINAL_NORMAL = (1 << 15),
	BRUSH_OFFSET_PRESSURE = (1 << 16),
	BRUSH_FLAG_DEPRECATED_4 = (1 << 17),
	BRUSH_SPACE_ATTEN = (1 << 18),
	BRUSH_ADAPTIVE_SPACE = (1 << 19),
	BRUSH_LOCK_SIZE = (1 << 20),
	BRUSH_USE_GRADIENT = (1 << 21),
	BRUSH_EDGE_TO_EDGE = (1 << 22),
	BRUSH_DRAG_DOT = (1 << 23),
	BRUSH_INVERSE_SMOOTH_PRESSURE = (1 << 24),
	BRUSH_FRONTFACE_FALLOFF = (1 << 25),
	BRUSH_PLANE_TRIM = (1 << 26),
	BRUSH_FRONTFACE = (1 << 27),
	BRUSH_CUSTOM_ICON = (1 << 28),
	BRUSH_LINE = (1 << 29),
	BRUSH_ABSOLUTE_JITTER = (1 << 30),
	BRUSH_CURVE = (1u << 31)
} eBrushFlags;

typedef enum {
	BRUSH_MASK_PRESSURE_RAMP = (1 << 1),
	BRUSH_MASK_PRESSURE_CUTOFF = (1 << 2)
} BrushMaskPressureFlags;

/* Brush.overlay_flags */
typedef enum eOverlayFlags {
	BRUSH_OVERLAY_CURSOR = (1),
	BRUSH_OVERLAY_PRIMARY = (1 << 1),
	BRUSH_OVERLAY_SECONDARY = (1 << 2),
	BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE = (1 << 3),
	BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE = (1 << 4),
	BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE = (1 << 5)
} eOverlayFlags;

#define BRUSH_OVERLAY_OVERRIDE_MASK (BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE | \
									 BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE | \
									 BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE)

/* Brush.sculpt_tool */
typedef enum eBrushSculptTool {
	SCULPT_TOOL_DRAW = 1,
	SCULPT_TOOL_SMOOTH = 2,
	SCULPT_TOOL_PINCH = 3,
	SCULPT_TOOL_INFLATE = 4,
	SCULPT_TOOL_GRAB = 5,
	SCULPT_TOOL_LAYER = 6,
	SCULPT_TOOL_FLATTEN = 7,
	SCULPT_TOOL_CLAY = 8,
	SCULPT_TOOL_FILL = 9,
	SCULPT_TOOL_SCRAPE = 10,
	SCULPT_TOOL_NUDGE = 11,
	SCULPT_TOOL_THUMB = 12,
	SCULPT_TOOL_SNAKE_HOOK = 13,
	SCULPT_TOOL_ROTATE = 14,
	SCULPT_TOOL_SIMPLIFY = 15,
	SCULPT_TOOL_CREASE = 16,
	SCULPT_TOOL_BLOB = 17,
	SCULPT_TOOL_CLAY_STRIPS = 18,
	SCULPT_TOOL_MASK = 19
} eBrushSculptTool;

/** When #BRUSH_ACCUMULATE is used */
#define SCULPT_TOOL_HAS_ACCUMULATE(t) ELEM(t, \
	SCULPT_TOOL_DRAW, \
	SCULPT_TOOL_CREASE, \
	SCULPT_TOOL_BLOB, \
	SCULPT_TOOL_LAYER, \
	SCULPT_TOOL_INFLATE, \
	SCULPT_TOOL_CLAY, \
	SCULPT_TOOL_CLAY_STRIPS, \
	SCULPT_TOOL_ROTATE, \
	SCULPT_TOOL_FLATTEN \
	)

#define SCULPT_TOOL_HAS_NORMAL_WEIGHT(t) ELEM(t, \
	SCULPT_TOOL_GRAB, \
	SCULPT_TOOL_SNAKE_HOOK \
	)

#define SCULPT_TOOL_HAS_RAKE(t) ELEM(t, \
	SCULPT_TOOL_SNAKE_HOOK \
	)

#define SCULPT_TOOL_HAS_DYNTOPO(t) (ELEM(t, \
	/* These brushes, as currently coded, cannot support dynamic topology */ \
	SCULPT_TOOL_GRAB, \
	SCULPT_TOOL_ROTATE, \
	SCULPT_TOOL_THUMB, \
	SCULPT_TOOL_LAYER, \
	\
	/* These brushes could handle dynamic topology, but user feedback indicates it's better not to */ \
	SCULPT_TOOL_SMOOTH, \
	SCULPT_TOOL_MASK \
	) == 0)

/* ImagePaintSettings.tool */
typedef enum eBrushImagePaintTool {
	PAINT_TOOL_DRAW = 0,
	PAINT_TOOL_SOFTEN = 1,
	PAINT_TOOL_SMEAR = 2,
	PAINT_TOOL_CLONE = 3,
	PAINT_TOOL_FILL = 4,
	PAINT_TOOL_MASK = 5
} eBrushImagePaintTool;

/* direction that the brush displaces along */
enum {
	SCULPT_DISP_DIR_AREA = 0,
	SCULPT_DISP_DIR_VIEW = 1,
	SCULPT_DISP_DIR_X = 2,
	SCULPT_DISP_DIR_Y = 3,
	SCULPT_DISP_DIR_Z = 4
};

enum {
	PAINT_BLEND_MIX = 0,
	PAINT_BLEND_ADD = 1,
	PAINT_BLEND_SUB = 2,
	PAINT_BLEND_MUL = 3,
	PAINT_BLEND_BLUR = 4,
	PAINT_BLEND_LIGHTEN = 5,
	PAINT_BLEND_DARKEN = 6,
	PAINT_BLEND_AVERAGE = 7,
	PAINT_BLEND_SMEAR = 8,
	PAINT_BLEND_COLORDODGE = 9,
	PAINT_BLEND_DIFFERENCE = 10,
	PAINT_BLEND_SCREEN = 11,
	PAINT_BLEND_HARDLIGHT = 12,
	PAINT_BLEND_OVERLAY = 13,
	PAINT_BLEND_SOFTLIGHT = 14,
	PAINT_BLEND_EXCLUSION = 15,
	PAINT_BLEND_LUMINOCITY = 16,
	PAINT_BLEND_SATURATION = 17,
	PAINT_BLEND_HUE = 18,
	PAINT_BLEND_ALPHA_SUB = 19,
	PAINT_BLEND_ALPHA_ADD = 20,
};

typedef enum {
	BRUSH_MASK_DRAW = 0,
	BRUSH_MASK_SMOOTH = 1
} BrushMaskTool;

/* blur kernel types, Brush.blur_mode */
typedef enum eBlurKernelType {
	KERNEL_GAUSSIAN,
	KERNEL_BOX
} eBlurKernelType;

/* Brush.falloff_shape */
enum {
	PAINT_FALLOFF_SHAPE_SPHERE = 0,
	PAINT_FALLOFF_SHAPE_TUBE = 1,
};

#define MAX_BRUSH_PIXEL_RADIUS 500
#define GP_MAX_BRUSH_PIXEL_RADIUS 1000

/* Grease Pencil Stroke styles */
#define STROKE_STYLE_SOLID	0
#define STROKE_STYLE_TEXTURE 1

/* Grease Pencil Fill styles */
#define FILL_STYLE_SOLID	0
#define FILL_STYLE_GRADIENT	1
#define FILL_STYLE_RADIAL	2
#define FILL_STYLE_CHESSBOARD 3
#define FILL_STYLE_TEXTURE 4
#define FILL_STYLE_PATTERN 5

#endif  /* __DNA_BRUSH_TYPES_H__ */

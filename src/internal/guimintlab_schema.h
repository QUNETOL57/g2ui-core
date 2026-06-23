#pragma once

/**
 * GuiMintLab schema mirror for C runtime.
 *
 * This file MUST match `ui-ir/src/schema.ts`. If the IR version
 * bumps, bump `GML_IR_SCHEMA_VERSION` here and re-run codegen.
 */

#define GML_IR_SCHEMA_VERSION "0.1.0"

typedef enum {
    GML_WIDGET_TYPE_SCREEN = 0,
    GML_WIDGET_TYPE_PANEL,
    GML_WIDGET_TYPE_LABEL,
    GML_WIDGET_TYPE_BUTTON,
    GML_WIDGET_TYPE_ICON,
    GML_WIDGET_TYPE_IMAGE,
    GML_WIDGET_TYPE_LINE,
    GML_WIDGET_TYPE_RECT,
    GML_WIDGET_TYPE_CIRCLE,
    GML_WIDGET_TYPE_TRIANGLE,
    GML_WIDGET_TYPE_FREEHAND,
    GML_WIDGET_TYPE__COUNT,
} gml_widget_type_t;

typedef enum {
    GML_LAYOUT_ABSOLUTE = 0,
    GML_LAYOUT_ROW,
    GML_LAYOUT_COLUMN,
} gml_layout_mode_t;

typedef enum {
    GML_ALIGN_START = 0,
    GML_ALIGN_CENTER,
    GML_ALIGN_END,
    GML_ALIGN_STRETCH,
    GML_ALIGN_SPACE_BETWEEN,
} gml_align_t;

typedef enum {
    GML_LABEL_ALIGN_LEFT = 0,
    GML_LABEL_ALIGN_CENTER = 1,
    GML_LABEL_ALIGN_RIGHT = 2,
} gml_label_align_t;

typedef enum {
    GML_VERTICAL_ALIGN_TOP = 0,
    GML_VERTICAL_ALIGN_CENTER = 1,
    GML_VERTICAL_ALIGN_BOTTOM = 2,
} gml_vertical_align_t;

typedef enum {
    GML_TRIANGLE_DIRECTION_UP = 0,
    GML_TRIANGLE_DIRECTION_RIGHT,
    GML_TRIANGLE_DIRECTION_DOWN,
    GML_TRIANGLE_DIRECTION_LEFT,
} gml_triangle_direction_t;

typedef enum {
    GML_ACTION_NONE = 0,
    GML_ACTION_NAVIGATE,
    GML_ACTION_SHOW,
    GML_ACTION_HIDE,
    GML_ACTION_SET_TEXT,
    GML_ACTION_SET_VALUE,
} gml_action_kind_t;

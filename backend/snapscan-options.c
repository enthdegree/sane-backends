/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998, 2001 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, Sebastien Sable, Mikael Magnusson,
   Oliver Schwartz and Kevin Charter

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   This file is a component of the implementation of a backend for many
   of the the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */

/* $Id$
   SANE SnapScan backend */

/* ranges */
static const SANE_Range x_range_fb =
{
    SANE_FIX (0.0), SANE_FIX (216.0), 0
};        /* mm */
static const SANE_Range y_range_fb =
{
    SANE_FIX (0.0), SANE_FIX (297.0), 0
};        /* mm */
static const SANE_Range x_range_tpo_default =
{
    SANE_FIX (0.0), SANE_FIX (129.0), 0
};        /* mm */
static const SANE_Range y_range_tpo_default =
{
    SANE_FIX (0.0), SANE_FIX (180.0), 0
};        /* mm */
static const SANE_Range x_range_tpo_1236 =
{
    SANE_FIX (0.0), SANE_FIX (203.0), 0
};        /* mm */
static const SANE_Range y_range_tpo_1236 =
{
    SANE_FIX (0.0), SANE_FIX (254.0), 0
};        /* mm */
static SANE_Range x_range_tpo;
static SANE_Range y_range_tpo;
static const SANE_Range gamma_range =
{
    SANE_FIX (0.0), SANE_FIX (4.0), 0
};
static const SANE_Range gamma_vrange =
{
    0, 255, 1
};
static const SANE_Range lpr_range =
{
    1, 50, 1
};

static const SANE_Range brightness_range =
{
    -400 << SANE_FIXED_SCALE_SHIFT,
    400 << SANE_FIXED_SCALE_SHIFT,
    1 << SANE_FIXED_SCALE_SHIFT
};

static const SANE_Range contrast_range =
{
    -100 << SANE_FIXED_SCALE_SHIFT,
    400 << SANE_FIXED_SCALE_SHIFT,
    1 << SANE_FIXED_SCALE_SHIFT
};

static const SANE_Range positive_percent_range =
{
    0 << SANE_FIXED_SCALE_SHIFT,
    100 << SANE_FIXED_SCALE_SHIFT,
    1 << SANE_FIXED_SCALE_SHIFT
};

/* init_options -- initialize the option set for a scanner; expects the
   scanner structure's hardware configuration byte (hconfig) to be valid.

   ARGS: a pointer to an existing scanner structure
   RET:  nothing
   SIDE: the option set of *ps is initialized; this includes both
   the option descriptors and the option values themselves */
static void init_options (SnapScan_Scanner * ps)
{
    static SANE_Word resolutions_300[] =
        {6, 50, 75, 100, 150, 200, 300};
    static SANE_Word resolutions_600[] =
        {8, 50, 75, 100, 150, 200, 300, 450, 600};
    static SANE_Word resolutions_1200[] =
        {10, 50, 75, 100, 150, 200, 300, 450, 600, 900, 1200};
    static SANE_String_Const names_all[] =
        {md_colour, md_bilevelcolour, md_greyscale, md_lineart, NULL};
    static SANE_String_Const names_basic[] =
        {md_colour, md_greyscale, md_lineart, NULL};
    static SANE_String_Const preview_names_all[] =
        {md_auto, md_colour, md_bilevelcolour, md_greyscale, md_lineart, NULL};
    static SANE_String_Const preview_names_basic[] =
        {md_auto, md_colour, md_greyscale, md_lineart, NULL};
    SANE_Option_Descriptor *po = ps->options;

    po[OPT_COUNT].name = SANE_NAME_NUM_OPTIONS;
    po[OPT_COUNT].title = SANE_TITLE_NUM_OPTIONS;
    po[OPT_COUNT].desc = SANE_DESC_NUM_OPTIONS;
    po[OPT_COUNT].type = SANE_TYPE_INT;
    po[OPT_COUNT].unit = SANE_UNIT_NONE;
    po[OPT_COUNT].size = sizeof (SANE_Word);
    po[OPT_COUNT].cap = SANE_CAP_SOFT_DETECT;
    {
        static SANE_Range count_range =
            {NUM_OPTS, NUM_OPTS, 0};
        po[OPT_COUNT].constraint_type = SANE_CONSTRAINT_RANGE;
        po[OPT_COUNT].constraint.range = &count_range;
    }

    po[OPT_MODE_GROUP].title = SANE_I18N("Scan Mode");
    po[OPT_MODE_GROUP].desc = "";
    po[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
    po[OPT_MODE_GROUP].cap = 0;
    po[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_SCANRES].name = SANE_NAME_SCAN_RESOLUTION;
    po[OPT_SCANRES].title = SANE_TITLE_SCAN_RESOLUTION;
    po[OPT_SCANRES].desc = SANE_DESC_SCAN_RESOLUTION;
    po[OPT_SCANRES].type = SANE_TYPE_INT;
    po[OPT_SCANRES].unit = SANE_UNIT_DPI;
    po[OPT_SCANRES].size = sizeof (SANE_Word);
    po[OPT_SCANRES].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC;
    po[OPT_SCANRES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    switch (ps->pdev->model)
    {
    case SNAPSCAN310:
    case VUEGO310S:                /* WG changed */
        po[OPT_SCANRES].constraint.word_list = resolutions_300;
        break;
    case SNAPSCANE50:
    case SNAPSCANE52:
    case PRISA5300:
    case PRISA1240:
        po[OPT_SCANRES].constraint.word_list = resolutions_1200;
        break;
    default:
        po[OPT_SCANRES].constraint.word_list = resolutions_600;
        break;
    }
    ps->res = DEFAULT_RES;

    po[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
    po[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
    po[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
    po[OPT_PREVIEW].type = SANE_TYPE_BOOL;
    po[OPT_PREVIEW].unit = SANE_UNIT_NONE;
    po[OPT_PREVIEW].size = sizeof (SANE_Word);
    po[OPT_PREVIEW].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC;
    po[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
    ps->preview = DEFAULT_PREVIEW;

    po[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
    po[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
    po[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
    po[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
    po[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
    po[OPT_BRIGHTNESS].size = sizeof (int);
    po[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_BRIGHTNESS].constraint.range = &brightness_range;
    ps->bright = DEFAULT_BRIGHTNESS;

    po[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
    po[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
    po[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
    po[OPT_CONTRAST].type = SANE_TYPE_FIXED;
    po[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
    po[OPT_CONTRAST].size = sizeof (int);
    po[OPT_CONTRAST].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_CONTRAST].constraint.range = &contrast_range;
    ps->contrast = DEFAULT_CONTRAST;

    po[OPT_MODE].name = SANE_NAME_SCAN_MODE;
    po[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
    po[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
    po[OPT_MODE].type = SANE_TYPE_STRING;
    po[OPT_MODE].unit = SANE_UNIT_NONE;
    po[OPT_MODE].size = 32;
    po[OPT_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    switch (ps->pdev->model)
    {
    case SNAPSCAN310:
    case VUEGO310S:
        po[OPT_MODE].constraint.string_list = names_basic;
        break;
    default:
        po[OPT_MODE].constraint.string_list = names_all;
        break;
    }
    ps->mode_s = md_colour;
    ps->mode = MD_COLOUR;

    po[OPT_PREVIEW_MODE].name = "preview-mode";
    po[OPT_PREVIEW_MODE].title = SANE_I18N("Preview mode");
    po[OPT_PREVIEW_MODE].desc = SANE_I18N(
        "Select the mode for previews. Greyscale previews usually give "
        "the best combination of speed and detail.");
    po[OPT_PREVIEW_MODE].type = SANE_TYPE_STRING;
    po[OPT_PREVIEW_MODE].unit = SANE_UNIT_NONE;
    po[OPT_PREVIEW_MODE].size = 32;
    po[OPT_PREVIEW_MODE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;
    po[OPT_PREVIEW_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    switch (ps->pdev->model)
    {
    case SNAPSCAN310:
    case VUEGO310S:
        po[OPT_PREVIEW_MODE].constraint.string_list = preview_names_basic;
        break;
    default:
        po[OPT_PREVIEW_MODE].constraint.string_list = preview_names_all;
        break;
    }
    ps->preview_mode_s = md_auto;
    ps->preview_mode = ps->mode;

    /* source */
    po[OPT_SOURCE].name  = SANE_NAME_SCAN_SOURCE;
    po[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
    po[OPT_SOURCE].desc  = SANE_DESC_SCAN_SOURCE;
    po[OPT_SOURCE].type  = SANE_TYPE_STRING;
    po[OPT_SOURCE].cap   = SANE_CAP_SOFT_SELECT | SANE_CAP_INACTIVE;
    po[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    {
        static SANE_String_Const source_list[3];
        int i = 0;

        source_list[i++]= src_flatbed;
        if (ps->hconfig & HCFG_TPO)
        {
            source_list[i++] = src_tpo;
            po[OPT_SOURCE].cap &= ~SANE_CAP_INACTIVE;
        }
        source_list[i] = 0;
        po[OPT_SOURCE].size = max_string_size(source_list);
        po[OPT_SOURCE].constraint.string_list = source_list;
        ps->source = SRC_FLATBED;
        ps->source_s = (SANE_Char *) strdup(source_list[0]);
    }

    po[OPT_GEOMETRY_GROUP].title = SANE_I18N("Geometry");
    po[OPT_GEOMETRY_GROUP].desc = "";
    po[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
    po[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
    po[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_TLX].name = SANE_NAME_SCAN_TL_X;
    po[OPT_TLX].title = SANE_TITLE_SCAN_TL_X;
    po[OPT_TLX].desc = SANE_DESC_SCAN_TL_X;
    po[OPT_TLX].type = SANE_TYPE_FIXED;
    po[OPT_TLX].unit = SANE_UNIT_MM;
    po[OPT_TLX].size = sizeof (SANE_Word);
    po[OPT_TLX].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_TLX].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_TLX].constraint.range = &(ps->pdev->x_range);
    ps->tlx = ps->pdev->x_range.min;

    po[OPT_TLY].name = SANE_NAME_SCAN_TL_Y;
    po[OPT_TLY].title = SANE_TITLE_SCAN_TL_Y;
    po[OPT_TLY].desc = SANE_DESC_SCAN_TL_Y;
    po[OPT_TLY].type = SANE_TYPE_FIXED;
    po[OPT_TLY].unit = SANE_UNIT_MM;
    po[OPT_TLY].size = sizeof (SANE_Word);
    po[OPT_TLY].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_TLY].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_TLY].constraint.range = &(ps->pdev->y_range);
    ps->tly = ps->pdev->y_range.min;

    po[OPT_BRX].name = SANE_NAME_SCAN_BR_X;
    po[OPT_BRX].title = SANE_TITLE_SCAN_BR_X;
    po[OPT_BRX].desc = SANE_DESC_SCAN_BR_X;
    po[OPT_BRX].type = SANE_TYPE_FIXED;
    po[OPT_BRX].unit = SANE_UNIT_MM;
    po[OPT_BRX].size = sizeof (SANE_Word);
    po[OPT_BRX].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_BRX].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_BRX].constraint.range = &(ps->pdev->x_range);
    ps->brx = ps->pdev->x_range.max;

    po[OPT_BRY].name = SANE_NAME_SCAN_BR_Y;
    po[OPT_BRY].title = SANE_TITLE_SCAN_BR_Y;
    po[OPT_BRY].desc = SANE_DESC_SCAN_BR_Y;
    po[OPT_BRY].type = SANE_TYPE_FIXED;
    po[OPT_BRY].unit = SANE_UNIT_MM;
    po[OPT_BRY].size = sizeof (SANE_Word);
    po[OPT_BRY].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_BRY].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_BRY].constraint.range = &(ps->pdev->y_range);
    ps->bry = ps->pdev->y_range.max;

    po[OPT_PREDEF_WINDOW].name = "predef-window";
    po[OPT_PREDEF_WINDOW].title = SANE_I18N("Predefined settings");
    po[OPT_PREDEF_WINDOW].desc = SANE_I18N(
        "Provides standard scanning areas for photographs, printed pages "
        "and the like.");
    po[OPT_PREDEF_WINDOW].type = SANE_TYPE_STRING;
    po[OPT_PREDEF_WINDOW].unit = SANE_UNIT_NONE;
    po[OPT_PREDEF_WINDOW].size = 32;
    po[OPT_PREDEF_WINDOW].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    {
        static SANE_String_Const names[] =
            {pdw_none, pdw_6X4, pdw_8X10, pdw_85X11, NULL};
        po[OPT_PREDEF_WINDOW].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        po[OPT_PREDEF_WINDOW].constraint.string_list = names;
    }
    ps->predef_window = pdw_none;

    po[OPT_ENHANCEMENT_GROUP].title = SANE_I18N("Enhancement");
    po[OPT_ENHANCEMENT_GROUP].desc = "";
    po[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
    po[OPT_ENHANCEMENT_GROUP].cap = 0;
    po[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_QUALITY_CAL].name = SANE_NAME_QUALITY_CAL;
    po[OPT_QUALITY_CAL].title = SANE_TITLE_QUALITY_CAL;
    po[OPT_QUALITY_CAL].desc = SANE_DESC_QUALITY_CAL;
    po[OPT_QUALITY_CAL].type = SANE_TYPE_BOOL;
    po[OPT_QUALITY_CAL].unit = SANE_UNIT_NONE;
    po[OPT_QUALITY_CAL].size = sizeof (SANE_Bool);
    po[OPT_QUALITY_CAL].constraint_type = SANE_CONSTRAINT_NONE;
    po[OPT_QUALITY_CAL].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    ps->val[OPT_QUALITY_CAL].b = DEFAULT_QUALITY;
    /* Disable quality calibration option if not supported
       Note: Snapscan e52 does not support quality calibration,
       although HCFG_CAL_ALLOWED is set. */
    if ((!(ps->hconfig & HCFG_CAL_ALLOWED))
        || (ps->pdev->model == SNAPSCANE52)) {
        po[OPT_QUALITY_CAL].cap |= SANE_CAP_INACTIVE;
        ps->val[OPT_QUALITY_CAL].b = SANE_FALSE;
    }

    po[OPT_GAMMA_BIND].name = SANE_NAME_ANALOG_GAMMA_BIND;
    po[OPT_GAMMA_BIND].title = SANE_TITLE_ANALOG_GAMMA_BIND;
    po[OPT_GAMMA_BIND].desc = SANE_DESC_ANALOG_GAMMA_BIND;
    po[OPT_GAMMA_BIND].type = SANE_TYPE_BOOL;
    po[OPT_GAMMA_BIND].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_BIND].size = sizeof (SANE_Bool);
    po[OPT_GAMMA_BIND].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_GAMMA_BIND].constraint_type = SANE_CONSTRAINT_NONE;
    ps->val[OPT_GAMMA_BIND].b = DEFAULT_GAMMA_BIND;

    po[OPT_GAMMA_GS].name = SANE_NAME_ANALOG_GAMMA;
    po[OPT_GAMMA_GS].title = SANE_TITLE_ANALOG_GAMMA;
    po[OPT_GAMMA_GS].desc = SANE_DESC_ANALOG_GAMMA;
    po[OPT_GAMMA_GS].type = SANE_TYPE_FIXED;
    po[OPT_GAMMA_GS].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_GS].size = sizeof (SANE_Word);
    po[OPT_GAMMA_GS].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    po[OPT_GAMMA_GS].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_GS].constraint.range = &gamma_range;
    ps->gamma_gs = DEFAULT_GAMMA;

    po[OPT_GAMMA_R].name = SANE_NAME_ANALOG_GAMMA_R;
    po[OPT_GAMMA_R].title = SANE_TITLE_ANALOG_GAMMA_R;
    po[OPT_GAMMA_R].desc = SANE_DESC_ANALOG_GAMMA_R;
    po[OPT_GAMMA_R].type = SANE_TYPE_FIXED;
    po[OPT_GAMMA_R].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_R].size = sizeof (SANE_Word);
    po[OPT_GAMMA_R].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_R].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_R].constraint.range = &gamma_range;
    ps->gamma_r = DEFAULT_GAMMA;

    po[OPT_GAMMA_G].name = SANE_NAME_ANALOG_GAMMA_G;
    po[OPT_GAMMA_G].title = SANE_TITLE_ANALOG_GAMMA_G;
    po[OPT_GAMMA_G].desc = SANE_DESC_ANALOG_GAMMA_G;
    po[OPT_GAMMA_G].type = SANE_TYPE_FIXED;
    po[OPT_GAMMA_G].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_G].size = sizeof (SANE_Word);
    po[OPT_GAMMA_G].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_G].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_G].constraint.range = &gamma_range;
    ps->gamma_g = DEFAULT_GAMMA;

    po[OPT_GAMMA_B].name = SANE_NAME_ANALOG_GAMMA_B;
    po[OPT_GAMMA_B].title = SANE_TITLE_ANALOG_GAMMA_B;
    po[OPT_GAMMA_B].desc = SANE_DESC_ANALOG_GAMMA_B;
    po[OPT_GAMMA_B].type = SANE_TYPE_FIXED;
    po[OPT_GAMMA_B].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_B].size = sizeof (SANE_Word);
    po[OPT_GAMMA_B].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_B].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_B].constraint.range = &gamma_range;
    ps->gamma_b = DEFAULT_GAMMA;

    po[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
    po[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
    po[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
    po[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
    po[OPT_CUSTOM_GAMMA].unit = SANE_UNIT_NONE;
    po[OPT_CUSTOM_GAMMA].size = sizeof (SANE_Bool);
    po[OPT_CUSTOM_GAMMA].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    ps->val[OPT_CUSTOM_GAMMA].b = DEFAULT_CUSTOM_GAMMA;

    po[OPT_GAMMA_VECTOR_GS].name = SANE_NAME_GAMMA_VECTOR;
    po[OPT_GAMMA_VECTOR_GS].title = SANE_TITLE_GAMMA_VECTOR;
    po[OPT_GAMMA_VECTOR_GS].desc = SANE_DESC_GAMMA_VECTOR;
    po[OPT_GAMMA_VECTOR_GS].type = SANE_TYPE_INT;
    po[OPT_GAMMA_VECTOR_GS].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_VECTOR_GS].size = ps->gamma_length * sizeof (SANE_Word);
    po[OPT_GAMMA_VECTOR_GS].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_VECTOR_GS].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_VECTOR_GS].constraint.range = &gamma_vrange;
    ps->val[OPT_GAMMA_VECTOR_GS].wa = ps->gamma_table_gs;

    po[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
    po[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
    po[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
    po[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
    po[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_VECTOR_R].size = ps->gamma_length * sizeof (SANE_Word);
    po[OPT_GAMMA_VECTOR_R].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_vrange;
    ps->val[OPT_GAMMA_VECTOR_R].wa = ps->gamma_table_r;

    po[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
    po[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
    po[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
    po[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
    po[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_VECTOR_G].size = ps->gamma_length * sizeof (SANE_Word);
    po[OPT_GAMMA_VECTOR_G].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_vrange;
    ps->val[OPT_GAMMA_VECTOR_G].wa = ps->gamma_table_g;

    po[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
    po[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
    po[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
    po[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
    po[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
    po[OPT_GAMMA_VECTOR_B].size = ps->gamma_length * sizeof (SANE_Word);
    po[OPT_GAMMA_VECTOR_B].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_vrange;
    ps->val[OPT_GAMMA_VECTOR_B].wa = ps->gamma_table_b;

    po[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
    po[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
    po[OPT_HALFTONE].desc = SANE_DESC_HALFTONE;
    po[OPT_HALFTONE].type = SANE_TYPE_BOOL;
    po[OPT_HALFTONE].unit = SANE_UNIT_NONE;
    po[OPT_HALFTONE].size = sizeof (SANE_Bool);
    po[OPT_HALFTONE].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_NONE;
    ps->halftone = DEFAULT_HALFTONE;

    po[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
    po[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
    po[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
    po[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
    po[OPT_HALFTONE_PATTERN].unit = SANE_UNIT_NONE;
    po[OPT_HALFTONE_PATTERN].size = 32;
    po[OPT_HALFTONE_PATTERN].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    switch (ps->hconfig & HCFG_HT)
    {
    case HCFG_HT:
        /* both 16x16, 8x8 matrices */
        {
            static SANE_String_Const names[] = {dm_dd8x8, dm_dd16x16, NULL};

            po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
            ps->dither_matrix = dm_dd8x8;
        }
        break;
    case HCFG_HT16:
        /* 16x16 matrices only */
        {
            static SANE_String_Const names[] = {dm_dd16x16, NULL};

            po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
            ps->dither_matrix = dm_dd16x16;
        }
        break;
    case HCFG_HT8:
        /* 8x8 matrices only */
        {
            static SANE_String_Const names[] = {dm_dd8x8, NULL};

            po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
            ps->dither_matrix = dm_dd8x8;
        }
        break;
    default:
        /* no halftone matrices */
        {
            static SANE_String_Const names[] = {dm_none, NULL};

            po[OPT_HALFTONE_PATTERN].constraint.string_list = names;
            ps->dither_matrix = dm_none;
        }
    }

    po[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
    po[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
    po[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
    po[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
    po[OPT_NEGATIVE].unit = SANE_UNIT_NONE;
    po[OPT_NEGATIVE].size = sizeof (SANE_Bool);
    po[OPT_NEGATIVE].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_NONE;
    ps->negative = DEFAULT_NEGATIVE;

    po[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
    po[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
    po[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
    po[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
    po[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
    po[OPT_THRESHOLD].size = sizeof (SANE_Int);
    po[OPT_THRESHOLD].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
    po[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_THRESHOLD].constraint.range = &positive_percent_range;
    ps->threshold = DEFAULT_THRESHOLD;

    po[OPT_ADVANCED_GROUP].title = SANE_I18N("Advanced");
    po[OPT_ADVANCED_GROUP].desc = "";
    po[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
    po[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;
    po[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_RGB_LPR].name = "rgb-lpr";
    po[OPT_RGB_LPR].title = SANE_I18N("Colour lines per read");
    po[OPT_RGB_LPR].desc = lpr_desc;
    po[OPT_RGB_LPR].type = SANE_TYPE_INT;
    po[OPT_RGB_LPR].unit = SANE_UNIT_NONE;
    po[OPT_RGB_LPR].size = sizeof (SANE_Word);
    po[OPT_RGB_LPR].cap =
        SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
    po[OPT_RGB_LPR].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_RGB_LPR].constraint.range = &lpr_range;
    ps->rgb_lpr = def_rgb_lpr;

    po[OPT_GS_LPR].name = "gs-lpr";
    po[OPT_GS_LPR].title = SANE_I18N("Greyscale lines per read");
    po[OPT_GS_LPR].desc = lpr_desc;
    po[OPT_GS_LPR].type = SANE_TYPE_INT;
    po[OPT_GS_LPR].unit = SANE_UNIT_NONE;
    po[OPT_GS_LPR].size = sizeof (SANE_Word);
    po[OPT_GS_LPR].cap = SANE_CAP_SOFT_SELECT
                       | SANE_CAP_SOFT_DETECT
                       | SANE_CAP_ADVANCED
                       | SANE_CAP_INACTIVE;
    po[OPT_GS_LPR].constraint_type = SANE_CONSTRAINT_RANGE;
    po[OPT_GS_LPR].constraint.range = &lpr_range;
    ps->gs_lpr = def_gs_lpr;

    po[OPT_SCSI_CMDS].title = SANE_I18N("SCSI commands (for debugging)");
    po[OPT_SCSI_CMDS].desc = "";
    po[OPT_SCSI_CMDS].type = SANE_TYPE_GROUP;
    po[OPT_SCSI_CMDS].cap = SANE_CAP_ADVANCED;

    po[OPT_INQUIRY].name = "do-inquiry";
    po[OPT_INQUIRY].title = SANE_I18N("Inquiry");
    po[OPT_INQUIRY].desc = SANE_I18N(
        "Send an Inquiry command to the scanner and dump out some of "
        "the current settings.");
    po[OPT_INQUIRY].type = SANE_TYPE_BUTTON;
    po[OPT_INQUIRY].cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
    po[OPT_INQUIRY].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_SELF_TEST].name = "do-self-test";
    po[OPT_SELF_TEST].title = SANE_I18N("Self test");
    po[OPT_SELF_TEST].desc = SANE_I18N(
        "Send a Self Test command to the scanner and report the result.");
    po[OPT_SELF_TEST].type = SANE_TYPE_BUTTON;
    po[OPT_SELF_TEST].cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
    po[OPT_SELF_TEST].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_REQ_SENSE].name = "do-req-sense";
    po[OPT_REQ_SENSE].title = SANE_I18N("Request sense");
    po[OPT_REQ_SENSE].desc = SANE_I18N(
        "Send a Request Sense command to the scanner, and print out the sense "
        "report.");
    po[OPT_REQ_SENSE].type = SANE_TYPE_BUTTON;
    po[OPT_REQ_SENSE].cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
    po[OPT_REQ_SENSE].constraint_type = SANE_CONSTRAINT_NONE;

    po[OPT_REL_UNIT].name = "do-rel-unit";
    po[OPT_REL_UNIT].title = SANE_I18N("Release unit (cancel)");
    po[OPT_REL_UNIT].desc = SANE_I18N(
        "Send a Release Unit command to the scanner. This is the same as "
        "a cancel command.");
    po[OPT_REL_UNIT].type = SANE_TYPE_BUTTON;
    po[OPT_REL_UNIT].cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
    po[OPT_REL_UNIT].constraint_type = SANE_CONSTRAINT_NONE;
}

const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle h,
                                                          SANE_Int n)
{
    DBG (DL_CALL_TRACE,
         "sane_snapscan_get_option_descriptor (%p, %ld)\n",
         (void *) h,
         (long) n);

    if ((n >= 0) && (n < NUM_OPTS))
        return ((SnapScan_Scanner *) h)->options + n;
    return NULL;
}

/* Activates or deactivates options depending on mode  */
static void control_options(SnapScan_Scanner *pss)
{
    /* first deactivate all options */
    pss->options[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_GS].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_R].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_G].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_B].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_VECTOR_GS].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
    pss->options[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

    if ((pss->mode == MD_COLOUR) ||
        ((pss->mode == MD_BILEVELCOLOUR) && (pss->hconfig & HCFG_HT) &&
         pss->halftone))
    {
        pss->options[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
        pss->options[OPT_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
        if (pss->val[OPT_CUSTOM_GAMMA].b)
        {
            if (pss->val[OPT_GAMMA_BIND].b)
            {
                pss->options[OPT_GAMMA_VECTOR_GS].cap &= ~SANE_CAP_INACTIVE;
            }
            else
            {
                pss->options[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
                pss->options[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
                pss->options[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
            }
        }
        else
        {
            pss->options[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
            pss->options[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
            if (pss->val[OPT_GAMMA_BIND].b)
            {
                pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
            }
            else
            {
                pss->options[OPT_GAMMA_R].cap &= ~SANE_CAP_INACTIVE;
                pss->options[OPT_GAMMA_G].cap &= ~SANE_CAP_INACTIVE;
                pss->options[OPT_GAMMA_B].cap &= ~SANE_CAP_INACTIVE;
            }
        }
    }
    else if ((pss->mode == MD_GREYSCALE) ||
             ((pss->mode == MD_LINEART) && (pss->hconfig & HCFG_HT) &&
              pss->halftone))
    {
        pss->options[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;

        if (pss->val[OPT_CUSTOM_GAMMA].b)
        {
            pss->options[OPT_GAMMA_VECTOR_GS].cap &= ~SANE_CAP_INACTIVE;
        }
        else
        {
            pss->options[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
            pss->options[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
            pss->options[OPT_GAMMA_GS].cap &= ~SANE_CAP_INACTIVE;
        }
    }
}

SANE_Status sane_control_option (SANE_Handle h,
                                 SANE_Int n,
                                 SANE_Action a,
                                 void *v,
                                 SANE_Int *i)
{
    static const char *me = "sane_snapscan_control_option";
    SnapScan_Scanner *pss = h;
    SnapScan_Device *pdev = pss->pdev;
    static SANE_Status status;

    DBG (DL_CALL_TRACE,
        "%s (%p, %ld, %ld, %p, %p)\n",
        me,
        (void *) h,
        (long) n,
        (long) a,
        v,
        (void *) i);

    status = open_scanner (pss);
    CHECK_STATUS (status, me, "open_scanner");

    /* possible authorization required */

    switch (a)
    {
    case SANE_ACTION_GET_VALUE:
        /* prevent getting of inactive options */
        if (!SANE_OPTION_IS_ACTIVE(pss->options[n].cap)) {
            return SANE_STATUS_INVAL;
        }
        switch (n)
        {
        case OPT_COUNT:
            *(SANE_Int *) v = NUM_OPTS;
            break;
        case OPT_SCANRES:
            *(SANE_Int *) v = pss->res;
            break;
        case OPT_PREVIEW:
            *(SANE_Bool *) v = pss->preview;
            break;
        case OPT_MODE:
            DBG (DL_VERBOSE,
                 "%s: writing \"%s\" to location %p\n",
                 me,
                 pss->mode_s,
                 (SANE_String) v);
            strcpy ((SANE_String) v, pss->mode_s);
            break;
        case OPT_PREVIEW_MODE:
            DBG (DL_VERBOSE,
                 "%s: writing \"%s\" to location %p\n",
                 me,
                 pss->preview_mode_s,
                 (SANE_String) v);
            strcpy ((SANE_String) v, pss->preview_mode_s);
            break;
        case OPT_SOURCE:
            strcpy (v, pss->source_s);
            break;
        case OPT_TLX:
            *(SANE_Fixed *) v = pss->tlx;
            break;
        case OPT_TLY:
            *(SANE_Fixed *) v = pss->tly;
            break;
        case OPT_BRX:
            *(SANE_Fixed *) v = pss->brx;
            break;
        case OPT_BRY:
            *(SANE_Fixed *) v = pss->bry;
            break;
        case OPT_BRIGHTNESS:
            *(SANE_Int *) v = pss->bright << SANE_FIXED_SCALE_SHIFT;
            break;
        case OPT_CONTRAST:
            *(SANE_Int *) v = pss->contrast << SANE_FIXED_SCALE_SHIFT;
            break;
        case OPT_PREDEF_WINDOW:
            DBG (DL_VERBOSE,
                "%s: writing \"%s\" to location %p\n",
                me,
                pss->predef_window,
                (SANE_String) v);
            strcpy ((SANE_String) v, pss->predef_window);
            break;
        case OPT_GAMMA_GS:
            *(SANE_Fixed *) v = pss->gamma_gs;
            break;
        case OPT_GAMMA_R:
            *(SANE_Fixed *) v = pss->gamma_r;
            break;
        case OPT_GAMMA_G:
            *(SANE_Fixed *) v = pss->gamma_g;
            break;
        case OPT_GAMMA_B:
            *(SANE_Fixed *) v = pss->gamma_b;
            break;
        case OPT_CUSTOM_GAMMA:
        case OPT_GAMMA_BIND:
        case OPT_QUALITY_CAL:
            *(SANE_Bool *) v = pss->val[n].b;
            break;

        case OPT_GAMMA_VECTOR_GS:
        case OPT_GAMMA_VECTOR_R:
        case OPT_GAMMA_VECTOR_G:
        case OPT_GAMMA_VECTOR_B:
            memcpy (v, pss->val[n].wa, pss->options[n].size);
            break;
        case OPT_HALFTONE:
            *(SANE_Bool *) v = pss->halftone;
            break;
        case OPT_HALFTONE_PATTERN:
            DBG (DL_VERBOSE,
                "%s: writing \"%s\" to location %p\n",
                me,
                pss->dither_matrix,
                (SANE_String) v);
            strcpy ((SANE_String) v, pss->dither_matrix);
            break;
        case OPT_NEGATIVE:
            *(SANE_Bool *) v = pss->negative;
            break;
        case OPT_THRESHOLD:
            *(SANE_Int *) v = pss->threshold << SANE_FIXED_SCALE_SHIFT;
            break;
        case OPT_RGB_LPR:
            *(SANE_Int *) v = pss->rgb_lpr;
            break;
        case OPT_GS_LPR:
            *(SANE_Int *) v = pss->gs_lpr;
            break;
        default:
            DBG (DL_MAJOR_ERROR,
                 "%s: invalid option number %ld\n",
                 me,
                 (long) n);
            return SANE_STATUS_UNSUPPORTED;
        }
        break;
    case SANE_ACTION_SET_VALUE:
        if (i)
            *i = 0;
        /* prevent setting of inactive options */
        if ((!SANE_OPTION_IS_SETTABLE(pss->options[n].cap)) ||
            (!SANE_OPTION_IS_ACTIVE(pss->options[n].cap))) {
            return SANE_STATUS_INVAL;
        }
        status = sanei_constrain_value(&pss->options[n], v, i);
        if (status != SANE_STATUS_GOOD) {
            return status;
        }
        switch (n)
        {
        case OPT_COUNT:
            return SANE_STATUS_UNSUPPORTED;
        case OPT_SCANRES:
            pss->res = *(SANE_Int *) v;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_PREVIEW:
            pss->preview = *(SANE_Bool *) v;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_MODE:
            {
                char *s = (SANE_String) v;
                if (strcmp (s, md_colour) == 0)
                {
                    pss->mode_s = md_colour;
                    pss->mode = MD_COLOUR;
                    if (pss->preview_mode_s == md_auto)
                      pss->preview_mode = MD_COLOUR;
                    pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
                }
                else if (strcmp (s, md_bilevelcolour) == 0)
                {
                    int ht_cap = pss->hconfig & HCFG_HT;
                    pss->mode_s = md_bilevelcolour;
                    pss->mode = MD_BILEVELCOLOUR;
                    if (pss->preview_mode_s == md_auto)
                      pss->preview_mode = MD_BILEVELCOLOUR;
                    if (ht_cap)
                        pss->options[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
                    if (ht_cap && pss->halftone)
                    {
                        pss->options[OPT_HALFTONE_PATTERN].cap &=
                            ~SANE_CAP_INACTIVE;
                    }
                    else
                    {
                        pss->options[OPT_HALFTONE_PATTERN].cap |=
                            SANE_CAP_INACTIVE;
                    }
                    pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
                }
                else if (strcmp (s, md_greyscale) == 0)
                {
                    pss->mode_s = md_greyscale;
                    pss->mode = MD_GREYSCALE;
                    if (pss->preview_mode_s == md_auto)
                      pss->preview_mode = MD_GREYSCALE;
                    pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
                    pss->options[OPT_GS_LPR].cap &= ~SANE_CAP_INACTIVE;
                    pss->options[OPT_RGB_LPR].cap |= SANE_CAP_INACTIVE;
                }
                else if (strcmp (s, md_lineart) == 0)
                {
                    int ht_cap = pss->hconfig & HCFG_HT;
                    pss->mode_s = md_lineart;
                    pss->mode = MD_LINEART;
                    if (pss->preview_mode_s == md_auto)
                      pss->preview_mode = MD_LINEART;
                    if (ht_cap)
                        pss->options[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
                    if (ht_cap && pss->halftone)
                    {
                        pss->options[OPT_HALFTONE_PATTERN].cap &=
                            ~SANE_CAP_INACTIVE;
                        pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
                    }
                    else
                    {
                        pss->options[OPT_HALFTONE_PATTERN].cap |=
                            SANE_CAP_INACTIVE;
                        pss->options[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
                    }
                    pss->options[OPT_NEGATIVE].cap &= ~SANE_CAP_INACTIVE;
                    pss->options[OPT_GS_LPR].cap &= ~SANE_CAP_INACTIVE;
                    pss->options[OPT_RGB_LPR].cap |= SANE_CAP_INACTIVE;
                }
                else
                {
                    DBG (DL_MAJOR_ERROR,
                        "%s: internal error: given illegal mode "
                        "string \"%s\"\n",
                        me,
                        s);
                }
            }
            control_options (pss);
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_PREVIEW_MODE:
            {
                char *s = (SANE_String) v;
                if (strcmp (s, md_auto) == 0)
                {
                  pss->preview_mode_s = md_auto;
                  pss->preview_mode = pss->mode;
                }
                else if (strcmp (s, md_colour) == 0)
                {
                    pss->preview_mode_s = md_colour;
                    pss->preview_mode = MD_COLOUR;
                }
                else if (strcmp (s, md_bilevelcolour) == 0)
                {
                    pss->preview_mode_s = md_bilevelcolour;
                    pss->preview_mode = MD_BILEVELCOLOUR;
                }
                else if (strcmp (s, md_greyscale) == 0)
                {
                    pss->preview_mode_s = md_greyscale;
                    pss->preview_mode = MD_GREYSCALE;
                }
                else if (strcmp (s, md_lineart) == 0)
                {
                    pss->preview_mode_s = md_lineart;
                    pss->preview_mode = MD_LINEART;
                }
                else
                {
                    DBG (DL_MAJOR_ERROR,
                        "%s: internal error: given illegal mode string "
                        "\"%s\"\n",
                        me,
                        s);
                }
                if (i)
                    *i = 0;
                break;
            }
        case OPT_SOURCE:
            if (strcmp(v, src_flatbed) == 0)
            {
                pss->source = SRC_FLATBED;
                pss->pdev->x_range.max = x_range_fb.max;
                pss->pdev->y_range.max = y_range_fb.max;
             }
            else if (strcmp(v, src_tpo) == 0)
            {
                pss->source = SRC_TPO;
                pss->pdev->x_range.max = x_range_tpo.max;
                pss->pdev->y_range.max = y_range_tpo.max;
            }
            else
            {
                DBG (DL_MAJOR_ERROR,
                     "%s: internal error: given illegal source string "
                      "\"%s\"\n",
                     me,
                     (char *) v);
            }
            /* Adjust actual range values to new max values */
            if (pss->brx > pss->pdev->x_range.max)
                pss->brx = pss->pdev->x_range.max;
            if (pss->bry > pss->pdev->y_range.max)
                pss->bry = pss->pdev->y_range.max;
            pss->predef_window = pdw_none;
            if (pss->source_s)
                free (pss->source_s);
            pss->source_s = (SANE_Char *) strdup(v);
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_TLX:
            pss->tlx = *(SANE_Fixed *) v;
            pss->predef_window = pdw_none;
            if (pss->tlx > pdev->x_range.max) {
                pss->tlx = pdev->x_range.max;
            }
            if (pss->brx < pss->tlx) {
                pss->brx = pss->tlx;
            }
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_TLY:
            pss->tly = *(SANE_Fixed *) v;
            pss->predef_window = pdw_none;
            if (pss->tly > pdev->y_range.max){
                pss->tly = pdev->y_range.max;
            }
            if (pss->bry < pss->tly) {
                pss->bry = pss->tly;
            }
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_BRX:
            pss->brx = *(SANE_Fixed *) v;
            pss->predef_window = pdw_none;
            if (pss->brx < pdev->x_range.min) {
                pss->brx = pdev->x_range.min;
            }
            if (pss->brx < pss->tlx) {
                pss->tlx = pss->brx;
            }
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_BRY:
            pss->bry = *(SANE_Fixed *) v;
            pss->predef_window = pdw_none;
            if (pss->bry < pdev->y_range.min) {
                pss->bry = pdev->y_range.min;
            }
            if (pss->bry < pss->tly) {
                pss->tly = pss->bry;
            }
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_BRIGHTNESS:
            pss->bright = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
            if (i)
                *i = 0;
            break;
        case OPT_CONTRAST:
            pss->contrast = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
            if (i)
                *i = 0;
            break;
        case OPT_PREDEF_WINDOW:
            {
                char *s = (SANE_String) v;
                if (strcmp (s, pdw_none) != 0)
                {
                    pss->tlx = 0;
                    pss->tly = 0;

                    if (strcmp (s, pdw_6X4) == 0)
                    {
                        pss->predef_window = pdw_6X4;
                        pss->brx = SANE_FIX (6.0*MM_PER_IN);
                        pss->bry = SANE_FIX (4.0*MM_PER_IN);
                    }
                    else if (strcmp (s, pdw_8X10) == 0)
                    {
                        pss->predef_window = pdw_8X10;
                        pss->brx = SANE_FIX (8.0*MM_PER_IN);
                        pss->bry = SANE_FIX (10.0*MM_PER_IN);
                    }
                    else if (strcmp (s, pdw_85X11) == 0)
                    {
                        pss->predef_window = pdw_85X11;
                        pss->brx = SANE_FIX (8.5*MM_PER_IN);
                        pss->bry = SANE_FIX (11.0*MM_PER_IN);
                    }
                    else
                    {
                        DBG (DL_MAJOR_ERROR,
                             "%s: trying to set predef window with "
                             "garbage value.", me);
                        pss->predef_window = pdw_none;
                        pss->brx = SANE_FIX (6.0*MM_PER_IN);
                        pss->bry = SANE_FIX (4.0*MM_PER_IN);
                    }
                }
                else
                {
                    pss->predef_window = pdw_none;
                }
            }
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_GAMMA_GS:
            pss->gamma_gs = *(SANE_Fixed *) v;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_R:
            pss->gamma_r = *(SANE_Fixed *) v;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_G:
            pss->gamma_g = *(SANE_Fixed *) v;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_B:
            pss->gamma_b = *(SANE_Fixed *) v;
            if (i)
                *i = 0;
            break;
        case OPT_QUALITY_CAL:
            pss->val[n].b = *(SANE_Bool *)v;
            if (i)
                *i = 0;
            break;

        case OPT_CUSTOM_GAMMA:
        case OPT_GAMMA_BIND:
        {
            SANE_Bool b = *(SANE_Bool *) v;
            if (b == pss->val[n].b) { break; }
            pss->val[n].b = b;
            control_options (pss);
            if (i)
            {
                *i |= SANE_INFO_RELOAD_OPTIONS;
            }
            break;
        }

        case OPT_GAMMA_VECTOR_GS:
        case OPT_GAMMA_VECTOR_R:
        case OPT_GAMMA_VECTOR_G:
        case OPT_GAMMA_VECTOR_B:
            memcpy(pss->val[n].wa, v, pss->options[n].size);
            if (i)
                *i = 0;
            break;
        case OPT_HALFTONE:
            pss->halftone = *(SANE_Bool *) v;
            if (pss->halftone)
            {
                switch (pss->mode)
                {
                case MD_BILEVELCOLOUR:
                    break;
                case MD_LINEART:
                    pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
                    break;
                default:
                    break;
                }
                pss->options[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
            }
            else
            {
                pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
                if (pss->mode == MD_LINEART)
                    pss->options[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
            }
            control_options (pss);
            if (i)
                *i = SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_HALFTONE_PATTERN:
            {
                char *s = (SANE_String) v;
                if (strcmp (s, dm_dd8x8) == 0)
                {
                    pss->dither_matrix = dm_dd8x8;
                }
                else if (strcmp (s, dm_dd16x16) == 0)
                {
                    pss->dither_matrix = dm_dd16x16;
                }
                else
                {
                    DBG (DL_MAJOR_ERROR,
                         "%s: internal error: given illegal halftone pattern "
                         "string \"%s\"\n",
                         me,
                         s);
                }
            }
            if (i)
                *i = 0;
            break;
        case OPT_NEGATIVE:
            pss->negative = *(SANE_Bool *) v;
            if (i)
                *i = 0;
            break;
        case OPT_THRESHOLD:
            pss->threshold = *(SANE_Int *) v >> SANE_FIXED_SCALE_SHIFT;
            if (i)
                *i = 0;
            break;
        case OPT_INQUIRY:
            status = inquiry (pss);
            CHECK_STATUS (status, me, "inquiry");
            DBG (0,
                 "\nInquiry results:\n"
                 "\tScanner:                       %s\n"
                 "\thardware config:               0x%x\n"
                 "\tA/D converter:                 %s\n"
                 "\tAuto-document feeder:          %s\n"
                 "\tTransparency option:           %s\n"
                 "\tRing buffer:                   %s\n"
                 "\t16x16 halftone matrix support: %s\n"
                 "\t8x8 halftone matrix support:   %s\n"
                 "\tCalibration allowed:           %s\n"
                 "\toptical resolution:            %lu\n"
                 "\tscan resolution:               %lu\n"
                 "\tnumber of lines:               %lu\n"
                 "\tbytes per line:                %lu\n"
                 "\tpixels per line:               %lu\n"
                 "\tms per line:                   %f\n"
                 "\texposure time:                 %c.%c ms\n"
                 "\tred offset:                    %ld\n"
                 "\tgreen offset:                  %ld\n"
                 "\tblue offset:                   %ld\n"
                 "\tfirmware:                      %s\n\n",
                 pss->buf + INQUIRY_VENDOR,
                 pss->hconfig,
                 (pss->hconfig & HCFG_ADC)  ?  "10-bit"  :  "8-bit",
                 (pss->hconfig & HCFG_ADF)  ?  "Yes"  :  "No",
                 (pss->hconfig & HCFG_TPO) ?   "Yes"  :  "No",
                 (pss->hconfig & HCFG_RB)  ?  "Yes" : "No",
                 (pss->hconfig & HCFG_HT16)  ?  "Yes"  :  "No",
                 (pss->hconfig & HCFG_HT8)  ?  "Yes"  :  "No",
                 (pss->hconfig & HCFG_CAL_ALLOWED)  ?  "Yes"  :  "No",
                 (u_long) pss->actual_res,
                 (u_long) pss->res,
                 (u_long) pss->lines,
                 (u_long) pss->bytes_per_line,
                 (u_long) pss->pixels_per_line,
                 (double) pss->ms_per_line,
                 pss->buf[INQUIRY_EXPTIME1] + '0',
                 pss->buf[INQUIRY_EXPTIME2] + '0',
                 (long) pss->chroma_offset[R_CHAN],
                 (long) pss->chroma_offset[G_CHAN],
                 (long) pss->chroma_offset[B_CHAN],
                 pss->buf + INQUIRY_FIRMWARE);
            break;
        case OPT_SELF_TEST:
            status = send_diagnostic (pss);
            if (status == SANE_STATUS_GOOD)
                DBG (0, "Passes self-test.\n");
            CHECK_STATUS (status, me, "self_test");
            break;
        case OPT_REQ_SENSE:
            status = request_sense (pss);
            CHECK_STATUS (status, me, "request_sense");
            if (pss->sense_str)
                DBG (0, "Scanner sense: %s\n", pss->sense_str);
            if (pss->as_str)
                DBG (0, "Scanner ASC/ASCQ: %s\n", pss->as_str);
            break;
        case OPT_REL_UNIT:
            release_unit (pss);
            DBG (0, "Release unit sent.\n");
            break;
        case OPT_RGB_LPR:
            pss->rgb_lpr = *(SANE_Int *) v;
            if (i)
                *i = 0;
            break;
        case OPT_GS_LPR:
            pss->gs_lpr = *(SANE_Int *) v;
            if (i)
                *i = 0;
            break;
        default:
            DBG (DL_MAJOR_ERROR,
                 "%s: invalid option number %ld\n",
                 me,
                 (long) n);
            return SANE_STATUS_UNSUPPORTED;
        }
        DBG (DL_VERBOSE, "%s: option %s set to value ",
             me, pss->options[n].name);
        switch (pss->options[n].type)
        {
        case SANE_TYPE_INT:
            DBG (DL_VERBOSE, "%ld\n", (long) (*(SANE_Int *) v));
            break;
        case SANE_TYPE_BOOL:
            {
                char *valstr = (*(SANE_Bool *) v == SANE_TRUE)  ?  "TRUE"  :  "FALSE";
                DBG (DL_VERBOSE, "%s\n", valstr);
            }
            break;
        default:
            DBG (DL_VERBOSE, "other than an integer or boolean.\n");
            break;
        }
        break;
    case SANE_ACTION_SET_AUTO:
        switch (n)
        {
        case OPT_COUNT:
            break;
        case OPT_SCANRES:
            pss->res = 300;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_PREVIEW:
            pss->preview = SANE_FALSE;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_MODE:
            pss->mode_s = md_colour;
            pss->mode = MD_COLOUR;
            pss->options[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
            pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
            pss->options[OPT_NEGATIVE].cap |= SANE_CAP_INACTIVE;
            pss->options[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
            pss->options[OPT_GS_LPR].cap |= SANE_CAP_INACTIVE;
            pss->options[OPT_RGB_LPR].cap &= ~SANE_CAP_INACTIVE;
            control_options (pss);
            if (i)
                *i = SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_PREVIEW_MODE:
            pss->preview_mode_s = md_greyscale;
            pss->preview_mode = MD_GREYSCALE;
            if (i)
                *i = 0;
            break;
        case OPT_TLX:
            pss->tlx = pss->pdev->x_range.min;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_TLY:
            pss->tly = pss->pdev->y_range.min;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_BRX:
            pss->brx = pss->pdev->x_range.max;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_BRY:
            pss->bry = pss->pdev->y_range.max;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_PREDEF_WINDOW:
            pss->predef_window = pdw_none;
            if (i)
                *i = SANE_INFO_RELOAD_PARAMS;
            break;
        case OPT_GAMMA_GS:
            pss->gamma_gs = DEFAULT_GAMMA;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_R:
            pss->gamma_r = DEFAULT_GAMMA;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_G:
            pss->gamma_g = DEFAULT_GAMMA;
            if (i)
                *i = 0;
            break;
        case OPT_GAMMA_B:
            pss->gamma_b = DEFAULT_GAMMA;
            if (i)
                *i = 0;
            break;
        case OPT_HALFTONE:
            pss->halftone = DEFAULT_HALFTONE;
            if (pss->halftone)
            {
                pss->options[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
            }
            else
            {
                pss->options[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
            }
            control_options (pss);
            if (i)
                *i = SANE_INFO_RELOAD_OPTIONS;
            break;
        case OPT_HALFTONE_PATTERN:
            pss->dither_matrix = dm_dd8x8;
            if (i)
                *i = 0;
            break;
        case OPT_NEGATIVE:
            pss->negative = DEFAULT_NEGATIVE;
            if (i)
                *i = 0;
            break;
        case OPT_THRESHOLD:
            pss->threshold = DEFAULT_THRESHOLD;
            if (i)
                *i = 0;
            break;
        case OPT_RGB_LPR:
            pss->rgb_lpr = def_rgb_lpr;
            if (i)
                *i = 0;
            break;
        case OPT_GS_LPR:
            pss->gs_lpr = def_gs_lpr;
            if (i)
                *i = 0;
            break;
        default:
            DBG (DL_MAJOR_ERROR,
                 "%s: invalid option number %ld\n",
                 me,
                 (long) n);
            return SANE_STATUS_UNSUPPORTED;
        }
        break;
    default:
        DBG (DL_MAJOR_ERROR, "%s: invalid action code %ld\n", me, (long) a);
        return SANE_STATUS_UNSUPPORTED;
    }
    close_scanner (pss);
    return SANE_STATUS_GOOD;
}

/*
 * $Log$
 * Revision 1.2  2002/04/23 22:37:51  oliverschwartz
 * SnapScan backend version 1.4.11
 *
 * Revision 1.1  2002/03/24 12:07:15  oliverschwartz
 * Moved option functions from snapscan.c to snapscan-options.c
 *
 *
 * */

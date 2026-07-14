#include "openrcp_display.h"

#include <stdio.h>
#include "lvgl.h"

#define STRIP_SIDE_PAD      10
#define STRIP_TOP_Y         6
#define TOP_STRIP_H         50
#define TALLY_STRIP_Y       60
#define TALLY_STRIP_H       26
#define BOTTOM_STRIP_H      62
#define CELL_RADIUS         4
#define SELECTED_TALLY_BORDER_WIDTH 6

#if LV_FONT_MONTSERRAT_20
#define OPENRCP_CENTER_FONT (&lv_font_montserrat_20)
#else
#define OPENRCP_CENTER_FONT (&lv_font_montserrat_14)
#endif

static lv_obj_t *s_screen = NULL;

static lv_obj_t *s_top_strip = NULL;
static lv_obj_t *s_top_camera = NULL;
static lv_obj_t *s_top_page = NULL;
static lv_obj_t *s_top_iris = NULL;

static lv_obj_t *s_selected_tally_border = NULL;
static lv_obj_t *s_tally_strip = NULL;
static lv_obj_t *s_tally_cells[4] = {NULL};
static lv_obj_t *s_tally_labels[4] = {NULL};

static lv_obj_t *s_center_area = NULL;
static lv_obj_t *s_center_title = NULL;
static lv_obj_t *s_center_line1 = NULL;
static lv_obj_t *s_center_line2 = NULL;
static lv_obj_t *s_center_line3 = NULL;
static lv_obj_t *s_center_line4 = NULL;

static lv_obj_t *s_bottom_strip = NULL;
static lv_obj_t *s_soft_left = NULL;
static lv_obj_t *s_soft_mid = NULL;
static lv_obj_t *s_soft_right = NULL;

static lv_obj_t *s_boot_test = NULL;

static void remove_boot_test_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_boot_test) {
        lv_obj_delete(s_boot_test);
        s_boot_test = NULL;
    }
}

static const char *tally_to_text(openrcp_tally_state_t tally)
{
    switch (tally) {
        case OPENRCP_TALLY_PROGRAM: return "PROGRAM";
        case OPENRCP_TALLY_PREVIEW: return "PREVIEW";
        case OPENRCP_TALLY_NONE:
        default: return "NONE";
    }
}

static lv_color_t tally_to_color(openrcp_tally_state_t tally)
{
    switch (tally) {
        case OPENRCP_TALLY_PROGRAM: return lv_color_hex(0xFF4D4D);
        case OPENRCP_TALLY_PREVIEW: return lv_color_hex(0x33CC66);
        case OPENRCP_TALLY_NONE:
        default: return lv_color_hex(0xAAAAAA);
    }
}

static const char *tally_to_short_text(openrcp_tally_state_t tally)
{
    switch (tally) {
        case OPENRCP_TALLY_PROGRAM: return "PGM";
        case OPENRCP_TALLY_PREVIEW: return "PVW";
        case OPENRCP_TALLY_NONE:
        default: return "-";
    }
}

static lv_color_t tally_to_bg_color(openrcp_tally_state_t tally)
{
    switch (tally) {
        case OPENRCP_TALLY_PROGRAM: return lv_color_hex(0x9B111E);
        case OPENRCP_TALLY_PREVIEW: return lv_color_hex(0x0F7D38);
        case OPENRCP_TALLY_NONE:
        default: return lv_color_hex(0x15191D);
    }
}

static lv_color_t selected_tally_border_color(openrcp_tally_state_t tally)
{
    switch (tally) {
        case OPENRCP_TALLY_PROGRAM: return lv_color_hex(0xFF2020);
        case OPENRCP_TALLY_PREVIEW: return lv_color_hex(0x19D463);
        case OPENRCP_TALLY_NONE:
        default: return lv_color_black();
    }
}

static lv_obj_t *create_cell(lv_obj_t *parent, lv_coord_t height, lv_color_t accent)
{
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_size(cell, LV_PCT(32), height);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0x15191D), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, accent, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, CELL_RADIUS, 0);
    lv_obj_set_style_pad_all(cell, 4, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(cell);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_center(label);

    return label;
}

static lv_obj_t *create_center_label(lv_obj_t *parent, lv_coord_t y, lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

static lv_obj_t *create_tally_cell(lv_obj_t *parent)
{
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_size(cell, LV_PCT(24), TALLY_STRIP_H);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0x15191D), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(0x444A50), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_radius(cell, CELL_RADIUS, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(cell);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_center(label);

    return label;
}

static void set_soft_cells(const char *left, const char *mid, const char *right)
{
    lv_label_set_text(s_soft_left, left);
    lv_label_set_text(s_soft_mid, mid);
    lv_label_set_text(s_soft_right, right);
}

static void render_main_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "MAIN");

    snprintf(buf, buf_len, "WHITE  R %+d   G %+d   B %+d", cam->white_r, cam->white_g, cam->white_b);
    lv_label_set_text(s_center_line1, buf);

    snprintf(buf, buf_len, "BLACK  R %+d   G %+d   B %+d", cam->black_r, cam->black_g, cam->black_b);
    lv_label_set_text(s_center_line2, buf);

    snprintf(buf, buf_len, "PEDESTAL %+d", cam->black_level);
    lv_label_set_text(s_center_line3, buf);

    snprintf(buf, buf_len, "TALLY %s", tally_to_text(cam->tally_state));
    lv_label_set_text(s_center_line4, buf);
    lv_obj_set_style_text_color(s_center_line4, tally_to_color(cam->tally_state), 0);

    snprintf(buf, buf_len, "TINT\n%+d", cam->tint);
    char left[32];
    snprintf(left, sizeof(left), "%s", buf);

    char mid[32];
    snprintf(mid, sizeof(mid), "WB\n%d K", cam->white_balance_k);

    char right[32];
    snprintf(right, sizeof(right), "GAIN\n%+d dB", cam->gain_db);
    set_soft_cells(left, mid, right);
}

static void render_shift_soft_cells(const openrcp_camera_state_t *cam)
{
    char left[32];
    char mid[32];
    char right[32];

    snprintf(left, sizeof(left), "RESET\nCC");
    snprintf(mid, sizeof(mid), "HUE\n%+d", cam->hue);
    snprintf(right, sizeof(right), "CONTRAST\n%d", cam->contrast_adjust);
    set_soft_cells(left, mid, right);
}

static void render_lift_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "LIFT / PEDESTAL");
    snprintf(buf, buf_len, "R %+d   G %+d   B %+d", cam->black_r, cam->black_g, cam->black_b);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "LUMA %+d", cam->black_level);
    lv_label_set_text(s_center_line2, buf);
    lv_label_set_text(s_center_line3, "Black balance uses Color Correction 8.0");
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("LIFT R", "LIFT G", "LIFT B");
}

static void render_gain_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "GAIN BALANCE");
    snprintf(buf, buf_len, "R %+d   G %+d   B %+d", cam->white_r, cam->white_g, cam->white_b);
    lv_label_set_text(s_center_line1, buf);
    lv_label_set_text(s_center_line2, "LUMA 1.00");
    lv_label_set_text(s_center_line3, "White balance uses Color Correction 8.2");
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("GAIN R", "GAIN G", "GAIN B");
}

static void render_gamma_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "GAMMA");
    snprintf(buf, buf_len, "R %+d   G %+d   B %+d", cam->gamma_r, cam->gamma_g, cam->gamma_b);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "LUMA %+d", cam->gamma_luma);
    lv_label_set_text(s_center_line2, buf);
    lv_label_set_text(s_center_line3, "Prepared for Color Correction 8.1");
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("GAMMA R", "GAMMA G", "GAMMA B");
}

static void render_offset_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "OFFSET");
    snprintf(buf, buf_len, "R %+d   G %+d   B %+d", cam->offset_r, cam->offset_g, cam->offset_b);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "LUMA %+d", cam->offset_luma);
    lv_label_set_text(s_center_line2, buf);
    lv_label_set_text(s_center_line3, "Prepared for Color Correction 8.3");
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("OFFSET R", "OFFSET G", "OFFSET B");
}

static void render_contrast_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "CONTRAST");
    snprintf(buf, buf_len, "PIVOT %d%%", cam->contrast_pivot);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "ADJUST %d%%", cam->contrast_adjust);
    lv_label_set_text(s_center_line2, buf);
    snprintf(buf, buf_len, "LUMA MIX %d%%", cam->luma_mix);
    lv_label_set_text(s_center_line3, buf);
    lv_label_set_text(s_center_line4, "Prepared for Color Correction 8.4 / 8.5");
    set_soft_cells("PIVOT", "CONTRAST", "LUMA");
}

static void render_color_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "COLOR");
    snprintf(buf, buf_len, "HUE %+d", cam->hue);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "SATURATION %d%%", cam->saturation);
    lv_label_set_text(s_center_line2, buf);
    lv_label_set_text(s_center_line3, "Prepared for Color Correction 8.6");
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("HUE", "SAT", "RESET");
}

static void render_shutter_page(const openrcp_camera_state_t *cam, char *buf, size_t buf_len)
{
    lv_label_set_text(s_center_title, "VIDEO");
    snprintf(buf, buf_len, "SHUTTER 1/%d", cam->shutter_speed);
    lv_label_set_text(s_center_line1, buf);
    snprintf(buf, buf_len, "SHARPNESS %d", cam->sharpness);
    lv_label_set_text(s_center_line2, buf);
    snprintf(buf, buf_len, "GAIN %+d dB   ISO/WB page later", cam->gain_db);
    lv_label_set_text(s_center_line3, buf);
    lv_label_set_text(s_center_line4, "");
    set_soft_cells("SHUTTER", "SHARP", "GAIN");
}

static void ensure_ui_created(void)
{
    if (s_screen) {
        return;
    }

    s_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);

    s_selected_tally_border = lv_obj_create(s_screen);
    lv_obj_set_size(s_selected_tally_border, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_selected_tally_border, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_selected_tally_border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_selected_tally_border, 0, 0);
    lv_obj_set_style_border_color(s_selected_tally_border, lv_color_black(), 0);
    lv_obj_set_style_radius(s_selected_tally_border, 0, 0);
    lv_obj_clear_flag(s_selected_tally_border, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_selected_tally_border, LV_OBJ_FLAG_CLICKABLE);

    s_top_strip = lv_obj_create(s_screen);
    lv_obj_set_size(s_top_strip, LV_PCT(100), TOP_STRIP_H);
    lv_obj_align(s_top_strip, LV_ALIGN_TOP_MID, 0, STRIP_TOP_Y);
    lv_obj_set_style_bg_opa(s_top_strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_top_strip, 0, 0);
    lv_obj_set_style_pad_left(s_top_strip, STRIP_SIDE_PAD, 0);
    lv_obj_set_style_pad_right(s_top_strip, STRIP_SIDE_PAD, 0);
    lv_obj_clear_flag(s_top_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_top_strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_top_strip, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_top_camera = create_cell(s_top_strip, 42, lv_color_hex(0x66CCFF));
    s_top_page = create_cell(s_top_strip, 42, lv_color_hex(0xFFFFFF));
    s_top_iris = create_cell(s_top_strip, 42, lv_color_hex(0xFFD166));

    s_tally_strip = lv_obj_create(s_screen);
    lv_obj_set_size(s_tally_strip, LV_PCT(100), TALLY_STRIP_H);
    lv_obj_align(s_tally_strip, LV_ALIGN_TOP_MID, 0, TALLY_STRIP_Y);
    lv_obj_set_style_bg_opa(s_tally_strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_tally_strip, 0, 0);
    lv_obj_set_style_pad_left(s_tally_strip, STRIP_SIDE_PAD, 0);
    lv_obj_set_style_pad_right(s_tally_strip, STRIP_SIDE_PAD, 0);
    lv_obj_set_style_pad_top(s_tally_strip, 0, 0);
    lv_obj_set_style_pad_bottom(s_tally_strip, 0, 0);
    lv_obj_clear_flag(s_tally_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_tally_strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_tally_strip, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        s_tally_labels[i] = create_tally_cell(s_tally_strip);
        s_tally_cells[i] = lv_obj_get_parent(s_tally_labels[i]);
    }

    s_center_area = lv_obj_create(s_screen);
    lv_obj_set_size(s_center_area, LV_PCT(100), LV_PCT(48));
    lv_obj_align(s_center_area, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_style_bg_opa(s_center_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_center_area, 0, 0);
    lv_obj_clear_flag(s_center_area, LV_OBJ_FLAG_SCROLLABLE);

    s_center_title = create_center_label(s_center_area, 0, lv_color_hex(0x66CCFF), OPENRCP_CENTER_FONT);
    s_center_line1 = create_center_label(s_center_area, 36, lv_color_white(), OPENRCP_CENTER_FONT);
    s_center_line2 = create_center_label(s_center_area, 72, lv_color_white(), OPENRCP_CENTER_FONT);
    s_center_line3 = create_center_label(s_center_area, 108, lv_color_hex(0xCCCCCC), &lv_font_montserrat_14);
    s_center_line4 = create_center_label(s_center_area, 134, lv_color_hex(0xAAAAAA), &lv_font_montserrat_14);

    s_bottom_strip = lv_obj_create(s_screen);
    lv_obj_set_size(s_bottom_strip, LV_PCT(100), BOTTOM_STRIP_H);
    lv_obj_align(s_bottom_strip, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(s_bottom_strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_bottom_strip, 0, 0);
    lv_obj_set_style_pad_left(s_bottom_strip, STRIP_SIDE_PAD, 0);
    lv_obj_set_style_pad_right(s_bottom_strip, STRIP_SIDE_PAD, 0);
    lv_obj_clear_flag(s_bottom_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_bottom_strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_bottom_strip, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_soft_left = create_cell(s_bottom_strip, 54, lv_color_hex(0x66CCFF));
    s_soft_mid = create_cell(s_bottom_strip, 54, lv_color_hex(0xFFD166));
    s_soft_right = create_cell(s_bottom_strip, 54, lv_color_hex(0xCC99FF));
}

void openrcp_display_init(void)
{
    ensure_ui_created();

    if (!s_boot_test) {
        s_boot_test = lv_obj_create(lv_screen_active());
        lv_obj_set_size(s_boot_test, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(s_boot_test, lv_color_hex(0x0057FF), 0);
        lv_obj_set_style_bg_opa(s_boot_test, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_boot_test, 0, 0);
        lv_obj_clear_flag(s_boot_test, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *label = lv_label_create(s_boot_test);
        lv_label_set_text(label, "OPENRCP DISPLAY OK");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, OPENRCP_CENTER_FONT, 0);
        lv_obj_center(label);

        lv_refr_now(NULL);
        lv_timer_t *timer = lv_timer_create(remove_boot_test_cb, 2000, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }
}

void openrcp_display_render(const openrcp_model_t *model)
{
    if (!model) {
        return;
    }

    ensure_ui_created();

    const openrcp_camera_state_t *cam = &model->cameras[model->selected_camera];
    char buf[128];

    openrcp_tally_state_t selected_tally = cam->tally_state;
    lv_obj_set_style_border_width(
        s_selected_tally_border,
        selected_tally == OPENRCP_TALLY_NONE ? 0 : SELECTED_TALLY_BORDER_WIDTH,
        0
    );
    lv_obj_set_style_border_color(s_selected_tally_border, selected_tally_border_color(selected_tally), 0);

    snprintf(buf, sizeof(buf), "CAM %u", (unsigned)(model->selected_camera + 1));
    lv_label_set_text(s_top_camera, buf);

    lv_label_set_text(s_top_page, model->shift_mode ? "SHIFT" : openrcp_model_page_name(model->selected_page));

    snprintf(buf, sizeof(buf), "IRIS %.0f%%", cam->iris_normalized * 100.0f);
    lv_label_set_text(s_top_iris, buf);

    for (int i = 0; i < 4; i++) {
        const openrcp_camera_state_t *tally_cam = &model->cameras[i];
        snprintf(buf, sizeof(buf), "%d %s", i + 1, tally_to_short_text(tally_cam->tally_state));
        lv_label_set_text(s_tally_labels[i], buf);
        lv_obj_set_style_bg_color(s_tally_cells[i], tally_to_bg_color(tally_cam->tally_state), 0);
        lv_obj_set_style_border_color(
            s_tally_cells[i],
            i == model->selected_camera ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x444A50),
            0
        );
        lv_obj_set_style_border_width(s_tally_cells[i], i == model->selected_camera ? 2 : 1, 0);
    }

    lv_obj_set_style_text_color(s_center_line4, lv_color_hex(0xAAAAAA), 0);

    switch (model->selected_page) {
        case OPENRCP_PAGE_COLOR_LIFT:
            render_lift_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_COLOR_GAIN:
            render_gain_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_GAMMA:
            render_gamma_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_OFFSET:
            render_offset_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_CONTRAST:
            render_contrast_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_COLOR:
            render_color_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_SHUTTER:
            render_shutter_page(cam, buf, sizeof(buf));
            break;

        case OPENRCP_PAGE_MAIN:
        case OPENRCP_PAGE_COUNT:
        default:
            render_main_page(cam, buf, sizeof(buf));
            break;
    }

    if (model->shift_mode) {
        render_shift_soft_cells(cam);
    }
}

/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel.h>
#include <bluetooth/services/bas.h>

#include <logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/usb.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static lv_style_t label_style;

static bool style_initialized = false;

void battery_status_init() {
    if (style_initialized) {
        return;
    }

    style_initialized = true;
    lv_style_init(&label_style);
    lv_style_set_text_color(&label_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_text_font(&label_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_letter_space(&label_style, LV_STATE_DEFAULT, 1);
    lv_style_set_text_line_space(&label_style, LV_STATE_DEFAULT, 1);
}

K_MUTEX_DEFINE(battery_status_mutex);

struct {
    uint8_t level;
#if IS_ENABLED(CONFIG_USB)
    bool usb_present;
#endif
} battery_status_state;

void set_battery_symbol(lv_obj_t *label) {
    char text[2] = "  ";
    k_mutex_lock(&battery_status_mutex, K_FOREVER);

    uint8_t level = battery_status_state.level;

#if IS_ENABLED(CONFIG_USB)
    if (battery_status_state.usb_present) {
        strcpy(text, LV_SYMBOL_CHARGE);
    }
#endif /* IS_ENABLED(CONFIG_USB) */

    k_mutex_unlock(&battery_status_mutex);

    if (level > 95) {
        strcat(text, LV_SYMBOL_BATTERY_FULL);
    } else if (level > 65) {
        strcat(text, LV_SYMBOL_BATTERY_3);
    } else if (level > 35) {
        strcat(text, LV_SYMBOL_BATTERY_2);
    } else if (level > 5) {
        strcat(text, LV_SYMBOL_BATTERY_1);
    } else {
        strcat(text, LV_SYMBOL_BATTERY_EMPTY);
    }
    lv_label_set_text(label, text);
}

int zmk_widget_battery_status_init(struct zmk_widget_battery_status *widget, lv_obj_t *parent) {
    battery_status_init();
    widget->obj = lv_label_create(parent, NULL);
    lv_obj_add_style(widget->obj, LV_LABEL_PART_MAIN, &label_style);

    lv_obj_set_size(widget->obj, 40, 15);
    set_battery_symbol(widget->obj);

    sys_slist_append(&widgets, &widget->node);

    return 0;
}

lv_obj_t *zmk_widget_battery_status_obj(struct zmk_widget_battery_status *widget) {
    return widget->obj;
}

void battery_status_update_cb(struct k_work *work) {
    struct zmk_widget_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj); }
}

K_WORK_DEFINE(battery_status_update_work, battery_status_update_cb);

int battery_status_listener(const zmk_event_t *eh) {
    k_mutex_lock(&battery_status_mutex, K_FOREVER);

    battery_status_state.level = bt_bas_get_battery_level();

#if IS_ENABLED(CONFIG_USB)
    battery_status_state.usb_present = zmk_usb_is_powered();
#endif /* IS_ENABLED(CONFIG_USB) */

    k_mutex_unlock(&battery_status_mutex);

    k_work_submit_to_queue(zmk_display_work_q(), &battery_status_update_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_battery_status, battery_status_listener)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB) */

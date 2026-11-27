//! The controls of 1.5.3's editor, painted as `Source/UI/{Panel,KeyButton,Knob,FloatingModule}.h` and
//! JUCE's corner resizer and popup menu paint them. Each function paints one component in its own
//! coordinates; `w` and `h` are the component's size.

// The painters take what 1.5.3's paint methods read from their component, one argument each.
#![allow(clippy::too_many_arguments)]

use super::theme::Theme;
use crate::gfx::*;

pub fn c(argb: u32) -> Colour {
    Colour(argb)
}

// ---- Panel ----------------------------------------------------------------------------------------

/// `Panel::paint` with a compact (22 px) header, or none when `title` is empty.
pub fn panel(g: &mut Graphics, t: &Theme, w: i32, h: i32, title: &str, accent: Colour) {
    let outer = Rect::new(0.0, 0.0, w as f32, h as f32).reduced(0.5);
    let b = outer.reduced(0.8);

    g.set_colour(c(t.panel_border_lo));
    g.fill_rounded_rectangle(outer, 5.0);

    g.set_gradient_fill(gradient(c(t.panel_top), b.x, b.y, c(t.panel_bottom), b.x, b.bottom()));
    g.fill_rounded_rectangle(b, 4.5);

    g.set_colour(c(if t.light_surface { 0x05000000 } else { 0x07ffffff }));
    let mut y = b.y as i32 + 3;
    while y < b.bottom() as i32 - 2 {
        g.draw_horizontal_line(y, b.x + 3.0, b.right() - 3.0);
        y += 4;
    }

    if !title.is_empty() {
        let hh = 22.0;
        let hdr = b.with_height(hh);
        g.set_gradient_fill(gradient(c(t.panel_header_top), hdr.x, hdr.y, c(t.panel_header_bottom), hdr.x, hdr.bottom()));
        g.fill_rounded_rectangle(hdr, 4.5);

        let lamp = accent;
        g.set_colour(lamp.with_alpha(0.28));
        g.fill_rounded_rectangle(Rect::new(6.5, 3.5, 6.0, 15.0), 2.5);
        g.set_colour(lamp);
        g.fill_rounded_rectangle(Rect::new(8.0, 4.8, 2.8, 12.4), 1.3);
        g.set_colour(lamp.interpolated_with(WHITE, if t.light_surface { 0.25 } else { 0.45 }));
        g.fill_rounded_rectangle(Rect::new(8.8, 5.8, 1.2, 10.4), 0.6);

        g.set_colour(c(t.text_bright));
        g.set_font(Font::new(10.0, true));
        g.draw_text_i(title, 15, 2, w - 20, 18, Just::CentredLeft);

        g.set_colour(c(if t.light_surface { 0x24000000 } else { 0x90000000 }));
        g.draw_horizontal_line((b.y + hh) as i32, b.x + 4.0, b.right() - 4.0);
        g.set_colour(c(t.panel_border_hi).with_alpha(if t.light_surface { 0.85 } else { 0.45 }));
        g.draw_horizontal_line((b.y + hh) as i32 + 1, b.x + 4.0, b.right() - 4.0);
    }

    if t.synth_grid {
        g.set_colour(c(t.panel_border_hi).with_alpha(0.22));
        g.draw_rounded_rectangle(b.expanded(0.4), 4.8, 2.2);
    }
    g.set_colour(c(t.panel_border_hi).with_alpha(if t.light_surface { 0.90 } else if t.synth_grid { 0.80 } else { 0.68 }));
    g.draw_rounded_rectangle(b, 4.5, 1.0);
}

// ---- KeyButton and DropdownButton -----------------------------------------------------------------

/// `KeyButton::paintButton`. `over` is JUCE's "highlighted": true while hovered or pressed.
pub fn key(g: &mut Graphics, t: &Theme, w: i32, h: i32, text: &str, lamp: Colour, on: bool, over: bool, down: bool, enabled: bool) {
    let alpha = if enabled { 1.0 } else { 0.42 };
    let outer = Rect::new(0.0, 0.0, w as f32, h as f32).reduced(0.8);
    g.set_colour(c(if t.light_surface { t.panel_border_lo } else { t.well_bg_bottom }).with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(outer, 4.2);

    let b = outer.reduced(1.0).translated(0.0, if down { 0.5 } else { 0.0 });
    let mut top = c(if over { t.button_hover_top } else { t.button_top });
    let mut bot = c(t.button_bottom);
    if on {
        top = top.interpolated_with(lamp, if t.light_surface { 0.12 } else { 0.20 });
        bot = bot.interpolated_with(if t.light_surface { lamp } else { lamp.darker(0.6) }, 0.18);
    }
    g.set_gradient_fill(gradient(top.with_multiplied_alpha(alpha), b.x, b.y, bot.with_multiplied_alpha(alpha), b.x, b.bottom()));
    g.fill_rounded_rectangle(b, 3.6);

    if on {
        g.set_colour(lamp.with_alpha(0.20 * alpha));
        g.draw_rounded_rectangle(b.expanded(0.5), 4.0, 2.0);
    }
    let border = if on {
        lamp.with_alpha(0.85)
    } else if over {
        c(t.accent_primary).with_alpha(0.65)
    } else {
        c(t.button_border)
    };
    g.set_colour(border.with_multiplied_alpha(alpha));
    g.draw_rounded_rectangle(b, 3.6, if down { 1.4 } else { 1.0 });

    let slot_w = 36.0f32.min(b.w * 0.48);
    let slot_h = if b.h <= 23.0 { 2.1 } else { 2.6 };
    let slot = Rect::new(b.centre_x() - slot_w * 0.5, b.y + 2.8, slot_w, slot_h);
    g.set_colour(c(if t.light_surface { 0xffcdc8be } else { 0xff040608 }).with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(slot.expanded(0.6), slot_h * 0.5);
    if on {
        g.set_colour(lamp.with_alpha(0.34 * alpha));
        g.fill_rounded_rectangle(slot.expanded_xy(2.6, 1.8), slot_h);
        g.set_colour(lamp.with_multiplied_alpha(alpha));
        g.fill_rounded_rectangle(slot, slot_h * 0.5);
        g.set_colour(lamp.interpolated_with(WHITE, if t.light_surface { 0.35 } else { 0.58 }).with_multiplied_alpha(alpha));
        g.fill_rounded_rectangle(slot.reduced_xy(slot_w * 0.16, 0.45), slot_h * 0.4);
    } else {
        g.set_colour(lamp.with_alpha(if t.light_surface { 0.24 } else { 0.18 }).with_multiplied_alpha(alpha));
        g.fill_rounded_rectangle(slot, slot_h * 0.5);
    }

    let text_rect = IRect::new((b.x + 2.0) as i32, (b.y + 5.2) as i32, (b.w - 4.0) as i32, (b.h - 5.6) as i32);
    g.set_colour(c(if on { t.text_bright } else { t.text_primary }).with_multiplied_alpha(alpha));
    g.set_font(Font::new(if b.h <= 23.0 { 8.3 } else { 8.9 }, true));
    g.draw_fitted_text(text, text_rect, Just::Centred, 0.80);
}

/// `DropdownButton::paintButton`.
pub fn dropdown(g: &mut Graphics, t: &Theme, w: i32, h: i32, text: &str, accent: Colour, over: bool, down: bool, enabled: bool) {
    let alpha = if enabled { 1.0 } else { 0.50 };
    let outer = Rect::new(0.0, 0.0, w as f32, h as f32).reduced(0.6);
    g.set_colour(c(if t.light_surface { t.panel_border_lo } else { t.well_bg_bottom }).with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(outer, 4.4);

    let b = outer.reduced(1.0).translated(0.0, if down { 0.4 } else { 0.0 });
    let top = c(if over { t.button_hover_top } else { t.button_top });
    let bot = c(t.button_bottom);
    g.set_gradient_fill(gradient(top.with_multiplied_alpha(alpha), b.x, b.y, bot.with_multiplied_alpha(alpha), b.x, b.bottom()));
    g.fill_rounded_rectangle(b, 3.8);

    if over || down {
        g.set_colour(accent.with_alpha(0.22 * alpha));
        g.draw_rounded_rectangle(b.expanded(0.5), 4.2, 2.0);
    }
    g.set_colour(if over || down { accent.with_alpha(0.88) } else { c(t.button_border) }.with_multiplied_alpha(alpha));
    g.draw_rounded_rectangle(b, 3.8, 1.05);

    g.set_colour(accent.with_alpha(0.28 * alpha));
    g.fill_rounded_rectangle(Rect::new(b.x + 2.8, b.y + 4.0, 5.0, b.h - 8.0), 2.0);
    g.set_colour(accent.with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(Rect::new(b.x + 4.0, b.y + 5.0, 2.5, b.h - 10.0), 1.2);

    let zone = if b.w < 68.0 { 17.0 } else { 21.0 };
    let sep = b.right() - zone;
    g.set_colour(c(if t.light_surface { 0x26000000 } else { 0x88000000 }).with_multiplied_alpha(alpha));
    g.draw_vertical_line(sep as i32, b.y + 3.5, b.bottom() - 3.5);
    g.set_colour(c(t.panel_border_hi).with_alpha((if t.light_surface { 0.80 } else { 0.35 }) * alpha));
    g.draw_vertical_line(sep as i32 + 1, b.y + 3.5, b.bottom() - 3.5);

    let (cx, cy) = (sep + zone * 0.5, b.centre_y());
    let mut chevron = Path::new();
    chevron.start_new_sub_path(cx - 3.6, cy - 1.8);
    chevron.line_to(cx, cy + 2.2);
    chevron.line_to(cx + 3.6, cy - 1.8);
    let chev = if over { if t.light_surface { accent } else { accent.brighter(0.35) } } else { c(t.text_primary) };
    g.set_colour(chev.with_multiplied_alpha(alpha));
    g.stroke_path(&chevron, Stroke::curved_rounded(1.6));

    g.set_colour(c(t.text_bright).with_multiplied_alpha(alpha));
    g.set_font(Font::new(10.0, true));
    let text_rect = IRect::new((b.x + 10.0) as i32, b.y as i32, 16.max((sep - b.x - 13.0) as i32), b.h as i32);
    g.draw_fitted_text(text, text_rect, Just::CentredLeft, 0.80);
}

// ---- Knob ----------------------------------------------------------------------------------------

/// What one `Knob` paints with.
pub struct KnobLook<'a> {
    pub label: &'a str,
    pub text: &'a str,
    pub design: Colour,
    pub secondary: bool,
    pub position: f32,
    pub hovered: bool,
    pub dragging: bool,
    pub enabled: bool,
}

fn segment_colour(t: &Theme, base: Colour, f: f32) -> Colour {
    let safe = base.interpolated_with(c(t.lamp_safe), 0.35);
    let (warn, hot) = (c(t.lamp_warn), c(t.lamp_hot));
    if f < 0.60 {
        return safe.interpolated_with(warn, ((f - 0.38) / 0.22).clamp(0.0, 1.0) * 0.35);
    }
    if f < 0.86 {
        return safe.interpolated_with(warn, 0.35 + 0.65 * ((f - 0.60) / 0.26));
    }
    warn.interpolated_with(hot, ((f - 0.86) / 0.14).clamp(0.0, 1.0))
}

fn ghost_colour(t: &Theme, f: f32) -> Colour {
    c(if f < 0.60 {
        t.lamp_ghost_safe
    } else if f < 0.86 {
        t.lamp_ghost_warn
    } else {
        t.lamp_ghost_hot
    })
}

fn knob_accent(t: &Theme, k: &KnobLook) -> Colour {
    let theme_accent = c(if k.secondary { t.accent_secondary } else { t.accent_primary });
    theme_accent.interpolated_with(k.design, 0.14).with_multiplied_alpha(if k.enabled { 1.0 } else { 0.42 })
}

pub const THUMB_RADIUS: i32 = 6;

/// `Knob::resized` for a linear slider: header (top 16) and track.
pub fn slider_areas(w: i32, h: i32) -> (IRect, IRect) {
    let mut b = IRect::new(0, 0, w, h);
    let header = b.remove_from_top(16);
    (header, b)
}

/// `Knob::resized` for a rotary dial: dial, header (readout).
pub fn rotary_areas(w: i32, h: i32) -> (IRect, IRect) {
    let mut b = IRect::new(0, 0, w, h);
    let side = (b.h - 2).min(62.min(b.w / 2));
    if b.w >= side + 56 {
        let dial = IRect::new(b.x + 2, b.centre_y() - side / 2, side, side);
        let header = IRect::new(dial.right() + 8, b.centre_y() - 22, b.right() - dial.right() - 10, 44);
        (dial, header)
    } else {
        let header = b.remove_from_top(14);
        (b.reduced(2, 2), header)
    }
}

pub fn horizontal_slider(g: &mut Graphics, t: &Theme, w: i32, h: i32, k: &KnobLook) {
    let accent = knob_accent(t, k);
    let alpha = if k.enabled { 1.0 } else { 0.45 };
    let (header, track) = slider_areas(w, h);
    let readout_w = (if w <= 96 { w - 34 } else { w / 2 - 2 }).clamp(40, 58);
    let mut top_row = header;
    let value_box = top_row.remove_from_right(readout_w).to_float().reduced(0.4);
    let label_box = top_row.with_trimmed_right(2);
    let lit = k.hovered || k.dragging;

    g.set_colour(if k.enabled { c(t.text_bright) } else { c(t.text_muted).with_alpha(0.65) });
    g.set_font(Font::new(if w <= 96 { 8.5 } else { 9.5f32.clamp(8.2, 10.0) }, true));
    g.draw_fitted_text(k.label, label_box, Just::CentredLeft, 0.78);

    g.set_colour(c(t.oled_bg).with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(value_box, 2.8);
    g.set_colour(if lit { accent.with_alpha(0.80) } else { c(t.oled_border) }.with_multiplied_alpha(alpha));
    g.draw_rounded_rectangle(value_box, 2.8, 0.85);

    let digit = if t.light_surface {
        c(t.oled_text).interpolated_with(accent.darker(0.25), 0.28)
    } else {
        accent.interpolated_with(c(t.oled_text), 0.50).brighter(0.22)
    };
    g.set_colour(if k.enabled { digit } else { c(t.text_muted).with_alpha(0.65) });
    g.set_font(Font::new(if w <= 96 { 8.5 } else { 9.1 }, true));
    g.draw_fitted_text(k.text, value_box.to_nearest_int().reduced(2, 0), Just::Centred, 0.76);

    let half = THUMB_RADIUS as f32;
    let x0 = track.x as f32 + half;
    let x1 = (x0 + 1.0).max(track.right() as f32 - half);
    let cy = track.centre_y() as f32;
    let pos = k.position.clamp(0.0, 1.0);
    let thumb_x = x0 + pos * (x1 - x0);
    let groove_h = 6.4f32;
    let groove = Rect::new(x0, cy - groove_h * 0.5, x1 - x0, groove_h);

    g.set_colour(c(t.text_muted).with_alpha(0.50 * alpha));
    for i in 0..=4 {
        let tx = x0 + (x1 - x0) * (i as f32 / 4.0);
        let len = if i == 0 || i == 2 || i == 4 { 3.8 } else { 2.3 };
        g.draw_line(tx, groove.bottom() + 1.8, tx, groove.bottom() + 1.8 + len, 0.85);
    }

    g.set_colour(if t.light_surface { c(0xffd8d4ca) } else { c(t.well_bg_bottom) });
    g.fill_rounded_rectangle(groove.expanded(0.6), (groove_h + 1.2) * 0.5);
    g.set_colour(if t.light_surface { c(0xffb6b1a5).with_multiplied_alpha(alpha) } else { c(t.panel_border_hi).with_alpha(0.55 * alpha) });
    g.draw_rounded_rectangle(groove, groove_h * 0.5, 0.85);

    if thumb_x > x0 + 0.6 {
        let fill = Rect::new(x0, groove.y + 0.6, thumb_x - x0, groove_h - 1.2);
        let end = segment_colour(t, accent, pos * 0.85).with_multiplied_alpha(alpha);
        g.set_colour(end.with_alpha((if t.light_surface { 0.16 } else { 0.24 }) * alpha));
        g.fill_rounded_rectangle(fill.expanded_xy(1.4, 1.6), groove_h * 0.6);
        g.set_gradient_fill(gradient(
            accent.darker(if t.light_surface { 0.10 } else { 0.25 }),
            fill.x,
            fill.y,
            if t.light_surface { end } else { end.brighter(0.20) },
            fill.right(),
            fill.y,
        ));
        g.fill_rounded_rectangle(fill, (groove_h - 1.2) * 0.5);
    }

    let cap_w = 11.5f32;
    let cap_h = 18.0f32.min(13.5f32.max(track.h as f32 - 6.0));
    let cap = Rect::new(0.0, 0.0, cap_w, cap_h).with_centre(thumb_x, cy);

    g.set_colour(c(if t.light_surface { 0x2c000000 } else { 0x88000000 }));
    g.fill_rounded_rectangle(cap.translated(0.0, 1.5), 2.6);
    g.set_gradient_fill(gradient(
        c(t.rotor_outer_hi).with_multiplied_alpha(alpha),
        cap.x,
        cap.y,
        c(t.rotor_outer_lo).with_multiplied_alpha(alpha),
        cap.x,
        cap.bottom(),
    ));
    g.fill_rounded_rectangle(cap, 2.6);
    let rim = if lit {
        accent.with_alpha(0.90)
    } else if t.light_surface {
        c(0xff948e83)
    } else {
        c(t.rotor_flute_hi).with_alpha(0.85)
    };
    g.set_colour(rim.with_multiplied_alpha(alpha));
    g.draw_rounded_rectangle(cap, 2.6, 0.95);

    g.set_colour(accent.with_alpha((if t.light_surface { 0.22 } else { 0.36 }) * alpha));
    g.fill_rounded_rectangle(Rect::new(thumb_x - 2.3, cap.y + 1.8, 4.6, cap.h - 3.6), 1.6);
    g.set_colour(if t.light_surface { accent } else { accent.brighter(0.35) }.with_multiplied_alpha(alpha));
    g.fill_rounded_rectangle(Rect::new(thumb_x - 1.0, cap.y + 2.4, 2.0, cap.h - 4.8), 0.9);
    g.set_colour(if t.light_surface { c(0xff090c10) } else { WHITE }.with_alpha(0.82 * alpha));
    g.fill_rect(Rect::new(thumb_x - 0.35, cap.y + 3.2, 0.7, cap.h - 6.4));
}

pub fn rotary(g: &mut Graphics, t: &Theme, w: i32, h: i32, k: &KnobLook) {
    use std::f32::consts::PI;
    let accent = knob_accent(t, k);
    let alpha = if k.enabled { 1.0 } else { 0.45 };
    let lit = k.hovered || k.dragging;
    let (dial, header) = rotary_areas(w, h);
    let diameter = dial.w.min(dial.h) as f32;
    let (cx, cy) = (dial.to_float().centre_x(), dial.to_float().centre_y());
    let r = diameter * 0.5;
    let pos = k.position.clamp(0.0, 1.0);
    let (lo, hi) = (-0.80 * PI, 0.80 * PI);
    let cur = lo + pos * (hi - lo);
    let pt = |ang: f32, rad: f32| (cx + ang.sin() * rad, cy - ang.cos() * rad);

    // 1. housing dish
    let r_dish = r * 0.985;
    g.set_gradient_fill(gradient(
        if t.light_surface { c(0xffdedad0) } else { c(t.well_bg_bottom) },
        cx,
        cy - r_dish,
        if t.light_surface { c(0xffece9e2) } else { c(t.well_bg_top) },
        cx,
        cy + r_dish,
    ));
    g.fill_ellipse(Rect::new(cx - r_dish, cy - r_dish, r_dish * 2.0, r_dish * 2.0));
    g.set_colour(if t.light_surface { c(0xffbdb8ad) } else { c(t.panel_border_hi).with_alpha(0.60) });
    g.draw_ellipse(Rect::new(cx - r_dish, cy - r_dish, r_dish * 2.0, r_dish * 2.0), 1.0);

    // 2. 25-segment lamp arc
    const SEGS: usize = 25;
    let (r_in, r_out) = (r * 0.745, r * 0.935);
    let sweep = hi - lo;
    let step = sweep / (SEGS - 1) as f32;
    let half_seg = step * 0.34;
    let tip = segment_colour(t, accent, pos).with_multiplied_alpha(alpha);

    if pos > 0.005 && k.enabled {
        let mut glow = Path::new();
        let mid = (r_in + r_out) * 0.5;
        glow.add_centred_arc(cx, cy, mid, mid, lo, cur, true);
        g.set_colour(tip.with_alpha(if t.light_surface {
            if lit { 0.16 } else { 0.10 }
        } else if lit {
            0.28
        } else {
            0.19
        }));
        g.stroke_path(&glow, Stroke::curved_rounded(r * 0.25));
    }

    let mut last_lit: i32 = -1;
    for i in 0..SEGS {
        let f = i as f32 / (SEGS - 1) as f32;
        if (i == 0 && pos > 0.002) || (i > 0 && f <= pos + 0.012) {
            last_lit = i as i32;
        }
    }
    for i in 0..SEGS {
        let f = i as f32 / (SEGS - 1) as f32;
        let ang = lo + f * sweep;
        let (a0, a1) = (ang - half_seg, ang + half_seg);
        let mut seg = Path::new();
        let p = pt(a0, r_in);
        seg.start_new_sub_path(p.0, p.1);
        let p = pt(a0, r_out);
        seg.line_to(p.0, p.1);
        let p = pt(a1, r_out);
        seg.line_to(p.0, p.1);
        let p = pt(a1, r_in);
        seg.line_to(p.0, p.1);
        seg.close_sub_path();
        if (i as i32) <= last_lit {
            let mut col = segment_colour(t, accent, f).with_multiplied_alpha(alpha);
            if i as i32 == last_lit {
                col = if t.light_surface { col.darker(0.20) } else { col.interpolated_with(WHITE, 0.50) };
            }
            g.set_colour(col);
        } else {
            g.set_colour(ghost_colour(t, f).with_multiplied_alpha(alpha));
        }
        g.fill_path(&seg);
    }

    // 3. moat
    let r_moat = r * 0.70;
    g.set_colour(c(if t.light_surface { 0xffbab4a8 } else { 0xff020406 }));
    g.fill_ellipse(Rect::new(cx - r_moat, cy - r_moat, r_moat * 2.0, r_moat * 2.0));

    // 4. 12-lobe turbine rotor
    let r_rotor = r * 0.655;
    let mut turbine = Path::new();
    for s in 0..72 {
        let theta = 2.0 * PI * s as f32 / 72.0;
        let lobe = 0.952 + 0.048 * (12.0 * (theta - cur)).cos();
        let p = pt(theta, r_rotor * lobe);
        if s == 0 {
            turbine.start_new_sub_path(p.0, p.1);
        } else {
            turbine.line_to(p.0, p.1);
        }
    }
    turbine.close_sub_path();

    g.set_colour(c(if t.light_surface { 0x2e000000 } else { 0x95000000 }));
    g.fill_path_transformed(&turbine, Affine::translation(0.9, 1.9));
    g.set_gradient_fill(gradient(
        c(t.rotor_outer_hi).with_multiplied_alpha(alpha),
        cx - r_rotor * 0.8,
        cy - r_rotor * 0.8,
        c(t.rotor_outer_lo).with_multiplied_alpha(alpha),
        cx + r_rotor * 0.85,
        cy + r_rotor * 0.85,
    ));
    g.fill_path(&turbine);

    for f in 0..36 {
        let fa = cur + 2.0 * PI * f as f32 / 36.0;
        let col = if f % 2 == 0 {
            c(t.rotor_flute_hi).with_alpha(if t.light_surface { 0.72 } else { 0.52 })
        } else {
            c(t.rotor_flute_lo).with_alpha(if t.light_surface { 0.62 } else { 0.78 })
        };
        g.set_colour(col.with_multiplied_alpha(alpha));
        let (a, b) = (pt(fa, r_rotor * 0.73), pt(fa, r_rotor * 0.91));
        g.draw_line(a.0, a.1, b.0, b.1, 1.05);
    }

    g.set_gradient_fill(gradient(
        c(t.rotor_flute_hi).with_alpha(0.85 * alpha),
        cx - r_rotor,
        cy - r_rotor,
        if t.light_surface { c(0x90706b61) } else { c(0xd0000000) },
        cx + r_rotor,
        cy + r_rotor,
    ));
    g.stroke_path(&turbine, Stroke::new(1.05));

    // 5. collar and core cap
    let r_collar = r * 0.465;
    g.set_colour(c(if t.light_surface { 0xffc7c2b7 } else { 0xff040609 }));
    g.fill_ellipse(Rect::new(cx - r_collar - 1.2, cy - r_collar - 1.2, (r_collar + 1.2) * 2.0, (r_collar + 1.2) * 2.0));
    g.set_colour(tip.with_alpha((if lit { 0.36 } else { 0.22 }) * alpha));
    g.draw_ellipse(Rect::new(cx - r_collar, cy - r_collar, r_collar * 2.0, r_collar * 2.0), 3.0);
    g.set_colour(tip.with_alpha((if lit { 0.95 } else { 0.78 }) * alpha));
    g.draw_ellipse(Rect::new(cx - r_collar, cy - r_collar, r_collar * 2.0, r_collar * 2.0), 1.15);

    let r_cap = r * 0.415;
    g.set_gradient_fill(gradient(
        c(t.rotor_cap_hi).with_multiplied_alpha(alpha),
        cx - r_cap * 0.7,
        cy - r_cap * 0.7,
        c(t.rotor_cap_lo).with_multiplied_alpha(alpha),
        cx + r_cap * 0.8,
        cy + r_cap * 0.8,
    ));
    g.fill_ellipse(Rect::new(cx - r_cap, cy - r_cap, r_cap * 2.0, r_cap * 2.0));

    for ring in 1..=3 {
        let rr = r_cap * ring as f32 / 3.5;
        g.set_colour(c(if ring % 2 == 1 {
            if t.light_surface { 0x34ffffff } else { 0x16ffffff }
        } else if t.light_surface {
            0x12000000
        } else {
            0x22000000
        }));
        g.draw_ellipse(Rect::new(cx - rr, cy - rr, rr * 2.0, rr * 2.0), 0.55);
    }

    let mut gloss = Path::new();
    gloss.add_centred_arc(cx, cy, r_cap * 0.84, r_cap * 0.84, -0.78 * PI, 0.12 * PI, true);
    g.set_colour(WHITE.with_alpha((if t.light_surface { 0.55 } else { 0.18 }) * alpha));
    g.stroke_path(&gloss, Stroke::curved_rounded(1.3));

    // 6. pointer blade and hub
    let p_in = pt(cur, r * 0.10);
    let p_out = pt(cur, r_rotor * 0.95);
    g.set_colour(tip.with_alpha((if t.light_surface { 0.20 } else { 0.34 }) * alpha));
    g.draw_line(p_in.0, p_in.1, p_out.0, p_out.1, if t.light_surface { 4.2 } else { 5.0 });
    g.set_colour(if t.light_surface { tip } else { tip.brighter(0.32) });
    g.draw_line(p_in.0, p_in.1, p_out.0, p_out.1, 2.3);
    g.set_colour(if t.light_surface { c(0xff090c10) } else { WHITE }.with_alpha(0.92 * alpha));
    let (a, b) = (pt(cur, r * 0.14), pt(cur, r_rotor * 0.90));
    g.draw_line(a.0, a.1, b.0, b.1, 0.95);

    let jewel = pt(cur, r_rotor * 0.88);
    g.set_colour(tip.with_alpha((if t.light_surface { 0.30 } else { 0.45 }) * alpha));
    g.fill_ellipse(Rect::new(jewel.0 - 3.2, jewel.1 - 3.2, 6.4, 6.4));
    g.set_colour(if t.light_surface { tip.darker(0.25) } else { WHITE }.with_alpha(0.95 * alpha));
    g.fill_ellipse(Rect::new(jewel.0 - 1.3, jewel.1 - 1.3, 2.6, 2.6));

    let r_hub = r * 0.115;
    g.set_colour(if t.light_surface { c(0xffb5afa3) } else { c(t.rotor_cap_lo) });
    g.fill_ellipse(Rect::new(cx - r_hub, cy - r_hub, r_hub * 2.0, r_hub * 2.0));
    g.set_colour(if t.light_surface { c(0xff78736a) } else { c(t.rotor_flute_hi) }.with_alpha(0.75 * alpha));
    g.draw_ellipse(Rect::new(cx - r_hub, cy - r_hub, r_hub * 2.0, r_hub * 2.0), 0.8);

    // 7. readout console
    let mut info = header;
    let label_row = info.remove_from_top(17);
    info.remove_from_top(2);
    let oled = info.remove_from_top(24).to_float();

    let pip_y = label_row.centre_y() as f32 - 2.0;
    g.set_colour(tip.with_alpha(0.32 * alpha));
    g.fill_ellipse(Rect::new(label_row.x as f32 - 1.2, pip_y - 1.2, 6.4, 6.4));
    g.set_colour(tip);
    g.fill_ellipse(Rect::new(label_row.x as f32, pip_y, 4.0, 4.0));

    g.set_colour(c(if k.enabled { t.text_bright } else { t.text_muted }));
    g.set_font(Font::new(9.8, true));
    g.draw_fitted_text(k.label, label_row.with_trimmed_left(8), Just::CentredLeft, 0.84);

    g.set_colour(c(t.oled_bg));
    g.fill_rounded_rectangle(oled, 3.8);
    g.set_colour(c(if t.light_surface { 0x06000000 } else { 0x0cffffff }));
    let mut sy = oled.y as i32 + 3;
    while sy < oled.bottom() as i32 - 3 {
        g.draw_horizontal_line(sy, oled.x + 3.0, oled.right() - 3.0);
        sy += 3;
    }

    if pos > 0.005 {
        let bar_w = 4.0f32.max(oled.w - 8.0);
        g.set_colour(tip.with_alpha(0.82 * alpha));
        g.fill_rounded_rectangle(Rect::new(oled.x + 4.0, oled.bottom() - 3.3, bar_w * pos, 1.7), 0.8);
    }
    if lit {
        g.set_colour(tip.with_alpha(0.24 * alpha));
        g.draw_rounded_rectangle(oled.expanded(0.6), 4.2, 2.2);
    }
    g.set_colour(if lit { tip.with_alpha(0.88) } else { c(t.oled_border) }.with_multiplied_alpha(alpha));
    g.draw_rounded_rectangle(oled, 3.8, 1.0);

    let text_box = oled.with_trimmed_bottom(2.0).to_nearest_int().reduced(4, 0);
    let oled_col = if t.light_surface {
        c(t.oled_text).interpolated_with(tip.darker(0.25), 0.25)
    } else {
        tip.interpolated_with(c(t.oled_text), 0.45).brighter(0.28)
    };
    g.set_font(Font::new(10.6, true));
    if k.enabled && !t.light_surface {
        g.set_colour(oled_col.with_alpha(0.25));
        g.draw_fitted_text(k.text, text_box.translated(0, 1), Just::Centred, 0.80);
    }
    g.set_colour(if k.enabled { oled_col } else { c(t.text_muted) });
    g.draw_fitted_text(k.text, text_box, Just::Centred, 0.80);
}

// ---- FloatingModule -------------------------------------------------------------------------------

pub const HEADER: i32 = 28;

pub struct ModuleLook<'a> {
    pub title: &'a str,
    pub summary: &'a str,
    pub accent: Colour,
    pub open: bool,
    pub highlighted: bool,
    pub hover_header: bool,
}

pub fn module(g: &mut Graphics, t: &Theme, w: i32, h: i32, m: &ModuleLook) {
    let b = Rect::new(0.0, 0.0, w as f32, h as f32).reduced(1.0);
    let accent = m.accent;

    g.set_colour(c(if t.light_surface {
        if m.open { 0x26000000 } else { 0x1a000000 }
    } else if m.open {
        0x90000000
    } else {
        0x70000000
    }));
    g.fill_rounded_rectangle(b.translated(0.0, 2.2), 5.5);

    let mut bg = c(if m.open { t.hud_open_bg } else { t.hud_closed_bg });
    if m.hover_header && !m.open {
        bg = if t.light_surface { c(0xfffdfcfa) } else { bg.brighter(0.12) };
    }
    g.set_colour(bg);
    g.fill_rounded_rectangle(b, 5.0);

    let header_rect = b.with_height(HEADER as f32 - if m.open { 0.0 } else { 2.0 });
    if m.open {
        g.set_colour(c(t.hud_header_bg));
        g.fill_rounded_rectangle(header_rect, 5.0);
        g.set_colour(c(if t.light_surface { 0x24000000 } else { 0xa0000000 }));
        g.draw_horizontal_line(HEADER, b.x + 2.0, b.right() - 2.0);
        g.set_colour(accent.with_alpha(if t.light_surface { 0.22 } else { 0.30 }));
        g.draw_horizontal_line(HEADER + 1, b.x + 2.0, b.right() - 2.0);
    }

    let lit = m.open || m.highlighted;
    if lit {
        g.set_colour(accent.with_alpha(if t.light_surface { 0.16 } else { 0.24 }));
        g.draw_rounded_rectangle(b.expanded(0.5), 5.4, 2.6);
    }
    g.set_colour(if lit {
        accent.with_alpha(if t.light_surface { 0.78 } else { 0.90 })
    } else if m.hover_header {
        accent.with_alpha(0.58)
    } else {
        c(t.hud_border)
    });
    g.draw_rounded_rectangle(b, 5.0, if lit { 1.35 } else { 1.0 });

    g.set_colour(c(t.text_muted).with_alpha(0.80));
    for row in 0..3 {
        for col in 0..2 {
            let dx = b.x + 6.5 + col as f32 * 3.5;
            let dy = b.y + 8.5 + row as f32 * 4.2;
            g.fill_ellipse(Rect::new(dx, dy, 1.8, 1.8));
        }
    }

    g.set_colour(accent.with_alpha(if lit { 0.34 } else { 0.20 }));
    g.fill_rounded_rectangle(Rect::new(b.x + 14.5, b.y + 5.0, 5.2, HEADER as f32 - 10.0), 2.2);
    g.set_colour(if lit {
        if t.light_surface { accent } else { accent.brighter(0.2) }
    } else {
        accent.with_alpha(0.88)
    });
    g.fill_rounded_rectangle(Rect::new(b.x + 15.8, b.y + 6.2, 2.6, HEADER as f32 - 12.4), 1.3);

    let chev_x = b.right() - 12.5;
    let chev_y = HEADER as f32 * 0.5;
    let mut chev = Path::new();
    if m.open {
        chev.start_new_sub_path(chev_x - 3.6, chev_y - 1.7);
        chev.line_to(chev_x, chev_y + 2.3);
        chev.line_to(chev_x + 3.6, chev_y - 1.7);
    } else {
        chev.start_new_sub_path(chev_x - 1.7, chev_y - 3.6);
        chev.line_to(chev_x + 2.3, chev_y);
        chev.line_to(chev_x - 1.7, chev_y + 3.6);
    }
    g.set_colour(if lit || m.hover_header {
        if t.light_surface { accent } else { accent.brighter(0.35) }
    } else {
        c(t.text_primary)
    });
    g.stroke_path(&chev, Stroke::curved_rounded(1.55));

    g.set_colour(c(t.text_bright));
    let title_font = Font::new(9.3, true);
    g.set_font(title_font);
    let title_x = b.x as i32 + 21;
    let measured = Graphics::text_width(m.title, title_font).ceil() as i32 + 4;
    let title_w = measured.clamp(44, 84);
    g.draw_fitted_text(m.title, IRect::new(title_x, 0, title_w, HEADER), Just::CentredLeft, 0.85);

    if !m.summary.is_empty() {
        let sum_x = title_x + title_w + 3;
        let sum_w = 26.max(w - sum_x - 20);
        let sum_box = Rect::new(sum_x as f32, 5.5, sum_w as f32, HEADER as f32 - 11.0);
        g.set_colour(c(t.oled_bg).with_alpha(if t.light_surface { 0.94 } else { 0.86 }));
        g.fill_rounded_rectangle(sum_box, 3.0);
        g.set_colour(if lit { accent.with_alpha(0.45) } else { c(t.oled_border).with_alpha(0.82) });
        g.draw_rounded_rectangle(sum_box, 3.0, 0.75);
        let col = if t.light_surface {
            if m.highlighted { accent.darker(0.20).interpolated_with(c(t.oled_text), 0.35) } else { c(t.oled_text).with_alpha(0.92) }
        } else if m.highlighted {
            accent.interpolated_with(c(t.oled_text), 0.35).brighter(0.25)
        } else {
            c(t.oled_text).with_alpha(0.90)
        };
        g.set_colour(col);
        g.set_font(Font::new(8.2, true));
        g.draw_fitted_text(m.summary, sum_box.to_nearest_int().reduced(3, 0), Just::CentredRight, 0.72);
    }
}

// ---- JUCE's corner resizer ------------------------------------------------------------------------

/// `LookAndFeel_V2::drawCornerResizer`, the 18 x 18 corner of a resizable editor.
pub fn corner_resizer(g: &mut Graphics, w: i32, h: i32) {
    let (w, h) = (w as f32, h as f32);
    let thick = w.min(h) * 0.075;
    let mut i = 0.0f32;
    while i < 1.0 {
        g.set_colour(LIGHT_GREY);
        g.draw_line(w * i, h + 1.0, w + 1.0, h * i, thick);
        g.set_colour(DARK_GREY);
        g.draw_line(w * i + thick, h + 1.0, w + 1.0, h * i + thick, thick);
        i += 0.3;
    }
}

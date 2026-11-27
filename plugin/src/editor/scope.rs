//! The chassis, the lamp meters, the Compression History field and its transfer curve, as
//! `PluginEditor.cpp` (`Surface::paint`), `Source/UI/MeterPanel.h` and `TransferCurveDisplay.cpp` draw them.

use super::theme::Theme;
use super::widgets::{c, panel};
use crate::gfx::*;
use crate::params::whole;
use crate::{HistoryFrame, METER_VU, Meters, VU_REFERENCE_DBFS};

/// A readout that changes every frame, in whole numbers, signed; -0 reads 0.
pub fn signed(v: f32) -> String {
    let n = round_to_int(v);
    format!("{}{n}", if n > 0 { "+" } else { "" })
}

const DBU_OFFSET: f32 = 18.0;

/// `juce::Decibels::gainToDecibels (gain, floor)`.
fn gain_to_db(gain: f32, floor: f32) -> f32 {
    if gain > 0.0 { floor.max(gain.log10() * 20.0) } else { floor }
}

/// `juce::String (v, decimals)` with decimals > 0.
pub fn fixed(v: f32, decimals: usize) -> String {
    format!("{:.*}", decimals, f64::from(v))
}

// ---- Surface --------------------------------------------------------------------------------------

pub fn surface(g: &mut Graphics, t: &Theme, w: f32, h: f32) {
    g.set_gradient_fill(gradient(c(t.chassis_top), 0.0, 0.0, c(t.chassis_bottom), 0.0, h));
    g.fill_rect(Rect::new(0.0, 0.0, w, h));

    let mut y = 2;
    while y < h as i32 {
        g.set_colour(c(if t.light_surface {
            if y % 6 == 2 { 0x09ffffff } else { 0x04000000 }
        } else if y % 6 == 2 {
            0x06ffffff
        } else {
            0x0b000000
        }));
        g.draw_horizontal_line(y, 0.0, w);
        y += 3;
    }

    g.set_gradient_fill(gradient(c(t.panel_header_top).with_alpha(0.85), 0.0, 0.0, c(t.chassis_top).with_alpha(0.45), 0.0, 44.0));
    g.fill_rect(Rect::new(0.0, 0.0, w, 44.0));

    if t.synth_grid {
        g.set_gradient_fill(gradient(c(t.accent_primary).with_alpha(0.75), 12.0, 44.0, c(t.accent_secondary).with_alpha(0.75), w - 12.0, 44.0));
        g.fill_rect(Rect::new(12.0, 43.0, w - 24.0, 1.5));
        g.set_gradient_fill(gradient(
            c(t.accent_secondary).with_alpha(0.55),
            12.0,
            h - 2.0,
            c(t.accent_primary).with_alpha(0.55),
            w - 12.0,
            h - 2.0,
        ));
        g.fill_rect(Rect::new(12.0, h - 2.2, w - 24.0, 1.4));
    } else {
        g.set_colour(c(if t.light_surface { 0x22000000 } else { 0xa0000000 }));
        g.draw_horizontal_line(43, 12.0, w - 12.0);
        g.set_colour(c(t.panel_border_hi).with_alpha(if t.light_surface { 0.85 } else { 0.40 }));
        g.draw_horizontal_line(44, 12.0, w - 12.0);
    }

    let brand = c(if t.light_surface { t.lamp_hot } else { t.accent_primary });
    g.set_colour(brand.with_alpha(if t.light_surface { 0.22 } else { 0.30 }));
    g.fill_rounded_rectangle(Rect::new(11.5, 8.5, 7.5, 27.0), 3.0);
    g.set_colour(brand);
    g.fill_rounded_rectangle(Rect::new(13.5, 10.0, 3.5, 24.0), 1.7);
    g.set_colour(brand.interpolated_with(WHITE, if t.light_surface { 0.30 } else { 0.55 }));
    g.fill_rounded_rectangle(Rect::new(14.4, 11.5, 1.6, 21.0), 0.8);

    g.set_font(Font::new(16.0, true));
    g.set_colour(c(t.text_bright));
    g.draw_text_i("HARMONIC COMPRESSOR", 23, 6, 248, 19, Just::CentredLeft);

    g.set_font(Font::new(8.5, true));
    g.set_colour(c(t.text_muted));
    g.draw_text_i("0 dBu = -18 dBFS  /  ADAA + MULTIRATE", 24, 25, 248, 13, Just::CentredLeft);

    g.set_font(Font::new(8.3, true));
    g.set_colour(c(t.pre_stroke));
    g.draw_line(276.0, 22.0, 288.0, 22.0, 2.0);
    g.set_colour(c(t.text_primary));
    g.draw_text_i("PRE", 291, 14, 26, 16, Just::CentredLeft);

    g.set_colour(c(t.post_glow).with_alpha(0.35));
    g.fill_rounded_rectangle(Rect::new(317.5, 16.5, 15.0, 11.0), 3.0);
    g.set_colour(c(t.post_stroke));
    g.fill_rounded_rectangle(Rect::new(319.0, 18.0, 12.0, 8.0), 2.0);
    g.set_colour(c(t.text_primary));
    g.draw_text_i("POST", 334, 14, 32, 16, Just::CentredLeft);

    g.set_colour(c(t.gr_stroke));
    g.draw_line(368.0, 22.0, 380.0, 22.0, 2.3);
    g.set_colour(c(t.text_primary));
    g.draw_text_i("GR", 383, 14, 20, 16, Just::CentredLeft);

    g.set_font(Font::new(8.6, true));
    g.set_colour(c(t.text_muted));
    g.draw_text_i("MODEL", 406, 11, 41, 22, Just::CentredRight);
    g.draw_text_i("OS", 605, 11, 20, 22, Just::CentredRight);
    g.draw_text_i("COLOR", 951, 11, 41, 22, Just::CentredRight);
}

// ---- MeterPanel -----------------------------------------------------------------------------------

/// `MeterPanel::paint`: 96 x 618, the input meter (L, R) or the output meter with GR, on the
/// `scale` the editor's selector chose (new: 1.5.3 had sample peaks only).
#[allow(clippy::too_many_arguments)] // JUCE's paint arguments plus the scale
pub fn meter_panel(g: &mut Graphics, t: &Theme, w: i32, h: i32, input: bool, accent: Colour, m: &Meters, scale: usize) {
    panel(g, t, w, h, if input { "INPUT" } else { "OUT / GR" }, accent);

    let lanes = if input { 2 } else { 3 };
    let top = if input { 44.0f32 } else { 58.0 };
    let bottom = (h - 50) as f32;
    let height = 60.0f32.max(bottom - top);
    let lane_w = if input { 11.5f32 } else { 10.5 };
    let left = 8.0f32;
    let raw_gr = (-m.gr_db()).clamp(0.0, 36.0);
    // the bars and the scale are in dBFS; a VU reading sits where its dBFS level is
    let vu = scale == METER_VU;
    let at = if input { 0 } else { 2 };
    let dbfs = |k: usize| m.level(scale, k) + if vu { VU_REFERENCE_DBFS } else { 0.0 };
    let values = [dbfs(at), dbfs(at + 1), if raw_gr < 0.05 { 0.0 } else { raw_gr }];

    if !input {
        let gr_oled = Rect::new(6.0, 25.0, (w - 12) as f32, 16.0);
        g.set_colour(c(t.oled_bg));
        g.fill_rounded_rectangle(gr_oled, 3.0);
        g.set_colour(c(t.gr_stroke).with_alpha(0.45));
        g.draw_rounded_rectangle(gr_oled, 3.0, 0.8);
        g.set_colour(if t.light_surface { c(t.gr_stroke).darker(0.15) } else { c(t.gr_stroke).brighter(0.20) });
        g.set_font(Font::new(8.6, true));
        g.draw_text(&format!("{} dB GR", round_to_int(values[2])), gr_oled.to_nearest_int().to_float(), Just::Centred);
    }

    g.set_colour(c(t.text_muted));
    g.set_font(Font::new(8.0, true));
    // the input meter's scale selector stands where this caption was
    if !input {
        g.draw_text_i(if vu { "VU/FS" } else { "dBu/FS" }, (left + lane_w * 2.0 + 8.0) as i32, top as i32 - 16, 42, 10, Just::CentredLeft);
    }

    let well_top = if t.light_surface { c(0xffe5e2da) } else { c(t.well_bg_top) };
    let well_bot = if t.light_surface { c(0xffddd9cf) } else { c(t.well_bg_bottom) };
    let lr = Rect::new(left - 2.5, top - 2.5, lane_w * 2.0 + 8.5, height + 5.0);
    g.set_gradient_fill(gradient(well_top, lr.x, lr.y, well_bot, lr.x, lr.bottom()));
    g.fill_rounded_rectangle(lr, 3.0);
    g.set_colour(c(t.well_bezel_outer).with_alpha(if t.light_surface { 0.85 } else { 0.65 }));
    g.draw_rounded_rectangle(lr, 3.0, 0.9);

    if !input {
        let gr_x = left + 2.0 * (lane_w + 3.5) + 41.5;
        let gw = Rect::new(gr_x - 2.5, top - 2.5, lane_w + 5.0, height + 5.0);
        g.set_gradient_fill(gradient(well_top, gw.x, gw.y, well_bot, gw.x, gw.bottom()));
        g.fill_rounded_rectangle(gw, 3.0);
        g.set_colour(c(t.well_bezel_outer).with_alpha(if t.light_surface { 0.85 } else { 0.65 }));
        g.draw_rounded_rectangle(gw, 3.0, 0.9);
    }

    const SEGMENTS: i32 = 44;
    let pitch = height / SEGMENTS as f32;
    let seg_h = 1.8f32.max(pitch - 1.5);

    for i in 0..lanes {
        let x = left + i as f32 * (lane_w + 3.5) + if i == 2 { 41.5 } else { 0.0 };
        let proportion = if i == 2 { (values[i] / 36.0).clamp(0.0, 1.0) } else { ((values[i] + 60.0) / 60.0).clamp(0.0, 1.0) };
        let lit_count = if proportion <= 0.002 { 0 } else { ((proportion * SEGMENTS as f32).ceil() as i32).clamp(1, SEGMENTS) };

        if lit_count > 0 {
            let lit_h = lit_count as f32 * pitch;
            let glow_box = Rect::new(x - 1.5, if i == 2 { top } else { bottom - lit_h }, lane_w + 3.0, lit_h);
            let glow = if i == 2 {
                t.gr_stroke
            } else if proportion > 0.88 {
                t.lamp_hot
            } else if proportion > 0.70 {
                t.lamp_warn
            } else {
                t.lamp_safe
            };
            g.set_colour(c(glow).with_alpha(0.18));
            g.fill_rounded_rectangle(glow_box, 2.5);
        }

        for s in 0..SEGMENTS {
            let f = (s + 1) as f32 / SEGMENTS as f32;
            let is_lit = s < lit_count;
            let is_tip = s == lit_count - 1 && lit_count > 0;
            let sy = if i == 2 { top + s as f32 * pitch } else { bottom - (s + 1) as f32 * pitch + (pitch - seg_h) };
            let cell = if i == 2 {
                let base = c(t.gr_stroke).interpolated_with(c(t.lamp_hot), f * 0.55);
                if is_lit {
                    if is_tip {
                        if t.light_surface { base.darker(0.22) } else { base.interpolated_with(WHITE, 0.42) }
                    } else {
                        base
                    }
                } else {
                    base.with_alpha(if t.light_surface { 0.16 } else { 0.13 })
                }
            } else {
                let mut cell = c(if f < 0.70 {
                    if is_lit { t.lamp_safe } else { t.lamp_ghost_safe }
                } else if f < 0.90 {
                    if is_lit { t.lamp_warn } else { t.lamp_ghost_warn }
                } else if is_lit {
                    t.lamp_hot
                } else {
                    t.lamp_ghost_hot
                });
                if is_tip {
                    cell = if t.light_surface { cell.darker(0.22) } else { cell.interpolated_with(WHITE, 0.42) };
                }
                cell
            };
            g.set_colour(cell);
            g.fill_rounded_rectangle(Rect::new(x, sy, lane_w, seg_h), 1.0);
        }

        g.set_colour(c(t.text_primary));
        g.set_font(Font::new(8.3, true));
        g.draw_text_i(["L", "R", "GR"][i], x as i32 - 3, bottom as i32 + 3, lane_w as i32 + 6, 11, Just::Centred);
    }

    for db in [0, -12, -18, -24, -36, -48, -60] {
        let y = top + height * (-db) as f32 / 60.0;
        let is_ref = db == -18;
        g.set_colour(if is_ref { c(t.zero_dbu_ref) } else { c(t.text_muted).with_alpha(0.65) });
        g.draw_horizontal_line(y as i32, left + lane_w * 2.0 + 4.5, left + lane_w * 2.0 + 8.5);
        let dbu = db + 18;
        let label = if is_ref { "0/-18".to_string() } else { format!("{}{dbu}/{db}", if dbu > 0 { "+" } else { "" }) };
        g.set_font(Font::new(8.0, is_ref));
        g.set_colour(c(if is_ref { t.zero_dbu_ref } else { t.text_muted }));
        g.draw_text_i(&label, (left + lane_w * 2.0 + 9.5) as i32, y as i32 - 5, 41, 10, Just::CentredLeft);
    }

    // the louder channel, in whole numbers (a readout that changes every frame)
    let level = values[0].max(values[1]).clamp(-60.0, 12.0);
    let reading = if vu { format!("{} VU", signed(level - VU_REFERENCE_DBFS)) } else { format!("{} dBFS", signed(level)) };
    let oled = Rect::new(5.0, (h - 34) as f32, (w - 10) as f32, 29.0);
    g.set_colour(c(t.oled_bg));
    g.fill_rounded_rectangle(oled, 3.2);
    g.set_colour(c(t.oled_border));
    g.draw_rounded_rectangle(oled, 3.2, 0.9);
    g.set_colour(c(t.oled_text));
    g.set_font(Font::new(8.7, true));
    g.draw_text_i(&reading, 6, h - 33, w - 12, 14, Just::Centred);
    g.set_colour(c(t.text_primary).with_alpha(0.85));
    g.set_font(Font::new(8.0, true));
    g.draw_text_i(["PPM", "RMS", "VU"][scale.min(2)], 6, h - 20, w - 12, 13, Just::Centred);
}

// ---- CompressionScope -----------------------------------------------------------------------------

const CAPACITY: usize = 640;

/// The history the scope shows: frames of at least 10 ms, newest last, and the auto-scaled range.
pub struct History {
    frames: Vec<HistoryFrame>,
    write: usize,
    count: usize,
    pending: HistoryFrame,
    pub min_db: f32,
    pub max_db: f32,
}

impl Default for History {
    fn default() -> Self {
        History { frames: vec![HistoryFrame::default(); CAPACITY], write: 0, count: 0, pending: HistoryFrame::default(), min_db: -60.0, max_db: 0.0 }
    }
}

impl History {
    /// Changes whenever what the field shows changes.
    pub fn digest(&self) -> (usize, usize, u32, u32) {
        (self.write, self.count, self.min_db.to_bits(), self.max_db.to_bits())
    }

    pub fn clear(&mut self) {
        self.count = 0;
        self.write = 0;
        self.pending = HistoryFrame::default();
    }

    /// `drainFifo`: block frames joined into frames of 10 ms or more.
    pub fn push(&mut self, f: HistoryFrame) {
        let p = &mut self.pending;
        p.input = p.input.max(f.input);
        p.output = p.output.max(f.output);
        p.reduction = p.reduction.max(f.reduction);
        p.seconds += f.seconds;
        if p.seconds >= 0.01 {
            self.frames[self.write] = *p;
            self.write = (self.write + 1) % CAPACITY;
            self.count = CAPACITY.min(self.count + 1);
            self.pending = HistoryFrame::default();
        }
    }

    fn newest(&self, i: usize) -> &HistoryFrame {
        &self.frames[(self.write + CAPACITY - 1 - i) % CAPACITY]
    }

    /// `updateAutoScale`.
    pub fn update_auto_scale(&mut self, snap: bool) {
        let mut peak = 0.0f32;
        let mut elapsed = 0.0f32;
        for i in 0..self.count {
            let f = self.newest(i);
            peak = peak.max(f.input.max(f.output));
            elapsed += f.seconds;
            if elapsed >= 6.0 {
                break;
            }
        }
        let (mut target_max, mut target_min) = (0.0f32, -60.0f32);
        if self.count > 0 && peak > 1.0e-4 {
            let peak_db = gain_to_db(peak, -72.0);
            target_max = (peak_db + 4.5).clamp(-21.0, 6.0);
            target_min = (target_max - 42.0).clamp(-72.0, target_max - 36.0);
        }
        if snap {
            self.max_db = target_max;
            self.min_db = target_min;
        } else {
            let max_rate = if target_max > self.max_db { 0.40 } else { 0.10 };
            let min_rate = if target_min < self.min_db { 0.30 } else { 0.10 };
            self.max_db += (target_max - self.max_db) * max_rate;
            self.min_db += (target_min - self.min_db) * min_rate;
            if (target_max - self.max_db).abs() < 0.03 {
                self.max_db = target_max;
            }
            if (target_min - self.min_db).abs() < 0.03 {
                self.min_db = target_min;
            }
        }
    }
}

/// `CompressionScope::getPlotRect` for a scope of `w` x `h`.
pub fn plot_rect(w: i32, h: i32) -> Rect {
    Rect::new(62.0, 14.0, 80.0f32.max(w as f32 - 102.0), 80.0f32.max(h as f32 - 42.0))
}

/// `CompressionScope::paint` (the modules and the transfer curve are its children, painted after),
/// in two parts: what only the look and the scale range change, and what moves with the audio.
pub fn scope(g: &mut Graphics, t: &Theme, w: i32, h: i32, hist: &History, threshold_db: f32, curve_visible: bool) {
    scope_under(g, t, w, h, hist, curve_visible);
    scope_over(g, t, w, h, hist, threshold_db);
}

/// The housing, the glass, the grid and its labels.
pub fn scope_under(g: &mut Graphics, t: &Theme, w: i32, h: i32, hist: &History, curve_visible: bool) {
    let outer = Rect::new(0.0, 0.0, w as f32, h as f32).reduced(0.8);
    let (min_db, max_db) = (hist.min_db, hist.max_db);

    g.set_colour(c(t.panel_border_lo));
    g.fill_rounded_rectangle(outer.expanded(0.4), 5.0);
    g.set_gradient_fill(gradient(c(t.panel_top), outer.x, outer.y, c(t.panel_bottom), outer.x, outer.bottom()));
    g.fill_rounded_rectangle(outer, 4.5);
    if t.synth_grid {
        g.set_colour(c(t.panel_border_hi).with_alpha(0.24));
        g.draw_rounded_rectangle(outer.expanded(0.4), 4.8, 2.2);
    }
    g.set_colour(c(t.panel_border_hi).with_alpha(if t.synth_grid { 0.80 } else { 0.68 }));
    g.draw_rounded_rectangle(outer, 4.5, 1.0);

    let r = plot_rect(w, h);
    g.set_gradient_fill(gradient(c(t.well_bg_top), r.x, r.y, c(t.well_bg_bottom), r.x, r.bottom()));
    g.fill_rounded_rectangle(r, 3.5);

    if t.synth_grid {
        g.save_state();
        g.reduce_clip_region(r.to_nearest_int());
        let horizon = r.y + r.h * 0.44;
        g.set_gradient_fill(gradient(c(0x00ff2bd6), r.centre_x(), r.y, c(0x28ff2bd6), r.centre_x(), horizon));
        g.fill_rect(Rect::new(r.x, r.y, r.w, horizon - r.y));
        g.set_gradient_fill(gradient(c(0x2cff2bd6), r.centre_x(), horizon, c(0x168c2eff), r.centre_x(), r.bottom()));
        g.fill_rect(Rect::new(r.x, horizon, r.w, r.bottom() - horizon));
        let vx = r.centre_x();
        g.set_colour(c(0x2cff2bd6));
        for ray in -9..=9 {
            let bx = vx + ray as f32 * (r.w * 0.135);
            g.draw_line(vx + ray as f32 * 18.0, horizon, bx, r.bottom(), 0.85);
        }
        for row in 1..=8 {
            let p = (row as f32 / 8.0).powf(1.85);
            let py = horizon + p * (r.bottom() - horizon);
            g.set_colour(c(0xff00f6ff).interpolated_with(c(0xffff2bd6), 1.0 - p).with_alpha(0.14 + 0.14 * p));
            g.draw_horizontal_line(py as i32, r.x, r.right());
        }
        g.restore_state();
    }

    g.set_font(Font::new(8.0, true));
    g.set_colour(c(t.text_muted));
    g.draw_text_i("dBu / dBFS", 3, 2, 56, 11, Just::CentredRight);
    g.set_colour(c(t.gr_stroke).with_alpha(0.92));
    g.draw_text_i("GR dB", r.right() as i32 + 4, 2, 34, 11, Just::CentredLeft);

    g.set_font(Font::new(8.3, true));
    for sec in 0..=6 {
        let x = r.x + r.w * sec as f32 / 6.0;
        g.set_colour(c(t.grid_minor));
        g.draw_vertical_line(x as i32, r.y, r.bottom());
        g.set_colour(c(t.text_muted));
        let label = if sec == 6 { "NOW".to_string() } else { format!("-{} s", 6 - sec) };
        let label_y = r.bottom() as i32 + if curve_visible { 16 } else { 6 };
        g.draw_text_i(&label, x as i32 - 18, label_y, 36, 11, Just::Centred);
    }

    let span = 12.0f32.max(max_db - min_db);
    let step = if span > 48.0 { 12 } else { 6 };
    let mut db = -72;
    while db <= 12 {
        let is_zero = db == -18;
        if (is_zero || db % step == 0) && !((db as f32) < min_db - 0.2 || (db as f32) > max_db + 0.2) {
            let y = jmap(db as f32, min_db, max_db, r.bottom(), r.y);
            if is_zero {
                let dashed = Path::dashed_stroke_of_line(r.x, y, r.right(), y, &[6.0, 3.5], 1.3);
                g.set_colour(c(t.zero_dbu_ref).with_alpha(0.22));
                g.draw_line(r.x, y, r.right(), y, 3.5);
                g.set_colour(c(t.zero_dbu_ref).with_alpha(0.82));
                g.fill_path(&dashed);
            } else {
                g.set_colour(c(t.grid_major));
                g.draw_horizontal_line(y as i32, r.x, r.right());
            }
            let dbu = db + 18;
            let label = if is_zero { "0dBu/-18".to_string() } else { format!("{}{dbu} / {db}", if dbu > 0 { "+" } else { "" }) };
            g.set_font(Font::new(if is_zero { 8.3 } else { 8.0 }, is_zero));
            g.set_colour(if is_zero { c(t.zero_dbu_ref) } else { c(t.text_primary).with_alpha(0.82) });
            g.draw_text_i(&label, 2, y as i32 - 6, 57, 12, Just::CentredRight);
        }
        db += 6;
    }

    g.set_font(Font::new(8.0, true));
    for step in 0..=5 {
        let y = r.y + r.h * step as f32 / 5.0;
        let tick = 36.0 * step as f32 / 5.0;
        g.set_colour(c(t.gr_stroke).with_alpha(0.88));
        let label = if step == 0 { "0".to_string() } else { format!("-{}", super::general(f64::from(tick))) };
        g.draw_text_i(&label, r.right() as i32 + 5, y as i32 - 6, 33, 12, Just::CentredLeft);
    }

}

/// The traces, the threshold, the scanlines and reflection, and the glass's frame.
pub fn scope_over(g: &mut Graphics, t: &Theme, w: i32, h: i32, hist: &History, threshold_db: f32) {
    let r = plot_rect(w, h);
    let (min_db, max_db) = (hist.min_db, hist.max_db);
    let span = 12.0f32.max(max_db - min_db);
    g.save_state();
    g.reduce_clip_region(r.to_nearest_int());
    if hist.count > 0 {
        paint_traces(g, t, r, hist, span);
    }

    let thresh = threshold_db.clamp(-60.0, 0.0);
    if thresh >= min_db - 1.0 && thresh <= max_db + 1.0 {
        let y = jmap(thresh, min_db, max_db, r.bottom(), r.y);
        let dashed = Path::dashed_stroke_of_line(r.x, y, r.right(), y, &[5.0, 4.0], 1.15);
        g.set_colour(c(t.threshold_ref).with_alpha(0.72));
        g.fill_path(&dashed);
        let thresh_dbu = thresh + DBU_OFFSET;
        g.set_font(Font::new(8.0, true));
        let tag_y = if y < r.y + 18.0 { (y + 3.0) as i32 } else { (y - 15.0) as i32 };
        let pill = Rect::new(r.right() - 138.0, tag_y as f32, 133.0, 13.0);
        g.set_colour(c(t.oled_bg).with_alpha(0.90));
        g.fill_rounded_rectangle(pill, 3.0);
        g.set_colour(c(t.threshold_ref).with_alpha(0.55));
        g.draw_rounded_rectangle(pill, 3.0, 0.8);
        g.set_colour(c(t.threshold_ref));
        let text = format!("THR {}{} dBu ({})", if thresh_dbu > 0.0 { "+" } else { "" }, whole(thresh_dbu), whole(thresh));
        g.draw_text(&text, pill.to_nearest_int().reduced(4, 0).to_float(), Just::CentredRight);
    }

    if t.crt_scanlines {
        g.set_colour(c(0x0b000000));
        let mut sy = r.y as i32 + 2;
        while sy < r.bottom() as i32 {
            g.draw_horizontal_line(sy, r.x, r.right());
            sy += 3;
        }
    }
    g.set_gradient_fill(gradient(c(0x10ffffff), r.x, r.y, c(0x00ffffff), r.x, r.y + 52.0));
    g.fill_rect(Rect::new(r.x, r.y, r.w, 52.0));
    g.restore_state();

    g.set_colour(c(t.well_bezel_inner));
    g.draw_rounded_rectangle(r.reduced(0.8), 3.2, 1.1);
    g.set_colour(c(t.well_bezel_outer));
    g.draw_rounded_rectangle(r, 3.5, 1.1);
}

fn paint_traces(g: &mut Graphics, t: &Theme, r: Rect, hist: &History, span: f32) {
    let norm = |peak: f32| ((gain_to_db(peak, -80.0) - hist.min_db) / span).clamp(0.0, 1.0);
    let (mut pre_fill, mut pre_top, mut post_fill, mut post_top, mut gr_fill, mut gr_line) =
        (Path::new(), Path::new(), Path::new(), Path::new(), Path::new(), Path::new());
    let mut first = true;
    let mut elapsed = 0.0f32;
    let (mut last_x, mut last_in, mut last_out, mut last_gr) = (r.right(), r.bottom(), r.bottom(), r.y);
    for i in 0..hist.count {
        let f = hist.newest(i);
        let x_right = r.right() - (elapsed / 6.0) * r.w;
        if x_right < r.x {
            break;
        }
        let w = 1.0f32.max((f.seconds / 6.0) * r.w);
        let x_left = r.x.max(x_right - w);
        let x_mid = 0.5 * (x_right + x_left);
        let y_in = r.bottom() - norm(f.input) * r.h;
        let y_out = r.bottom() - norm(f.output) * r.h;
        let y_gr = r.y + (f.reduction.clamp(0.0, 36.0) / 36.0) * r.h;
        if first {
            pre_fill.start_new_sub_path(x_right, r.bottom());
            pre_fill.line_to(x_right, y_in);
            pre_top.start_new_sub_path(x_right, y_in);
            post_fill.start_new_sub_path(x_right, r.bottom());
            post_fill.line_to(x_right, y_out);
            post_top.start_new_sub_path(x_right, y_out);
            gr_fill.start_new_sub_path(x_right, r.y);
            gr_fill.line_to(x_right, y_gr);
            gr_line.start_new_sub_path(x_right, y_gr);
            first = false;
        }
        pre_fill.line_to(x_mid, y_in);
        pre_top.line_to(x_mid, y_in);
        post_fill.line_to(x_mid, y_out);
        post_top.line_to(x_mid, y_out);
        gr_fill.line_to(x_mid, y_gr);
        gr_line.line_to(x_mid, y_gr);
        last_x = x_left;
        last_in = y_in;
        last_out = y_out;
        last_gr = y_gr;
        elapsed += f.seconds;
    }
    if first {
        return;
    }
    pre_fill.line_to(last_x, last_in);
    pre_fill.line_to(last_x, r.bottom());
    pre_fill.close_sub_path();
    pre_top.line_to(last_x, last_in);
    post_fill.line_to(last_x, last_out);
    post_fill.line_to(last_x, r.bottom());
    post_fill.close_sub_path();
    post_top.line_to(last_x, last_out);
    gr_fill.line_to(last_x, last_gr);
    gr_fill.line_to(last_x, r.y);
    gr_fill.close_sub_path();
    gr_line.line_to(last_x, last_gr);

    let light = t.light_surface;
    g.set_gradient_fill(gradient(c(t.pre_fill_top), 0.0, r.y, c(t.pre_fill_bottom), 0.0, r.bottom()));
    g.fill_path(&pre_fill);
    g.set_colour(c(t.pre_stroke).with_alpha(0.28));
    g.stroke_path(&pre_top, Stroke::curved_rounded(3.6));
    g.set_colour(c(t.pre_stroke));
    g.stroke_path(&pre_top, Stroke::curved_rounded(1.45));

    g.set_gradient_fill(gradient(c(t.post_fill_top), 0.0, r.y, c(t.post_fill_bottom), 0.0, r.bottom()));
    g.fill_path(&post_fill);
    g.set_colour(c(t.post_glow).with_alpha(if light { 0.12 } else { 0.22 }));
    g.stroke_path(&post_top, Stroke::curved_rounded(if light { 5.2 } else { 6.4 }));
    g.set_colour(c(t.post_glow).with_alpha(if light { 0.24 } else { 0.42 }));
    g.stroke_path(&post_top, Stroke::curved_rounded(3.3));
    g.set_colour(c(t.post_stroke));
    g.stroke_path(&post_top, Stroke::curved_rounded(2.15));
    g.set_colour(if light { c(0xff07090d) } else { c(t.post_stroke).interpolated_with(WHITE, 0.52) });
    g.stroke_path(&post_top, Stroke::curved_rounded(0.90));

    g.set_colour(c(t.gr_fill));
    g.fill_path(&gr_fill);
    g.set_colour(c(t.gr_stroke).with_alpha(if light { 0.18 } else { 0.25 }));
    g.stroke_path(&gr_line, Stroke::curved_rounded(if light { 4.4 } else { 5.4 }));
    g.set_colour(c(t.gr_stroke));
    g.stroke_path(&gr_line, Stroke::curved_rounded(2.15));
    g.set_colour(if light { c(t.gr_stroke).darker(0.30) } else { c(t.gr_stroke).interpolated_with(WHITE, 0.45) });
    g.stroke_path(&gr_line, Stroke::curved_rounded(0.85));
}

// ---- TransferCurveDisplay -------------------------------------------------------------------------

pub const PAD_LEFT: f32 = 12.0;
pub const PAD_TOP: f32 = 12.0;
pub const PAD_RIGHT: f32 = 40.0;
pub const PAD_BOTTOM: f32 = 24.0;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Handle {
    Combined,
    Makeup,
    Threshold,
}

/// What the curve reads: the parameters and the display range it shares with the history.
pub struct Curve {
    pub area: Rect, // curveArea, in the curve component's coordinates
    pub min_db: f32,
    pub max_db: f32,
    pub threshold: f32,
    pub ratio: f32,
    pub knee: f32, // already 0 outside Red
    pub makeup: f32,
    pub makeup_norm: f32,
}

impl Curve {
    /// `computeOutputDb`.
    pub fn output_db(&self, input_db: f32) -> f32 {
        let slope = crate::dsp::slope_of(self.ratio);
        let over = input_db - self.threshold;
        if self.knee > 0.01 && over.abs() <= self.knee * 0.5 {
            let x = over + self.knee * 0.5;
            return input_db - slope * (x * x) / (2.0 * self.knee) + self.makeup;
        }
        if over <= 0.0 {
            return input_db + self.makeup;
        }
        input_db - slope * over + self.makeup
    }

    pub fn point(&self, input_db: f32, output_db: f32) -> (f32, f32) {
        let a = self.area;
        (jmap(input_db, self.min_db, self.max_db, a.x, a.right()), jmap(output_db, self.min_db, self.max_db, a.bottom(), a.y))
    }

    /// `positionForHandle`.
    pub fn handle(&self, h: Handle) -> (f32, f32) {
        let a = self.area;
        match h {
            Handle::Threshold => (self.point(self.threshold, self.min_db).0.clamp(a.x, a.right()), a.bottom() + 10.0),
            Handle::Makeup => (a.right() + 13.0, jmap(self.makeup_norm, 0.0, 1.0, a.bottom(), a.y)),
            Handle::Combined => {
                let p = self.point(self.threshold, self.output_db(self.threshold));
                (p.0.clamp(a.x + 6.0, a.right() - 6.0), p.1.clamp(a.y + 6.0, a.bottom() - 6.0))
            }
        }
    }

    /// `hitTestHandle`.
    pub fn hit(&self, x: f32, y: f32) -> Option<Handle> {
        [Handle::Threshold, Handle::Makeup, Handle::Combined].into_iter().find(|&h| {
            let (hx, hy) = self.handle(h);
            (x - hx).hypot(y - hy) <= 12.0
        })
    }
}

/// `juce::Rectangle<float> (size, size).withCentre (p)`.
fn around(p: (f32, f32), size: f32) -> Rect {
    Rect::new(0.0, 0.0, size, size).with_centre(p.0, p.1)
}

/// `TransferCurveDisplay::paint`.
pub fn curve(g: &mut Graphics, t: &Theme, cv: &Curve, active: Option<Handle>, live_input_db: f32) {
    let a = cv.area;
    if a.w <= 0.0 || a.h <= 0.0 {
        return;
    }
    let light = t.light_surface;
    g.set_colour(c(t.well_bg_top).with_alpha(if light { 0.62 } else { 0.48 }));
    g.fill_rounded_rectangle(a, 3.5);
    g.set_colour(c(t.accent_primary).with_alpha(if light { 0.30 } else { 0.42 }));
    g.draw_rounded_rectangle(a, 3.5, 1.0);

    g.set_font(Font::new(8.0, true));
    let mut db = -66;
    while db <= 18 {
        let dbf = db as f32;
        if !(dbf < cv.min_db + 2.0 || dbf > cv.max_db - 1.0) {
            let x = jmap(dbf, cv.min_db, cv.max_db, a.x, a.right());
            let is_zero = db == -18;
            g.set_colour(if is_zero { c(t.zero_dbu_ref).with_alpha(0.50) } else { c(t.text_muted).with_alpha(0.28) });
            g.draw_vertical_line(x as i32, a.y, a.bottom());
            let dbu = db + 18;
            let label = if dbu == 0 { "0dBu".to_string() } else { format!("{}{dbu}", if dbu > 0 { "+" } else { "" }) };
            g.set_colour(if is_zero { c(t.zero_dbu_ref) } else { c(t.text_primary).with_alpha(0.80) });
            g.draw_text_i(&label, x as i32 - 18, a.bottom() as i32 - 13, 36, 11, Just::Centred);
        }
        db += 12;
    }

    g.save_state();
    g.reduce_clip_region(a.to_nearest_int());
    let (u0, u1) = (cv.point(cv.min_db, cv.min_db), cv.point(cv.max_db, cv.max_db));
    let unity = Path::dashed_stroke_of_line(u0.0, u0.1, u1.0, u1.1, &[4.0, 3.0], 1.1);
    g.set_colour(c(t.text_muted).with_alpha(0.50));
    g.fill_path(&unity);

    let mut path = Path::new();
    let step = 0.25f32.max((cv.max_db - cv.min_db) / 140.0);
    let mut input = cv.min_db;
    let mut first = true;
    while input <= cv.max_db + 0.01 {
        let clamped = input.min(cv.max_db);
        let p = cv.point(clamped, cv.output_db(clamped));
        if first {
            path.start_new_sub_path(p.0, p.1);
            first = false;
        } else {
            path.line_to(p.0, p.1);
        }
        input += step;
    }
    g.set_colour(c(t.post_glow).with_alpha(if light { 0.14 } else { 0.24 }));
    g.stroke_path(&path, Stroke::curved_rounded(if light { 5.4 } else { 6.8 }));
    g.set_colour(c(t.accent_primary).with_alpha(if light { 0.32 } else { 0.50 }));
    g.stroke_path(&path, Stroke::curved_rounded(3.6));
    g.set_colour(c(t.accent_primary));
    g.stroke_path(&path, Stroke::curved_rounded(2.2));
    g.set_colour(if light { c(0xff07090d) } else { c(t.accent_primary).interpolated_with(WHITE, 0.58) });
    g.stroke_path(&path, Stroke::curved_rounded(0.9));
    g.restore_state();

    let bottom_rail = a.bottom() + 10.0;
    let right_rail = a.right() + 13.0;
    g.set_colour(c(t.accent_primary).with_alpha(0.45));
    g.draw_horizontal_line(bottom_rail as i32, a.x, a.right());
    g.set_colour(c(t.accent_secondary).with_alpha(0.45));
    g.draw_vertical_line(right_rail as i32, a.y, a.bottom());

    for h in [Handle::Combined, Handle::Makeup, Handle::Threshold] {
        let pos = cv.handle(h);
        let colour = c(match h {
            Handle::Combined => t.accent_primary,
            Handle::Makeup => t.accent_secondary,
            Handle::Threshold => t.threshold_ref,
        });
        let on = active == Some(h);
        g.set_colour(colour.with_alpha(if on { 0.34 } else { 0.20 }));
        g.fill_ellipse(around(pos, 16.5));
        g.set_colour(c(if light { 0xfffaf9f6 } else { t.oled_bg }));
        g.fill_ellipse(around(pos, 11.5));
        g.set_colour(if on { if light { colour.darker(0.25) } else { colour.brighter(0.40) } } else { colour });
        g.draw_ellipse(around(pos, 11.5), if on { 2.1 } else { 1.6 });
        g.fill_ellipse(around(pos, 4.6));
    }

    g.set_colour(c(t.text_primary).with_alpha(0.85));
    g.set_font(Font::new(8.0, true));
    g.draw_text_i("MAKEUP", right_rail as i32 - 21, bottom_rail as i32 - 4, 44, 10, Just::Centred);

    let live_out = cv.output_db(live_input_db);
    let live = cv.point(live_input_db.clamp(cv.min_db, cv.max_db), live_out.clamp(cv.min_db, cv.max_db));
    let dot = c(t.lamp_warn);
    g.set_colour(dot.with_alpha(0.35));
    g.fill_ellipse(around(live, 13.0));
    g.set_colour(dot);
    g.fill_ellipse(around(live, 7.5));
    g.set_colour(WHITE);
    g.fill_ellipse(around(live, 3.0));
}

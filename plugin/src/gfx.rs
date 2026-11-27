//! JUCE 8's `Graphics` as 1.5.3's editor used it on macOS, call for call on CoreGraphics: JUCE's own
//! CoreGraphics context (`juce_CoreGraphicsContext_mac.mm`), colour arithmetic (`juce_Colour.cpp`,
//! `juce_PixelFormats.h`), arcs and dashes (`juce_Path.cpp`, `juce_PathStrokeType.cpp`) and single-line
//! text layout (`juce_GlyphArrangement.cpp`, `juce_JustifiedText.cpp`) in Lucida Grande, JUCE's default
//! sans-serif face on macOS. The same calls reach the same CoreGraphics functions, so the pixels are
//! those of 1.5.3.
//!
//! macOS only: the editor exists only there (SDK `docs/EDITOR.md`).

#![allow(non_upper_case_globals, clippy::too_many_arguments)]

use std::ffi::c_void;
use std::ptr::null;

// ---- CoreGraphics, CoreText and CoreFoundation ---------------------------------------------------

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CGPoint {
    pub x: f64,
    pub y: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct CGSize {
    w: f64,
    h: f64,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct CGRect {
    origin: CGPoint,
    size: CGSize,
}
#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct CGAffineTransform {
    a: f64,
    b: f64,
    c: f64,
    d: f64,
    tx: f64,
    ty: f64,
}
#[repr(C)]
#[derive(Clone, Copy)]
struct CFRange {
    location: isize,
    length: isize,
}

type Ref = *const c_void;

fn cg_rect(x: f64, y: f64, w: f64, h: f64) -> CGRect {
    CGRect { origin: CGPoint { x, y }, size: CGSize { w, h } }
}

#[link(name = "CoreFoundation", kind = "framework")]
unsafe extern "C" {
    fn CFRelease(cf: Ref);
    fn CFRetain(cf: Ref) -> Ref;
    fn CFStringCreateWithBytes(alloc: Ref, bytes: *const u8, len: isize, encoding: u32, external: bool) -> Ref;
    fn CFDictionaryCreate(alloc: Ref, keys: *const Ref, values: *const Ref, n: isize, kcb: Ref, vcb: Ref) -> Ref;
    fn CFDictionaryGetValue(dict: Ref, key: Ref) -> Ref;
    fn CFAttributedStringCreate(alloc: Ref, s: Ref, attrs: Ref) -> Ref;
    fn CFArrayGetCount(a: Ref) -> isize;
    fn CFArrayGetValueAtIndex(a: Ref, i: isize) -> Ref;
    fn CFDataGetBytePtr(d: Ref) -> *const u8;
    fn CFDataGetLength(d: Ref) -> isize;
    static kCFTypeDictionaryKeyCallBacks: u8;
    static kCFTypeDictionaryValueCallBacks: u8;
}

#[link(name = "CoreGraphics", kind = "framework")]
unsafe extern "C" {
    static kCGColorSpaceSRGB: Ref;
    fn CGColorSpaceCreateWithName(name: Ref) -> Ref;
    fn CGColorSpaceRelease(cs: Ref);
    fn CGBitmapContextCreate(data: *mut c_void, w: usize, h: usize, bpc: usize, bpr: usize, cs: Ref, info: u32) -> Ref;
    fn CGContextRelease(c: Ref);
    fn CGContextSaveGState(c: Ref);
    fn CGContextRestoreGState(c: Ref);
    fn CGContextTranslateCTM(c: Ref, tx: f64, ty: f64);
    fn CGContextConcatCTM(c: Ref, t: CGAffineTransform);
    fn CGContextSetShouldAntialias(c: Ref, on: bool);
    fn CGContextSetShouldSmoothFonts(c: Ref, on: bool);
    fn CGContextSetAllowsFontSmoothing(c: Ref, on: bool);
    fn CGContextSetBlendMode(c: Ref, mode: i32);
    fn CGColorCreate(cs: Ref, comps: *const f64) -> Ref;
    fn CGColorRelease(col: Ref);
    fn CGContextSetFillColorWithColor(c: Ref, col: Ref);
    fn CGContextSetStrokeColorWithColor(c: Ref, col: Ref);
    fn CGContextSetAlpha(c: Ref, a: f64);
    fn CGContextFillRect(c: Ref, r: CGRect);
    fn CGContextClipToRect(c: Ref, r: CGRect);
    fn CGContextClipToRects(c: Ref, r: *const CGRect, n: usize);
    fn CGContextGetClipBoundingBox(c: Ref) -> CGRect;
    fn CGContextBeginPath(c: Ref);
    fn CGContextMoveToPoint(c: Ref, x: f64, y: f64);
    fn CGContextAddLineToPoint(c: Ref, x: f64, y: f64);
    fn CGContextClosePath(c: Ref);
    fn CGContextDrawPath(c: Ref, mode: i32);
    fn CGContextReplacePathWithStrokedPath(c: Ref);
    fn CGContextClip(c: Ref);
    fn CGContextEOClip(c: Ref);
    fn CGContextSetLineWidth(c: Ref, w: f64);
    fn CGContextSetLineCap(c: Ref, cap: i32);
    fn CGContextSetLineJoin(c: Ref, join: i32);
    fn CGContextAddEllipseInRect(c: Ref, r: CGRect);
    fn CGContextAddPath(c: Ref, p: Ref);
    fn CGPathCreateWithRoundedRect(r: CGRect, cw: f64, ch: f64, t: *const CGAffineTransform) -> Ref;
    fn CGPathCreateMutable() -> Ref;
    fn CGPathAddRoundedRect(p: Ref, t: *const CGAffineTransform, r: CGRect, cw: f64, ch: f64);
    fn CGPathRelease(p: Ref);
    fn CGGradientCreateWithColorComponents(cs: Ref, comps: *const f64, locs: *const f64, n: usize) -> Ref;
    fn CGGradientRelease(g: Ref);
    fn CGContextDrawLinearGradient(c: Ref, g: Ref, a: CGPoint, b: CGPoint, options: u32);
    fn CGContextBeginTransparencyLayer(c: Ref, aux: Ref);
    fn CGContextEndTransparencyLayer(c: Ref);
    fn CGContextSetTextMatrix(c: Ref, t: CGAffineTransform);
}

#[link(name = "CoreText", kind = "framework")]
unsafe extern "C" {
    static kCTFontAttributeName: Ref;
    fn CTFontCreateWithName(name: Ref, size: f64, matrix: *const CGAffineTransform) -> Ref;
    fn CTFontCreateCopyWithAttributes(font: Ref, size: f64, matrix: *const CGAffineTransform, desc: Ref) -> Ref;
    fn CTFontCopyTable(font: Ref, tag: u32, options: u32) -> Ref;
    fn CTFontGetAdvancesForGlyphs(font: Ref, orientation: u32, glyphs: *const u16, adv: *mut CGSize, n: isize) -> f64;
    fn CTFontDrawGlyphs(font: Ref, glyphs: *const u16, pos: *const CGPoint, n: usize, c: Ref);
    fn CTLineCreateWithAttributedString(s: Ref) -> Ref;
    fn CTLineGetGlyphRuns(line: Ref) -> Ref;
    fn CTRunGetGlyphCount(run: Ref) -> isize;
    fn CTRunGetGlyphs(run: Ref, r: CFRange, out: *mut u16);
    fn CTRunGetStringIndices(run: Ref, r: CFRange, out: *mut isize);
    fn CTRunGetAdvances(run: Ref, r: CFRange, out: *mut CGSize);
    fn CTRunGetAttributes(run: Ref) -> Ref;
}

const PATH_FILL: i32 = 0;
const PATH_EO_FILL: i32 = 1;
const PATH_STROKE: i32 = 2;
const GRADIENT_EXTEND: u32 = 3; // before start and after end

fn cf_string(s: &str) -> Ref {
    unsafe { CFStringCreateWithBytes(null(), s.as_ptr(), s.len() as isize, 0x0800_0100, false) }
}

// ---- JUCE arithmetic -------------------------------------------------------------------------------

/// `juce::roundToInt`: the magic-number trick, which rounds half to even.
pub fn round_to_int(v: f32) -> i32 {
    f64::from(v).round_ties_even() as i32
}

/// `juce::jmap`.
pub fn jmap(v: f32, s0: f32, s1: f32, t0: f32, t1: f32) -> f32 {
    t0 + (t1 - t0) * (v - s0) / (s1 - s0)
}

fn float_to_u8(n: f32) -> u32 {
    if n <= 0.0 {
        0
    } else if n >= 1.0 {
        255
    } else {
        round_to_int(n * 255.0) as u32
    }
}

/// `juce::Colour`: `0xAARRGGBB`, not premultiplied.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Colour(pub u32);

pub const WHITE: Colour = Colour(0xffff_ffff);
pub const LIGHT_GREY: Colour = Colour(0xffd3_d3d3);
pub const DARK_GREY: Colour = Colour(0xff55_5555);

impl Colour {
    pub fn alpha(self) -> u32 {
        self.0 >> 24
    }
    fn red(self) -> u32 {
        (self.0 >> 16) & 255
    }
    fn green(self) -> u32 {
        (self.0 >> 8) & 255
    }
    fn blue(self) -> u32 {
        self.0 & 255
    }
    fn set_alpha(self, a: u32) -> Self {
        Colour((self.0 & 0x00ff_ffff) | (a << 24))
    }
    pub fn with_alpha(self, a: f32) -> Self {
        self.set_alpha(float_to_u8(a))
    }
    pub fn with_multiplied_alpha(self, m: f32) -> Self {
        self.set_alpha(round_to_int(m * self.alpha() as f32).min(255) as u32)
    }
    fn premultiplied(self) -> u32 {
        let a = self.alpha();
        if a >= 255 {
            return self.0;
        }
        if a == 0 {
            return 0;
        }
        let ch = |c: u32| (c * a + 0x7f) >> 8;
        (a << 24) | (ch(self.red()) << 16) | (ch(self.green()) << 8) | ch(self.blue())
    }
    fn unpremultiplied(p: u32) -> Self {
        let a = p >> 24;
        if a >= 255 {
            return Colour(p);
        }
        if a == 0 {
            return Colour(0);
        }
        let ch = |c: u32| ((c * 255) / a).min(255);
        Colour((a << 24) | (ch((p >> 16) & 255) << 16) | (ch((p >> 8) & 255) << 8) | ch(p & 255))
    }
    pub fn interpolated_with(self, other: Colour, proportion: f32) -> Self {
        if proportion <= 0.0 {
            return self;
        }
        if proportion >= 1.0 {
            return other;
        }
        // PixelARGB::tween on the premultiplied pixels, in wrapping 32-bit arithmetic
        let amount = round_to_int(proportion * 255.0) as u32;
        let (d, s) = (self.premultiplied(), other.premultiplied());
        let tween = |d: u32, s: u32| (d.wrapping_add((s.wrapping_sub(d)).wrapping_mul(amount) >> 8)) & 0x00ff_00ff;
        let even = tween(d & 0x00ff_00ff, s & 0x00ff_00ff);
        let odd = tween((d >> 8) & 0x00ff_00ff, (s >> 8) & 0x00ff_00ff);
        Colour::unpremultiplied((odd << 8) | even)
    }
    pub fn brighter(self, amount: f32) -> Self {
        let k = 1.0 / (1.0 + amount);
        let ch = |c: u32| (255.0 - k * (255 - c) as f32) as u8 as u32;
        Colour((self.0 & 0xff00_0000) | (ch(self.red()) << 16) | (ch(self.green()) << 8) | ch(self.blue()))
    }
    pub fn darker(self, amount: f32) -> Self {
        let k = 1.0 / (1.0 + amount);
        let ch = |c: u32| (k * c as f32) as u8 as u32;
        Colour((self.0 & 0xff00_0000) | (ch(self.red()) << 16) | (ch(self.green()) << 8) | ch(self.blue()))
    }
    fn components(self) -> [f64; 4] {
        [self.red(), self.green(), self.blue(), self.alpha()].map(|c| f64::from(c as f32 / 255.0))
    }
}

// ---- geometry --------------------------------------------------------------------------------------

/// `juce::Rectangle<float>`.
#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}

impl Rect {
    pub const fn new(x: f32, y: f32, w: f32, h: f32) -> Self {
        Rect { x, y, w, h }
    }
    pub fn right(self) -> f32 {
        self.x + self.w
    }
    pub fn bottom(self) -> f32 {
        self.y + self.h
    }
    pub fn centre_x(self) -> f32 {
        self.x + self.w * 0.5
    }
    pub fn centre_y(self) -> f32 {
        self.y + self.h * 0.5
    }
    pub fn reduced(self, d: f32) -> Self {
        self.reduced_xy(d, d)
    }
    pub fn reduced_xy(self, dx: f32, dy: f32) -> Self {
        let w = (self.w - 2.0 * dx).max(0.0);
        let h = (self.h - 2.0 * dy).max(0.0);
        Rect::new(self.x + dx, self.y + dy, w, h)
    }
    pub fn expanded(self, d: f32) -> Self {
        self.expanded_xy(d, d)
    }
    pub fn expanded_xy(self, dx: f32, dy: f32) -> Self {
        let w = (self.w + 2.0 * dx).max(0.0);
        let h = (self.h + 2.0 * dy).max(0.0);
        Rect::new(self.x - dx, self.y - dy, w, h)
    }
    pub fn translated(self, dx: f32, dy: f32) -> Self {
        Rect::new(self.x + dx, self.y + dy, self.w, self.h)
    }
    pub fn with_height(self, h: f32) -> Self {
        Rect::new(self.x, self.y, self.w, h)
    }
    pub fn with_trimmed_bottom(self, d: f32) -> Self {
        Rect::new(self.x, self.y, self.w, (self.h - d).max(0.0))
    }
    /// A rectangle of this size centred on (`cx`, `cy`).
    pub fn with_centre(self, cx: f32, cy: f32) -> Self {
        Rect::new(cx - self.w / 2.0, cy - self.h / 2.0, self.w, self.h)
    }
    /// `toNearestInt`.
    pub fn to_nearest_int(self) -> IRect {
        IRect::new(round_to_int(self.x), round_to_int(self.y), round_to_int(self.w), round_to_int(self.h))
    }
    pub fn contains(self, x: f32, y: f32) -> bool {
        x >= self.x && y >= self.y && x < self.right() && y < self.bottom()
    }
}

/// `juce::Rectangle<int>`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default, Hash)]
pub struct IRect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl IRect {
    pub const fn new(x: i32, y: i32, w: i32, h: i32) -> Self {
        IRect { x, y, w, h }
    }
    pub fn to_float(self) -> Rect {
        Rect::new(self.x as f32, self.y as f32, self.w as f32, self.h as f32)
    }
    pub fn right(self) -> i32 {
        self.x + self.w
    }
    pub fn bottom(self) -> i32 {
        self.y + self.h
    }
    pub fn centre_y(self) -> i32 {
        self.y + self.h / 2
    }
    pub fn reduced(self, dx: i32, dy: i32) -> Self {
        IRect::new(self.x + dx, self.y + dy, (self.w - 2 * dx).max(0), (self.h - 2 * dy).max(0))
    }
    pub fn translated(self, dx: i32, dy: i32) -> Self {
        IRect::new(self.x + dx, self.y + dy, self.w, self.h)
    }
    pub fn expanded(self, d: i32) -> Self {
        IRect::new(self.x - d, self.y - d, self.w + 2 * d, self.h + 2 * d)
    }
    pub fn remove_from_top(&mut self, n: i32) -> IRect {
        let n = n.min(self.h);
        let top = IRect::new(self.x, self.y, self.w, n);
        self.y += n;
        self.h -= n;
        top
    }
    pub fn remove_from_right(&mut self, n: i32) -> IRect {
        let n = n.min(self.w);
        self.w -= n;
        IRect::new(self.x + self.w, self.y, n, self.h)
    }
    pub fn with_trimmed_left(self, n: i32) -> IRect {
        IRect::new(self.x + n, self.y, (self.w - n).max(0), self.h)
    }
    pub fn with_trimmed_right(self, n: i32) -> IRect {
        IRect::new(self.x, self.y, (self.w - n).max(0), self.h)
    }
    pub fn contains(self, x: f32, y: f32) -> bool {
        x >= self.x as f32 && y >= self.y as f32 && x < self.right() as f32 && y < self.bottom() as f32
    }
}

/// `juce::AffineTransform`: `x' = m00 x + m01 y + m02`, `y' = m10 x + m11 y + m12`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Affine {
    pub m00: f32,
    pub m01: f32,
    pub m02: f32,
    pub m10: f32,
    pub m11: f32,
    pub m12: f32,
}

impl Affine {
    pub const IDENTITY: Affine = Affine { m00: 1.0, m01: 0.0, m02: 0.0, m10: 0.0, m11: 1.0, m12: 0.0 };
    pub fn translation(dx: f32, dy: f32) -> Self {
        Affine { m02: dx, m12: dy, ..Self::IDENTITY }
    }
    pub fn scale(s: f32) -> Self {
        Affine { m00: s, m11: s, ..Self::IDENTITY }
    }
    pub fn followed_by(self, o: Affine) -> Self {
        Affine {
            m00: o.m00 * self.m00 + o.m01 * self.m10,
            m01: o.m00 * self.m01 + o.m01 * self.m11,
            m02: o.m00 * self.m02 + o.m01 * self.m12 + o.m02,
            m10: o.m10 * self.m00 + o.m11 * self.m10,
            m11: o.m10 * self.m01 + o.m11 * self.m11,
            m12: o.m10 * self.m02 + o.m11 * self.m12 + o.m12,
        }
    }
    pub fn translated(self, dx: f32, dy: f32) -> Self {
        Affine { m02: self.m02 + dx, m12: self.m12 + dy, ..self }
    }
    fn scaled(self, sx: f32, sy: f32) -> Self {
        Affine { m00: self.m00 * sx, m01: self.m01 * sx, m02: self.m02 * sx, m10: self.m10 * sy, m11: self.m11 * sy, m12: self.m12 * sy }
    }
    fn vertical_flip(h: f32) -> Self {
        Affine { m11: -1.0, m12: h, ..Self::IDENTITY }
    }
    pub fn apply(self, x: f32, y: f32) -> (f32, f32) {
        (self.m00 * x + self.m01 * y + self.m02, self.m10 * x + self.m11 * y + self.m12)
    }
    fn cg(self) -> CGAffineTransform {
        CGAffineTransform {
            a: f64::from(self.m00),
            b: f64::from(self.m10),
            c: f64::from(self.m01),
            d: f64::from(self.m11),
            tx: f64::from(self.m02),
            ty: f64::from(self.m12),
        }
    }
}

#[derive(Clone, Copy, Debug)]
enum El {
    Move(f32, f32),
    Line(f32, f32),
    Close,
}

/// `juce::Path` with the parts 1.5.3 used: polylines, closed subpaths and centred arcs.
#[derive(Clone, Debug, Default)]
pub struct Path {
    els: Vec<El>,
}

impl Path {
    pub fn new() -> Self {
        Path::default()
    }
    pub fn is_empty(&self) -> bool {
        // JUCE: a path is empty when it has no lineTo / curve (a lone startNewSubPath counts as empty)
        !self.els.iter().any(|e| matches!(e, El::Line(..)))
    }
    pub fn start_new_sub_path(&mut self, x: f32, y: f32) {
        self.els.push(El::Move(x, y));
    }
    pub fn line_to(&mut self, x: f32, y: f32) {
        if self.els.is_empty() {
            self.start_new_sub_path(0.0, 0.0);
        }
        self.els.push(El::Line(x, y));
    }
    pub fn close_sub_path(&mut self) {
        if matches!(self.els.last(), Some(El::Line(..))) {
            self.els.push(El::Close);
        }
    }
    /// `Path::addCentredArc` with no rotation: straight pieces every 0.05 rad.
    pub fn add_centred_arc(&mut self, cx: f32, cy: f32, rx: f32, ry: f32, from: f32, to: f32, start_new: bool) {
        if !(rx > 0.0 && ry > 0.0) {
            return;
        }
        const STEP: f32 = 0.05;
        let at = |a: f32| (cx + rx * a.sin(), cy - ry * a.cos());
        let mut angle = from;
        if start_new {
            let (x, y) = at(angle);
            self.start_new_sub_path(x, y);
        }
        if from < to {
            if start_new {
                angle += STEP;
            }
            while angle < to {
                let (x, y) = at(angle);
                self.line_to(x, y);
                angle += STEP;
            }
        } else {
            if start_new {
                angle -= STEP;
            }
            while angle > to {
                let (x, y) = at(angle);
                self.line_to(x, y);
                angle -= STEP;
            }
        }
        let (x, y) = at(to);
        self.line_to(x, y);
    }

    /// `PathStrokeType (thickness).createDashedStroke` for a path of one straight segment, the only
    /// kind 1.5.3 dashed: the dashes, each stroked with butt ends (a rectangle).
    pub fn dashed_stroke_of_line(x1: f32, y1: f32, x2: f32, y2: f32, dashes: &[f32], thickness: f32) -> Path {
        let mut dashes_path: Vec<Segment> = Vec::new();
        let (dx, dy) = (x2 - x1, y2 - y1);
        let line_len = dx.hypot(dy);
        let mut start: Option<(f32, f32)> = None;
        let mut first = true;
        let (mut dash_num, mut pos, mut line_end_pos) = (0usize, 0.0f32, 0.0f32);
        let mut segment_used = false;
        loop {
            let is_solid = dash_num & 1 == 0;
            let dash_len = dashes[dash_num % dashes.len()];
            dash_num += 1;
            if dash_len <= 0.0 {
                continue;
            }
            pos += dash_len;
            while pos > line_end_pos {
                if segment_used {
                    if is_solid && !first {
                        if let Some(s) = start {
                            dashes_path.push((s, (x2, y2)));
                        }
                    }
                    return stroke_segments(&dashes_path, thickness);
                }
                segment_used = true;
                if is_solid {
                    if first {
                        start = Some((x1, y1));
                    } else if let Some(s) = start {
                        dashes_path.push((s, (x1, y1)));
                        start = Some((x1, y1));
                    }
                }
                line_end_pos += line_len;
                first = false;
            }
            let alpha = (pos - (line_end_pos - line_len)) / line_len;
            let p = (x1 + dx * alpha, y1 + dy * alpha);
            if is_solid {
                if let Some(s) = start.take() {
                    dashes_path.push((s, p));
                }
            } else {
                start = Some(p);
            }
        }
    }
}

type Segment = ((f32, f32), (f32, f32));

fn stroke_segments(segments: &[Segment], thickness: f32) -> Path {
    let mut out = Path::new();
    let hw = thickness * 0.5;
    for &((ax, ay), (bx, by)) in segments {
        let len = (bx - ax).hypot(by - ay);
        if len <= 0.0 {
            continue;
        }
        let (nx, ny) = (-(by - ay) / len * hw, (bx - ax) / len * hw);
        out.start_new_sub_path(ax + nx, ay + ny);
        out.line_to(bx + nx, by + ny);
        out.line_to(bx - nx, by - ny);
        out.line_to(ax - nx, ay - ny);
        out.close_sub_path();
    }
    out
}

/// `PathStrokeType`: joint and end cap.
#[derive(Clone, Copy, Debug)]
pub struct Stroke {
    pub width: f32,
    pub curved: bool,
    pub rounded: bool,
}

impl Stroke {
    pub fn new(width: f32) -> Self {
        Stroke { width, curved: false, rounded: false }
    }
    /// `PathStrokeType (w, curved, rounded)`.
    pub fn curved_rounded(width: f32) -> Self {
        Stroke { width, curved: true, rounded: true }
    }
}

/// `juce::Justification`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Just {
    CentredLeft,
    Centred,
    CentredRight,
}

// ---- fonts -----------------------------------------------------------------------------------------

/// `juce::FontOptions (height, bold)` in Lucida Grande, with a horizontal scale.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Font {
    pub height: f32,
    pub bold: bool,
    pub hscale: f32,
}

impl Font {
    pub fn new(height: f32, bold: bool) -> Self {
        Font { height, bold, hscale: 1.0 }
    }
    fn face(self) -> &'static Face {
        faces(self.bold)
    }
    /// `Font::getHeightInPoints`.
    fn points(self) -> f32 {
        self.height / (self.face().ascent + self.face().descent)
    }
    /// `Font::getAscent`.
    fn ascent(self) -> f32 {
        self.face().ascent * (1.0 / (self.face().ascent + self.face().descent)) * self.height
    }
}

struct Face {
    font: Ref, // at 1 pt
    ascent: f32,
    descent: f32,
}
// CTFont is immutable and thread-safe.
unsafe impl Send for Face {}
unsafe impl Sync for Face {}

fn faces(bold: bool) -> &'static Face {
    use std::sync::OnceLock;
    static PLAIN: OnceLock<Face> = OnceLock::new();
    static BOLD: OnceLock<Face> = OnceLock::new();
    let make = |name: &str| {
        let cf = cf_string(name);
        let font = unsafe { CTFontCreateWithName(cf, 1.0, null()) };
        unsafe { CFRelease(cf) };
        let (ascent, descent) = hhea_extents(font);
        Face { font, ascent, descent }
    };
    if bold { BOLD.get_or_init(|| make("LucidaGrande-Bold")) } else { PLAIN.get_or_init(|| make("LucidaGrande")) }
}

/// HarfBuzz's horizontal extents (JUCE's "portable" metrics): `hhea` ascender and descender over the
/// units per em, or `OS/2`'s typographic ones when the font asks for them.
fn hhea_extents(font: Ref) -> (f32, f32) {
    let table = |tag: &[u8; 4]| -> Vec<u8> {
        let data = unsafe { CTFontCopyTable(font, u32::from_be_bytes(*tag), 0) };
        if data.is_null() {
            return Vec::new();
        }
        let bytes = unsafe { std::slice::from_raw_parts(CFDataGetBytePtr(data), CFDataGetLength(data) as usize) }.to_vec();
        unsafe { CFRelease(data) };
        bytes
    };
    let (head, hhea, os2) = (table(b"head"), table(b"hhea"), table(b"OS/2"));
    let u16_at = |t: &[u8], o: usize| t.get(o..o + 2).map_or(0, |b| u16::from_be_bytes([b[0], b[1]]));
    let i16_at = |t: &[u8], o: usize| u16_at(t, o) as i16;
    let upem = f32::from(u16_at(&head, 18).max(1));
    let use_typo = u16_at(&os2, 62) & (1 << 7) != 0;
    let (a, d) = if use_typo && i16_at(&os2, 68) != 0 { (i16_at(&os2, 68), i16_at(&os2, 70)) } else { (i16_at(&hhea, 4), i16_at(&hhea, 6)) };
    (f32::from(a).abs() / upem, f32::from(d).abs() / upem)
}

/// A retained CoreText font, released with the last glyph that uses it.
struct CtFont(Ref);
impl Drop for CtFont {
    fn drop(&mut self) {
        unsafe { CFRelease(self.0) };
    }
}
// CTFont is immutable and thread-safe.
unsafe impl Send for CtFont {}
unsafe impl Sync for CtFont {}

#[derive(Clone)]
struct Glyph {
    id: u16,
    font: std::sync::Arc<CtFont>, // the run's font (Lucida Grande or a fallback) at the point size
    advance: f32,
    whitespace: bool,
}

/// The glyphs of one line as HarfBuzz shapes them for JUCE: CoreText's glyphs, fallback fonts and
/// kerning at the point size, advances from `CTFontGetAdvancesForGlyphs` times the horizontal scale,
/// held at 1/65536 as `hb_position_t`.
fn shape(text: &str, font: Font) -> Vec<Glyph> {
    use std::cell::RefCell;
    use std::collections::HashMap;
    type Key = (String, u32, bool, u32);
    thread_local! {
        static CACHE: RefCell<HashMap<Key, Vec<Glyph>>> = RefCell::new(HashMap::new());
    }
    let key = (text.to_string(), font.height.to_bits(), font.bold, font.hscale.to_bits());
    if let Some(hit) = CACHE.with(|c| c.borrow().get(&key).cloned()) {
        return hit;
    }
    let glyphs = shape_uncached(text, font);
    CACHE.with(|c| {
        let mut c = c.borrow_mut();
        // readouts change with every value; a bounded cache keeps the steady ones
        if c.len() > 1024 {
            c.clear();
        }
        c.insert(key, glyphs.clone());
    });
    glyphs
}

fn shape_uncached(text: &str, font: Font) -> Vec<Glyph> {
    let mut out = Vec::new();
    if text.is_empty() {
        return out;
    }
    let chars: Vec<u16> = text.encode_utf16().collect();
    unsafe {
        let sized = CTFontCreateCopyWithAttributes(font.face().font, f64::from(font.points()), null(), null());
        let s = cf_string(text);
        let keys = [kCTFontAttributeName];
        let values = [sized];
        let attrs = CFDictionaryCreate(
            null(),
            keys.as_ptr(),
            values.as_ptr(),
            1,
            (&raw const kCFTypeDictionaryKeyCallBacks).cast(),
            (&raw const kCFTypeDictionaryValueCallBacks).cast(),
        );
        let attributed = CFAttributedStringCreate(null(), s, attrs);
        let line = CTLineCreateWithAttributedString(attributed);
        let runs = CTLineGetGlyphRuns(line);
        for r in 0..CFArrayGetCount(runs) {
            let run = CFArrayGetValueAtIndex(runs, r);
            let n = CTRunGetGlyphCount(run);
            if n <= 0 {
                continue;
            }
            let all = CFRange { location: 0, length: n };
            let mut ids = vec![0u16; n as usize];
            let mut idx = vec![0isize; n as usize];
            let mut kerned = vec![CGSize { w: 0.0, h: 0.0 }; n as usize];
            CTRunGetGlyphs(run, all, ids.as_mut_ptr());
            CTRunGetStringIndices(run, all, idx.as_mut_ptr());
            CTRunGetAdvances(run, all, kerned.as_mut_ptr());
            let run_font = CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName);
            let run_font = if run_font.is_null() { sized } else { run_font };
            let mut plain = vec![CGSize { w: 0.0, h: 0.0 }; n as usize];
            CTFontGetAdvancesForGlyphs(run_font, 1, ids.as_ptr(), plain.as_mut_ptr(), n);
            let shared = std::sync::Arc::new(CtFont(CFRetain(run_font)));
            for i in 0..n as usize {
                // the plain advance as JUCE asks for it, plus the kerning CoreText's layout applied
                let kern = kerned[i].w - plain[i].w;
                let advance = ((plain[i].w + kern) as f32) * font.hscale;
                let c = idx[i].max(0) as usize;
                let whitespace = chars.get(c).is_some_and(|&c| c == 0x20 || c == 0x09 || c == 0xa0);
                out.push(Glyph { id: ids[i], font: shared.clone(), advance: (advance * 65536.0) as i32 as f32 / 65536.0, whitespace });
            }
        }
        CFRelease(line);
        CFRelease(attributed);
        CFRelease(attrs);
        CFRelease(s);
        CFRelease(sized);
    }
    out
}

/// Line length without trailing whitespace, as JustifiedText measures it.
fn effective_length(glyphs: &[Glyph]) -> f32 {
    let total: f32 = glyphs.iter().map(|g| g.advance).sum();
    let trailing: f32 = glyphs.iter().rev().take_while(|g| g.whitespace).map(|g| g.advance).sum();
    total - trailing
}

/// A single line: glyphs and the x of each.
struct Line {
    glyphs: Vec<Glyph>,
    xs: Vec<f32>,
    font: Font,
}

fn place(glyphs: Vec<Glyph>, anchor: f32, font: Font) -> Line {
    let mut x = anchor;
    let xs = glyphs
        .iter()
        .map(|g| {
            let at = x;
            x += g.advance;
            at
        })
        .collect();
    Line { glyphs, xs, font }
}

/// The visible part of a line that does not fit `width`, with an ellipsis (JustifiedText).
fn curtail(mut glyphs: Vec<Glyph>, width: f32, font: Font) -> Vec<Glyph> {
    let ellipsis = shape("\u{2026}", font);
    let ellipsis_len = effective_length(&ellipsis);
    let mut length: f32 = glyphs.iter().map(|g| g.advance).sum();
    let mut keep = 0;
    for i in (0..glyphs.len()).rev() {
        length -= glyphs[i].advance;
        if width >= ellipsis_len + length {
            keep = i;
            break;
        }
    }
    glyphs.truncate(keep);
    glyphs.extend(ellipsis);
    glyphs
}

// ---- the context -----------------------------------------------------------------------------------

#[derive(Clone, Copy, Debug)]
pub struct Gradient {
    pub c1: Colour,
    pub x1: f32,
    pub y1: f32,
    pub c2: Colour,
    pub x2: f32,
    pub y2: f32,
}

/// `juce::ColourGradient (c1, x1, y1, c2, x2, y2, false)`.
pub fn gradient(c1: Colour, x1: f32, y1: f32, c2: Colour, x2: f32, y2: f32) -> Gradient {
    Gradient { c1, x1, y1, c2, x2, y2 }
}

#[derive(Clone, Copy, Debug)]
enum Fill {
    Colour,
    Gradient(Gradient),
}

#[derive(Clone, Copy)]
struct SavedState {
    fill: Fill,
    font: Font,
}

/// An offscreen sRGB bitmap and a `juce::Graphics` on it.
pub struct Canvas {
    ctx: Ref,
    rgb: Ref,
    pixels: Vec<u32>,
    pub width: usize,
    pub height: usize,
}

impl Canvas {
    pub fn new(width: usize, height: usize) -> Self {
        let mut pixels = vec![0u32; width.max(1) * height.max(1)];
        let rgb = unsafe { CGColorSpaceCreateWithName(kCGColorSpaceSRGB) };
        // premultiplied first, host (little-endian) order: each word is 0xAARRGGBB, as JUCE's images
        let info = 2 | (2 << 12);
        let ctx = unsafe { CGBitmapContextCreate(pixels.as_mut_ptr().cast(), width.max(1), height.max(1), 8, width.max(1) * 4, rgb, info) };
        Canvas { ctx, rgb, pixels, width: width.max(1), height: height.max(1) }
    }
    /// Premultiplied `0xAARRGGBB`, top row first.
    pub fn pixels(&self) -> &[u32] {
        &self.pixels
    }
    /// The editor's whole picture is opaque, so the frame is the pixels without the alpha byte.
    pub fn opaque_pixels(&mut self) -> &[u32] {
        for p in self.pixels.iter_mut() {
            *p &= 0x00ff_ffff;
        }
        &self.pixels
    }
    /// The pixels of `r` (device pixels), row by row.
    pub fn copy_out(&self, r: IRect) -> Vec<u32> {
        let mut out = Vec::with_capacity((r.w * r.h).max(0) as usize);
        for y in r.y.max(0)..r.bottom().min(self.height as i32) {
            let row = y as usize * self.width;
            out.extend_from_slice(&self.pixels[row + r.x.max(0) as usize..row + r.right().min(self.width as i32) as usize]);
        }
        out
    }
    /// Puts back what `copy_out` took from `r`, opaque.
    pub fn copy_in(&mut self, r: IRect, px: &[u32]) {
        let w = (r.right().min(self.width as i32) - r.x.max(0)).max(0) as usize;
        for (i, y) in (r.y.max(0)..r.bottom().min(self.height as i32)).enumerate() {
            let row = y as usize * self.width + r.x.max(0) as usize;
            for (d, s) in self.pixels[row..row + w].iter_mut().zip(&px[i * w..(i + 1) * w]) {
                *d = *s | 0xff00_0000;
            }
        }
    }
    /// A `Graphics` drawing at `scale` device pixels per unit, as JUCE's `Graphics (image)` followed by
    /// `addTransform (AffineTransform::scale (scale))`.
    pub fn graphics(&mut self, scale: f32) -> Graphics<'_> {
        self.pixels.fill(0);
        let mut g = Graphics::new(self.ctx, self.rgb, self.height as f32);
        g.add_transform(Affine::scale(scale));
        g
    }
    /// The same, repainting only inside `rects` (device pixels, origin top left) and keeping every
    /// other pixel of the last frame.
    pub fn graphics_within(&mut self, scale: f32, rects: &[IRect]) -> Graphics<'_> {
        let h = self.height as f64;
        let cg: Vec<CGRect> = rects.iter().map(|r| cg_rect(f64::from(r.x), h - f64::from(r.bottom()), f64::from(r.w), f64::from(r.h))).collect();
        let mut g = Graphics::new(self.ctx, self.rgb, self.height as f32);
        unsafe { CGContextClipToRects(self.ctx, cg.as_ptr(), cg.len()) };
        g.add_transform(Affine::scale(scale));
        g
    }
}

impl Drop for Canvas {
    fn drop(&mut self) {
        unsafe {
            CGContextRelease(self.ctx);
            CGColorSpaceRelease(self.rgb);
        }
    }
}

/// `juce::Graphics` over JUCE's `CoreGraphicsContext`.
pub struct Graphics<'a> {
    ctx: Ref,
    rgb: Ref,
    flip: f64,
    fill: Fill,
    font: Font,
    stack: Vec<SavedState>,
    _canvas: std::marker::PhantomData<&'a mut Canvas>,
}

impl Graphics<'_> {
    fn new(ctx: Ref, rgb: Ref, flip_height: f32) -> Self {
        unsafe {
            CGContextSaveGState(ctx);
            CGContextSetShouldSmoothFonts(ctx, true);
            CGContextSetAllowsFontSmoothing(ctx, true);
            CGContextSetShouldAntialias(ctx, true);
            CGContextSetBlendMode(ctx, 0);
        }
        let mut g = Graphics {
            ctx,
            rgb,
            flip: f64::from(flip_height),
            fill: Fill::Colour,
            font: Font::new(15.0, false),
            stack: Vec::new(),
            _canvas: std::marker::PhantomData,
        };
        g.set_colour(Colour(0xff00_0000));
        g
    }

    fn concat(&self, t: Affine) {
        unsafe { CGContextConcatCTM(self.ctx, t.cg()) };
    }
    fn flip_ctm(&self) {
        unsafe { CGContextConcatCTM(self.ctx, CGAffineTransform { a: 1.0, b: 0.0, c: 0.0, d: -1.0, tx: 0.0, ty: self.flip }) };
    }
    fn flipped(&self, r: Rect) -> CGRect {
        cg_rect(f64::from(r.x), self.flip - f64::from(r.bottom()), f64::from(r.w), f64::from(r.h))
    }

    pub fn add_transform(&mut self, t: Affine) {
        let h = self.flip as f32;
        self.concat(Affine::vertical_flip(h).followed_by(t).translated(0.0, -h).scaled(1.0, -1.0));
    }
    /// `setOrigin`: component positions are whole units.
    pub fn set_origin(&mut self, x: i32, y: i32) {
        unsafe { CGContextTranslateCTM(self.ctx, f64::from(x), f64::from(-y)) };
    }
    pub fn reduce_clip_region(&mut self, r: IRect) {
        unsafe { CGContextClipToRect(self.ctx, cg_rect(f64::from(r.x), self.flip - f64::from(r.bottom()), f64::from(r.w), f64::from(r.h))) };
    }
    /// `clipRegionIntersects`: whether anything of `r` (current coordinates) can still be painted.
    pub fn clip_intersects(&self, r: IRect) -> bool {
        let b = unsafe { CGContextGetClipBoundingBox(self.ctx) };
        let (x0, x1) = (b.origin.x, b.origin.x + b.size.w);
        let (y0, y1) = (self.flip - (b.origin.y + b.size.h), self.flip - b.origin.y);
        b.size.w > 0.0
            && b.size.h > 0.0
            && f64::from(r.x) < x1
            && f64::from(r.right()) > x0
            && f64::from(r.y) < y1
            && f64::from(r.bottom()) > y0
    }

    pub fn save_state(&mut self) {
        unsafe { CGContextSaveGState(self.ctx) };
        self.stack.push(SavedState { fill: self.fill, font: self.font });
    }
    pub fn restore_state(&mut self) {
        unsafe { CGContextRestoreGState(self.ctx) };
        if let Some(s) = self.stack.pop() {
            self.fill = s.fill;
            self.font = s.font;
        }
    }
    pub fn begin_transparency_layer(&mut self, opacity: f32) {
        self.save_state();
        unsafe {
            CGContextSetAlpha(self.ctx, f64::from(opacity));
            CGContextBeginTransparencyLayer(self.ctx, null());
        }
    }
    pub fn end_transparency_layer(&mut self) {
        unsafe { CGContextEndTransparencyLayer(self.ctx) };
        self.restore_state();
    }

    pub fn set_colour(&mut self, c: Colour) {
        self.fill = Fill::Colour;
        unsafe {
            let col = CGColorCreate(self.rgb, c.components().as_ptr());
            CGContextSetFillColorWithColor(self.ctx, col);
            CGContextSetStrokeColorWithColor(self.ctx, col);
            CGColorRelease(col);
            CGContextSetAlpha(self.ctx, 1.0);
        }
    }
    pub fn set_gradient_fill(&mut self, g: Gradient) {
        self.fill = Fill::Gradient(g);
    }
    pub fn set_font(&mut self, f: Font) {
        self.font = f;
    }

    fn draw_gradient(&self, g: Gradient) {
        self.flip_ctm();
        unsafe {
            CGContextSetAlpha(self.ctx, 1.0);
            let mut comps = [0.0f64; 8];
            comps[..4].copy_from_slice(&g.c1.components());
            comps[4..].copy_from_slice(&g.c2.components());
            let locs = [0.0f64, 1.0];
            let grad = CGGradientCreateWithColorComponents(self.rgb, comps.as_ptr(), locs.as_ptr(), 2);
            let p1 = CGPoint { x: f64::from(g.x1), y: f64::from(g.y1) };
            let p2 = CGPoint { x: f64::from(g.x2), y: f64::from(g.y2) };
            CGContextDrawLinearGradient(self.ctx, grad, p1, p2, GRADIENT_EXTEND);
            CGGradientRelease(grad);
        }
    }

    fn draw_current_path(&self, mode: i32) {
        match self.fill {
            Fill::Colour => unsafe { CGContextDrawPath(self.ctx, mode) },
            Fill::Gradient(g) => unsafe {
                if mode == PATH_STROKE {
                    CGContextReplacePathWithStrokedPath(self.ctx);
                }
                CGContextSaveGState(self.ctx);
                if mode == PATH_EO_FILL {
                    CGContextEOClip(self.ctx);
                } else {
                    CGContextClip(self.ctx);
                }
                self.draw_gradient(g);
                CGContextRestoreGState(self.ctx);
            },
        }
    }

    fn fill_cg_rect(&self, r: CGRect) {
        if r.size.w <= 0.0 || r.size.h <= 0.0 {
            return;
        }
        match self.fill {
            Fill::Colour => unsafe { CGContextFillRect(self.ctx, r) },
            Fill::Gradient(g) => unsafe {
                CGContextSaveGState(self.ctx);
                CGContextClipToRect(self.ctx, r);
                self.draw_gradient(g);
                CGContextRestoreGState(self.ctx);
            },
        }
    }

    pub fn fill_rect(&mut self, r: Rect) {
        self.fill_cg_rect(self.flipped(r));
    }
    /// `fillAll (colour)`: a rectangle larger than any clip.
    pub fn fill_all(&mut self, c: Colour) {
        self.save_state();
        self.set_colour(c);
        self.fill_rect(Rect::new(-1.0e5, -1.0e5, 2.0e5, 2.0e5));
        self.restore_state();
    }
    pub fn draw_horizontal_line(&mut self, y: i32, left: f32, right: f32) {
        if left < right {
            self.fill_rect(Rect::new(left, y as f32, right - left, 1.0));
        }
    }
    pub fn draw_vertical_line(&mut self, x: i32, top: f32, bottom: f32) {
        if top < bottom {
            self.fill_rect(Rect::new(x as f32, top, 1.0, bottom - top));
        }
    }

    fn add_path(&self, path: &Path, t: Affine) {
        unsafe { CGContextBeginPath(self.ctx) };
        for e in &path.els {
            match *e {
                El::Move(x, y) => {
                    let (x, y) = t.apply(x, y);
                    unsafe { CGContextMoveToPoint(self.ctx, f64::from(x), self.flip - f64::from(y)) };
                }
                El::Line(x, y) => {
                    let (x, y) = t.apply(x, y);
                    unsafe { CGContextAddLineToPoint(self.ctx, f64::from(x), self.flip - f64::from(y)) };
                }
                El::Close => unsafe { CGContextClosePath(self.ctx) },
            }
        }
    }
    pub fn fill_path(&mut self, path: &Path) {
        self.fill_path_transformed(path, Affine::IDENTITY);
    }
    pub fn fill_path_transformed(&mut self, path: &Path, t: Affine) {
        if path.is_empty() {
            return;
        }
        self.add_path(path, t);
        self.draw_current_path(PATH_FILL);
    }
    pub fn stroke_path(&mut self, path: &Path, s: Stroke) {
        if path.is_empty() {
            return;
        }
        unsafe {
            CGContextSetLineWidth(self.ctx, f64::from(s.width));
            CGContextSetLineCap(self.ctx, if s.rounded { 1 } else { 0 });
            CGContextSetLineJoin(self.ctx, if s.curved { 1 } else { 0 });
        }
        self.add_path(path, Affine::IDENTITY);
        self.draw_current_path(PATH_STROKE);
    }

    pub fn fill_ellipse(&mut self, r: Rect) {
        unsafe {
            CGContextBeginPath(self.ctx);
            CGContextAddEllipseInRect(self.ctx, self.flipped(r));
        }
        self.draw_current_path(PATH_FILL);
    }
    pub fn draw_ellipse(&mut self, r: Rect, thickness: f32) {
        unsafe {
            CGContextBeginPath(self.ctx);
            CGContextSetLineWidth(self.ctx, f64::from(thickness));
            CGContextSetLineCap(self.ctx, 0);
            CGContextSetLineJoin(self.ctx, 0);
            CGContextAddEllipseInRect(self.ctx, self.flipped(r));
        }
        self.draw_current_path(PATH_STROKE);
    }
    pub fn fill_rounded_rectangle(&mut self, r: Rect, corner: f32) {
        unsafe {
            CGContextBeginPath(self.ctx);
            let p = CGPathCreateWithRoundedRect(
                self.flipped(r),
                f64::from(corner.clamp(0.0, (r.w / 2.0).max(0.0))),
                f64::from(corner.clamp(0.0, (r.h / 2.0).max(0.0))),
                null(),
            );
            CGContextAddPath(self.ctx, p);
            CGPathRelease(p);
        }
        self.draw_current_path(PATH_FILL);
    }
    pub fn draw_rounded_rectangle(&mut self, r: Rect, corner: f32, thickness: f32) {
        unsafe { CGContextBeginPath(self.ctx) };
        match self.fill {
            Fill::Colour => unsafe {
                CGContextSetLineWidth(self.ctx, f64::from(thickness));
                CGContextSetLineCap(self.ctx, 0);
                CGContextSetLineJoin(self.ctx, 0);
                let p = CGPathCreateWithRoundedRect(
                    self.flipped(r),
                    f64::from(corner.clamp(0.0, (r.w / 2.0).max(0.0))),
                    f64::from(corner.clamp(0.0, (r.h / 2.0).max(0.0))),
                    null(),
                );
                CGContextAddPath(self.ctx, p);
                CGPathRelease(p);
                self.draw_current_path(PATH_STROKE);
            },
            Fill::Gradient(_) => unsafe {
                let half = thickness * 0.5;
                let (outer, inner) = (r.expanded(half), r.reduced(half));
                let (oc, ic) = (corner + half, corner - half);
                let p = CGPathCreateMutable();
                let clampc = |c: f32, side: f32| f64::from(c.clamp(0.0, (side / 2.0).max(0.0)));
                CGPathAddRoundedRect(p, null(), self.flipped(outer), clampc(oc, outer.w), clampc(oc, outer.h));
                CGPathAddRoundedRect(p, null(), self.flipped(inner), clampc(ic, inner.w), clampc(ic, inner.h));
                CGContextAddPath(self.ctx, p);
                CGPathRelease(p);
                self.draw_current_path(PATH_EO_FILL);
            },
        }
    }
    /// `drawRect (r, thickness)`: four filled sides (LowLevelGraphicsContext::drawRect).
    pub fn draw_rect(&mut self, r: Rect, t: f32) {
        self.fill_rect(Rect::new(r.x, r.y, r.w, t));
        self.fill_rect(Rect::new(r.x, r.bottom() - t, r.w, t));
        self.fill_rect(Rect::new(r.x, r.y + t, t, r.h - 2.0 * t));
        self.fill_rect(Rect::new(r.right() - t, r.y + t, t, r.h - 2.0 * t));
    }
    /// `drawLine (line, thickness)`: CoreGraphicsContext::drawLineWithThickness.
    pub fn draw_line(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, thickness: f32) {
        let half = thickness * 0.5;
        // Line::getPointAlongLine (0, ±half): perpendicular offsets at each end
        let along = |ax: f32, ay: f32, bx: f32, by: f32, perp: f32| {
            let len = (bx - ax).hypot(by - ay);
            if len <= 0.0 {
                return (ax, ay);
            }
            let (dx, dy) = ((bx - ax) / len, (by - ay) / len);
            (ax - perp * dy, ay + perp * dx)
        };
        let p1 = along(x1, y1, x2, y2, half);
        let p2 = along(x1, y1, x2, y2, -half);
        let p3 = along(x2, y2, x1, y1, half);
        let p4 = along(x2, y2, x1, y1, -half);
        unsafe {
            CGContextBeginPath(self.ctx);
            CGContextMoveToPoint(self.ctx, f64::from(p1.0), self.flip - f64::from(p1.1));
            CGContextAddLineToPoint(self.ctx, f64::from(p2.0), self.flip - f64::from(p2.1));
            CGContextAddLineToPoint(self.ctx, f64::from(p3.0), self.flip - f64::from(p3.1));
            CGContextAddLineToPoint(self.ctx, f64::from(p4.0), self.flip - f64::from(p4.1));
            CGContextClosePath(self.ctx);
        }
        self.draw_current_path(PATH_FILL);
    }

    // ---- text ----

    fn draw_line_of_glyphs(&mut self, line: &Line, baseline: f32, origin: (f32, f32)) {
        let Fill::Colour = self.fill else { return };
        let f = line.font;
        let text_matrix = CGAffineTransform { a: f64::from(f.hscale), b: 0.0, c: 0.0, d: 1.0, tx: 0.0, ty: 0.0 };
        let mut i = 0;
        while i < line.glyphs.len() {
            let font = line.glyphs[i].font.clone();
            let mut ids = Vec::new();
            let mut pos = Vec::new();
            while i < line.glyphs.len() && std::sync::Arc::ptr_eq(&line.glyphs[i].font, &font) {
                let g = &line.glyphs[i];
                if !g.whitespace {
                    ids.push(g.id);
                    // CGPointApplyAffineTransform ((x, -y), inverseTextMatrix)
                    pos.push(CGPoint { x: f64::from(line.xs[i]) / f64::from(f.hscale), y: -f64::from(baseline) });
                }
                i += 1;
            }
            if ids.is_empty() {
                continue;
            }
            unsafe {
                CGContextSetTextMatrix(self.ctx, text_matrix);
                CGContextSaveGState(self.ctx);
                self.flip_ctm();
                self.concat(Affine { m11: -1.0, ..Affine::IDENTITY }.followed_by(Affine::translation(origin.0, origin.1)));
                CTFontDrawGlyphs(font.0, ids.as_ptr(), pos.as_ptr(), ids.len(), self.ctx);
                CGContextRestoreGState(self.ctx);
            }
        }
    }

    /// `drawText (text, area, justification, useEllipsesIfTooBig = true)`.
    pub fn draw_text(&mut self, text: &str, area: Rect, just: Just) {
        if text.is_empty() {
            return;
        }
        let font = self.font;
        let mut glyphs = shape(text, font);
        if effective_length(&glyphs) > area.w + 0.005 {
            glyphs = curtail(glyphs, area.w, font);
        }
        let line = place(glyphs, 0.0, font);
        // justifyGlyphs over the glyph bounds (whitespace counted unless centred)
        let include_ws = just != Just::Centred;
        let mut left = f32::INFINITY;
        let mut right = f32::NEG_INFINITY;
        for (g, x) in line.glyphs.iter().zip(&line.xs) {
            if include_ws || !g.whitespace {
                left = left.min(*x);
                right = right.max(x + g.advance);
            }
        }
        if !left.is_finite() {
            return;
        }
        let bw = right - left;
        let dx = match just {
            Just::Centred => (area.w - bw) * 0.5 - left,
            Just::CentredRight => area.w - right,
            Just::CentredLeft => -left,
        };
        let ascent = font.ascent();
        let dy = (area.h - font.height) * 0.5 + ascent;
        let mut line = line;
        for x in line.xs.iter_mut() {
            *x += dx;
        }
        self.draw_line_of_glyphs(&line, dy, (area.x, area.y));
    }

    pub fn draw_text_i(&mut self, text: &str, x: i32, y: i32, w: i32, h: i32, just: Just) {
        self.draw_text(text, IRect::new(x, y, w, h).to_float(), just);
    }

    /// `drawFittedText (text, area, justification, 1, minimumHorizontalScale)`.
    pub fn draw_fitted_text(&mut self, text: &str, area: IRect, just: Just, min_scale: f32) {
        if text.is_empty() || area.w <= 0 || area.h <= 0 {
            return;
        }
        let (width, height) = (area.w as f32, area.h as f32);
        let trimmed = text.trim();
        let mut font = self.font;
        let glyphs = shape(trimmed, font);
        let required = effective_length(&glyphs);
        let glyphs = if required <= width {
            glyphs
        } else if required * min_scale < width {
            font.hscale *= width / (required + 0.01);
            shape(trimmed, font)
        } else {
            font.hscale *= min_scale;
            let g = shape(trimmed, font);
            if effective_length(&g) > width + 0.005 { curtail(g, width, font) } else { g }
        };
        let len = effective_length(&glyphs);
        let too_long = width + 0.005 < len;
        let anchor = if too_long {
            0.0
        } else {
            match just {
                Just::Centred => (width - len) / 2.0,
                Just::CentredRight => width - len,
                Just::CentredLeft => 0.0,
            }
        };
        let line = place(glyphs, anchor, font);
        let baseline = (height - font.height) / 2.0 + font.ascent();
        self.draw_line_of_glyphs(&line, baseline, (area.x as f32, area.y as f32));
    }

    /// The advance width of `text` (`GlyphArrangement::addLineOfText` bounds, whitespace included).
    pub fn text_width(text: &str, font: Font) -> f32 {
        let glyphs = shape(text, font);
        glyphs.iter().map(|g| g.advance).sum()
    }
}

impl Drop for Graphics<'_> {
    fn drop(&mut self) {
        while !self.stack.is_empty() {
            self.restore_state();
        }
        unsafe { CGContextRestoreGState(self.ctx) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn colour_arithmetic_is_juces() {
        // worked by hand from juce_Colour.cpp and juce_PixelFormats.h
        let c = Colour(0xff29_ec6b);
        assert_eq!(c.with_alpha(0.5), Colour(0x8029_ec6b));
        assert_eq!(c.with_multiplied_alpha(0.42), Colour(0x6b29_ec6b));
        assert_eq!(round_to_int(2.5), 2);
        assert_eq!(round_to_int(3.5), 4);
        assert_eq!(c.brighter(0.2).0 >> 24, 0xff);
        // tween by round(127.5) = 128 of 256: 0 + (255 * 128) >> 8 = 127
        assert_eq!(Colour(0xff00_0000).interpolated_with(WHITE, 0.5), Colour(0xff7f_7f7f));
    }

    #[test]
    fn dashes_cover_the_line() {
        let p = Path::dashed_stroke_of_line(0.0, 0.0, 20.0, 0.0, &[6.0, 3.5], 1.3);
        // dashes at 0..6, 9.5..15.5, 19..20
        let moves: Vec<f32> = p.els.iter().filter_map(|e| if let El::Move(x, _) = e { Some(*x) } else { None }).collect();
        assert_eq!(moves.len(), 3);
        assert!((moves[1] - 9.5).abs() < 1e-4);
    }

    #[test]
    fn lucida_grande_is_found() {
        let f = Font::new(10.0, true);
        assert!(f.points() > 8.0 && f.points() < 10.0, "{}", f.points());
        assert!(Graphics::text_width("THRESHOLD", f) > 20.0);
    }
}

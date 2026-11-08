#!/usr/bin/env python3
"""Build the versioned Harmonic Compressor PDF manual from Docs/UserManual.md (requires reportlab)."""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak

ROOT = Path(__file__).resolve().parents[1]
VERSION_FILE = ROOT / "VERSION"
VERSION = VERSION_FILE.read_text().strip() if VERSION_FILE.exists() else "1.5.0"
SOURCE = ROOT / "Docs/UserManual.md"
OUTPUT = ROOT / "output/pdf/HarmonicCompressor-User-Manual.pdf"

def inline(text):
    text = escape(text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`([^`]+)`", r"<font name='Courier'>\1</font>", text)
    return re.sub(r"\[([^\]]+)\]\((https?://[^)]+)\)", r'<link href="\2" color="#0d5b75">\1</link>', text)

def main():
    text = SOURCE.read_text(encoding="utf-8")
    if f"version {VERSION}" not in text:
        raise SystemExit(f"Manual version must match VERSION ({VERSION})")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    
    styles = getSampleStyleSheet()
    body = ParagraphStyle(
        "ManualBody",
        fontName="Helvetica",
        fontSize=9.2,
        leading=13.5,
        textColor=colors.HexColor("#22292f"),
        spaceAfter=8
    )
    cell = ParagraphStyle(
        "ManualCell",
        parent=body,
        fontSize=7.8,
        leading=10.5,
        spaceAfter=0
    )
    cell_header = ParagraphStyle(
        "ManualCellHeader",
        parent=body,
        fontName="Helvetica-Bold",
        fontSize=8.0,
        leading=11.0,
        textColor=colors.HexColor("#0d3f54"),
        spaceAfter=0
    )
    head = ParagraphStyle(
        "ManualHead",
        parent=body,
        fontName="Helvetica-Bold",
        fontSize=20,
        leading=24,
        textColor=colors.HexColor("#0e4d64"),
        spaceAfter=12,
        keepWithNext=True
    )
    sub = ParagraphStyle(
        "ManualSub",
        parent=head,
        fontSize=12.5,
        leading=16,
        textColor=colors.HexColor("#1b627d"),
        spaceAfter=8,
        keepWithNext=True
    )
    sub3 = ParagraphStyle(
        "ManualSub3",
        parent=sub,
        fontSize=10.5,
        leading=14,
        textColor=colors.HexColor("#2e4049"),
        spaceAfter=6,
        keepWithNext=True
    )
    bullet = ParagraphStyle(
        "ManualBullet",
        parent=body,
        leftIndent=12,
        firstLineIndent=-9,
        bulletIndent=0,
        spaceAfter=4
    )
    
    width = A4[0] - 88
    story = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line == "<!-- pagebreak -->":
            story.append(PageBreak())
            i += 1
            continue
        if line.startswith("!["):
            target = re.fullmatch(r"!\[[^\]]*\]\(([^)]+)\)", line)
            path = SOURCE.parent / target.group(1)
            if path.exists():
                w, h = ImageReader(str(path)).getSize()
                # Max image height to avoid overflow
                max_h = 320
                render_w = width
                render_h = width * h / w
                if render_h > max_h:
                    render_h = max_h
                    render_w = max_h * w / h
                story.extend([Image(str(path), width=render_w, height=render_h), Spacer(1, 10)])
            i += 1
            continue
        if line.startswith("|"):
            rows = []
            is_header = True
            while i < len(lines) and lines[i].strip().startswith("|"):
                parts = [v.strip() for v in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r"[-: ]+", v) for v in parts):
                    if is_header:
                        rows.append([Paragraph(inline(v), cell_header) for v in parts])
                        is_header = False
                    else:
                        rows.append([Paragraph(inline(v), cell) for v in parts])
                i += 1
            if rows:
                n = len(rows[0])
                if n == 2:
                    widths = [110, width - 110]
                elif n == 6:
                    widths = [94, 79, 85, 52, 38, width - 348]
                else:
                    widths = [width / n] * n
                table = Table(rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
                table.setStyle(TableStyle([
                    ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e2edf2")),
                    ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#c0d2dc")),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("LEFTPADDING", (0, 0), (-1, -1), 5),
                    ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                    ("TOPPADDING", (0, 0), (-1, -1), 4),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 4)
                ]))
                story.extend([table, Spacer(1, 10)])
            continue
        if line.startswith("# "):
            story.append(Paragraph(inline(line[2:]), head))
            i += 1
            continue
        if line.startswith("## "):
            story.append(Paragraph(inline(line[3:]), sub))
            i += 1
            continue
        if line.startswith("### "):
            story.append(Paragraph(inline(line[4:]), sub3))
            i += 1
            continue
        if line.startswith("- "):
            story.append(Paragraph("• " + inline(line[2:]), bullet))
            i += 1
            continue
        if line.startswith("$$") and line.endswith("$$"):
            # KaTeX / math line
            math_text = line[2:-2].strip()
            story.append(Paragraph(f"<i>{escape(math_text)}</i>", ParagraphStyle("Math", parent=body, alignment=TA_CENTER, textColor=colors.HexColor("#1b4d61"))))
            i += 1
            continue
        
        paragraph = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("#", "|", "- ", "![", "<!--", "$$")):
            paragraph.append(lines[i].strip())
            i += 1
        story.append(Paragraph(inline(" ".join(paragraph)), body))

    def footer(canvas, doc):
        canvas.saveState()
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#59717d"))
        canvas.drawString(44, 25, f"OpenGrid / Harmonic Compressor {VERSION} — Analogue Dynamics Engine")
        canvas.drawRightString(A4[0] - 44, 25, f"Page {doc.page}")
        canvas.restoreState()

    doc = SimpleDocTemplate(
        str(OUTPUT),
        pagesize=A4,
        rightMargin=44,
        leftMargin=44,
        topMargin=38,
        bottomMargin=42,
        title=f"Harmonic Compressor {VERSION} User Manual",
        author="OpenGrid",
        creator="Harmonic Compressor documentation"
    )
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print(f"Built manual PDF successfully: {OUTPUT}")

if __name__ == "__main__":
    main()

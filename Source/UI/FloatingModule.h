#pragma once

#include "Toolkit.h"
#include <functional>

// A collapsible, user-movable smoked dark-glass HUD module floating inside the Compression History field.
// - Starts closed as a compact hardware capsule displaying a machined grip handle, glowing LED status bar,
//   module title, recessed OLED live parameter summary, and expand chevron.
// - Clicking the header (without dragging) opens or closes the module body.
// - Dragging the header (or closed pill) moves the module freely within its parent (the Compression
//   History viewport), clamping to the parent bounds so it can never be lost off-screen.
class FloatingModule : public juce::Component
{
public:
    static constexpr int kHeaderHeight = 28;

    FloatingModule (juce::String moduleTitle, juce::Colour accentColour,
                    juce::Point<int> closedDimensions, juce::Point<int> openDimensions)
        : title (std::move (moduleTitle)),
          accent (accentColour),
          closedSize (closedDimensions),
          openSize (openDimensions)
    {
        addChildComponent (body);
        setSize (closedSize.x, closedSize.y);
    }

    juce::Component& content() noexcept { return body; }
    bool isOpen() const noexcept { return open; }

    void setSkin (const Skin* s) noexcept { skin = s; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setHighlighted (bool h) { if (highlighted != h) { highlighted = h; repaint(); } }
    void setSummary (const juce::String& s)
    {
        if (summary != s)
        {
            summary = s;
            repaint (0, 0, getWidth(), kHeaderHeight);
        }
    }

    void setOpen (bool shouldOpen, bool notify = true)
    {
        if (open == shouldOpen)
            return;
        open = shouldOpen;
        body.setVisible (open);
        const auto targetSize = open ? openSize : closedSize;
        setSize (targetSize.x, targetSize.y);
        if (open)
            toFront (false);
        clampToParent();
        resized();
        repaint();
        if (notify && onStateChanged)
            onStateChanged();
    }

    void clampToParent()
    {
        auto* p = getParentComponent();
        if (p == nullptr || p->getWidth() <= 0 || p->getHeight() <= 0)
            return;

        constexpr int margin = 6;
        const int maxX = juce::jmax (margin, p->getWidth() - getWidth() - margin);
        const int maxY = juce::jmax (margin, p->getHeight() - getHeight() - margin);
        const int nx = juce::jlimit (margin, maxX, getX());
        const int ny = juce::jlimit (margin, maxY, getY());
        if (nx != getX() || ny != getY())
            setTopLeftPosition (nx, ny);
    }

    void resized() override
    {
        body.setBounds (0, kHeaderHeight, getWidth(), juce::jmax (0, getHeight() - kHeaderHeight));
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = skin != nullptr ? skin->theme() : Skin::defaultTheme();
        const auto b = getLocalBounds().toFloat().reduced (1.0f);

        // Drop shadow so HUD modules float clearly above the waveform
        g.setColour (juce::Colour (t.lightSurface ? (open ? 0x26000000 : 0x1a000000)
                                                  : (open ? 0x90000000 : 0x70000000)));
        g.fillRoundedRectangle (b.translated (0.0f, 2.2f), 5.5f);

        // Smoked dark-glass / frosted soft-white milk-glass surface
        juce::Colour bgCol (open ? t.hudOpenBg : t.hudClosedBg);
        if (hoverHeader && ! open)
            bgCol = t.lightSurface ? juce::Colour (0xfffdfcfa) : bgCol.brighter (0.12f);
        g.setColour (bgCol);
        g.fillRoundedRectangle (b, 5.0f);

        const auto headerRect = b.withHeight (static_cast<float> (kHeaderHeight) - (open ? 0.0f : 2.0f));
        if (open)
        {
            g.setColour (juce::Colour (t.hudHeaderBg));
            g.fillRoundedRectangle (headerRect, 5.0f);
            g.setColour (juce::Colour (t.lightSurface ? 0x24000000 : 0xa0000000));
            g.drawHorizontalLine (kHeaderHeight, b.getX() + 2.0f, b.getRight() - 2.0f);
            g.setColour (accent.withAlpha (t.lightSurface ? 0.22f : 0.30f));
            g.drawHorizontalLine (kHeaderHeight + 1, b.getX() + 2.0f, b.getRight() - 2.0f);
        }

        // Outer neon / anodized border
        const bool lit = open || highlighted;
        if (lit)
        {
            g.setColour (accent.withAlpha (t.lightSurface ? 0.16f : 0.24f));
            g.drawRoundedRectangle (b.expanded (0.5f), 5.4f, 2.6f);
        }
        g.setColour (lit ? accent.withAlpha (t.lightSurface ? 0.78f : 0.90f)
                         : (hoverHeader ? accent.withAlpha (0.58f) : juce::Colour (t.hudBorder)));
        g.drawRoundedRectangle (b, 5.0f, lit ? 1.35f : 1.0f);

        // Left 2x3 machined grip dots
        g.setColour (juce::Colour (t.textMuted).withAlpha (0.80f));
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 2; ++col)
            {
                const float dx = b.getX() + 6.5f + static_cast<float> (col) * 3.5f;
                const float dy = b.getY() + 8.5f + static_cast<float> (row) * 4.2f;
                g.fillEllipse (dx, dy, 1.8f, 1.8f);
            }
        }

        // Glowing vertical LED status bar
        g.setColour (accent.withAlpha (lit ? 0.34f : 0.20f));
        g.fillRoundedRectangle (b.getX() + 14.5f, b.getY() + 5.0f, 5.2f, static_cast<float> (kHeaderHeight) - 10.0f, 2.2f);
        g.setColour (lit ? (t.lightSurface ? accent : accent.brighter (0.2f)) : accent.withAlpha (0.88f));
        g.fillRoundedRectangle (b.getX() + 15.8f, b.getY() + 6.2f, 2.6f, static_cast<float> (kHeaderHeight) - 12.4f, 1.3f);

        // Expand / collapse chevron on right
        const float chevX = b.getRight() - 12.5f;
        const float chevY = static_cast<float> (kHeaderHeight) * 0.5f;
        juce::Path chev;
        if (open)
        {
            chev.startNewSubPath (chevX - 3.6f, chevY - 1.7f);
            chev.lineTo (chevX, chevY + 2.3f);
            chev.lineTo (chevX + 3.6f, chevY - 1.7f);
        }
        else
        {
            chev.startNewSubPath (chevX - 1.7f, chevY - 3.6f);
            chev.lineTo (chevX + 2.3f, chevY);
            chev.lineTo (chevX - 1.7f, chevY + 3.6f);
        }
        g.setColour (lit || hoverHeader ? (t.lightSurface ? accent : accent.brighter (0.35f)) : juce::Colour (t.textPrimary));
        g.strokePath (chev, juce::PathStrokeType (1.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Module title
        g.setColour (juce::Colour (t.textBright));
        const juce::Font titleFont (juce::FontOptions (9.3f, juce::Font::bold));
        g.setFont (titleFont);
        const int titleX = static_cast<int> (b.getX()) + 21;
        juce::GlyphArrangement ga;
        ga.addLineOfText (titleFont, title, 0.0f, 0.0f);
        const int measuredTitleW = static_cast<int> (std::ceil (ga.getBoundingBox (0, -1, true).getWidth())) + 4;
        const int titleW = juce::jlimit (44, 84, measuredTitleW);
        g.drawFittedText (title, titleX, 0, titleW, kHeaderHeight, juce::Justification::centredLeft, 1, 0.85f);

        // Recessed mini-OLED / e-ink live parameter summary badge
        if (summary.isNotEmpty())
        {
            const int sumX = titleX + titleW + 3;
            const int sumW = juce::jmax (26, getWidth() - sumX - 20);
            const juce::Rectangle<float> sumBox (float (sumX), 5.5f, float (sumW), float (kHeaderHeight) - 11.0f);
            g.setColour (juce::Colour (t.oledBg).withAlpha (t.lightSurface ? 0.94f : 0.86f));
            g.fillRoundedRectangle (sumBox, 3.0f);
            g.setColour (lit ? accent.withAlpha (0.45f) : juce::Colour (t.oledBorder).withAlpha (0.82f));
            g.drawRoundedRectangle (sumBox, 3.0f, 0.75f);

            const juce::Colour sumCol = t.lightSurface
                ? (highlighted ? accent.darker (0.20f).interpolatedWith (juce::Colour (t.oledText), 0.35f)
                               : juce::Colour (t.oledText).withAlpha (0.92f))
                : (highlighted ? accent.interpolatedWith (juce::Colour (t.oledText), 0.35f).brighter (0.25f)
                               : juce::Colour (t.oledText).withAlpha (0.90f));
            g.setColour (sumCol);
            g.setFont (juce::FontOptions (8.2f, juce::Font::bold));
            g.drawFittedText (summary, sumBox.toNearestInt().reduced (3, 0), juce::Justification::centredRight, 1, 0.72f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool inHdr = e.y <= kHeaderHeight || ! open;
        if (inHdr != hoverHeader)
        {
            hoverHeader = inHdr;
            setMouseCursor (inHdr ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint (0, 0, getWidth(), kHeaderHeight);
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverHeader)
        {
            hoverHeader = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint (0, 0, getWidth(), kHeaderHeight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        headerGestureActive = (! open) || (e.y <= kHeaderHeight);
        wasDragged = false;
        if (! headerGestureActive)
            return;

        toFront (false);
        if (auto* p = getParentComponent())
            dragStartParent = e.getEventRelativeTo (p).getPosition();
        else
            dragStartParent = e.getScreenPosition();
        startTopLeft = getPosition();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! headerGestureActive)
            return;

        const auto curParent = (getParentComponent() != nullptr)
                                   ? e.getEventRelativeTo (getParentComponent()).getPosition()
                                   : e.getScreenPosition();
        const auto delta = curParent - dragStartParent;
        if (! wasDragged && (std::abs (delta.x) >= 4 || std::abs (delta.y) >= 4))
        {
            wasDragged = true;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        if (wasDragged)
        {
            setTopLeftPosition (startTopLeft + delta);
            clampToParent();
            if (onStateChanged)
                onStateChanged();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! headerGestureActive)
            return;

        setMouseCursor (hoverHeader ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        const bool shouldToggle = ! wasDragged;
        headerGestureActive = false;
        wasDragged = false;

        if (shouldToggle)
            setOpen (! open, true);
    }

    std::function<void()> onStateChanged;

private:
    const Skin* skin = nullptr;
    juce::String title;
    juce::String summary;
    juce::Colour accent;
    juce::Point<int> closedSize;
    juce::Point<int> openSize;
    juce::Component body;
    bool open = false;
    bool highlighted = false;
    bool hoverHeader = false;
    bool headerGestureActive = false;
    bool wasDragged = false;
    juce::Point<int> dragStartParent;
    juce::Point<int> startTopLeft;
};

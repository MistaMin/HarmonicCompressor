// Clicks a linear module slider the way the editor does and checks the value
// lands under the pointer. Before the track-span fix, JUCE kept a 1px span, so
// any click with x >= 1 snapped to the top of the range.
#include <juce_gui_basics/juce_gui_basics.h>
#include "UI/Knob.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
juce::MouseEvent clickAt (juce::Component& c, float x, float y)
{
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> pos (x, y);
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                             pos,
                             juce::ModifierKeys(),
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             &c, &c,
                             now,
                             pos,
                             now,
                             1,
                             false);
}

bool near (double a, double b, double tol)
{
    return std::abs (a - b) <= tol;
}

int gFailures = 0;

void expect (bool ok, const std::string& name, double actual, const std::string& detail)
{
    if (ok)
    {
        std::cout << "PASS  " << name << "  value=" << actual << "\n";
        return;
    }

    ++gFailures;
    std::cout << "FAIL  " << name << "  value=" << actual << "  " << detail << "\n";
}

double valueAtProportion (double proportion, double rangeMin, double rangeMax, double skew)
{
    if (skew != 1.0 && proportion > 0.0)
        proportion = std::exp (std::log (proportion) / skew);
    return rangeMin + (rangeMax - rangeMin) * proportion;
}

// Header is 16px. The painted cap travels inset by the 6px thumb radius.
void checkLinear (const char* name, int width, int height, double rangeMin, double rangeMax, double skew)
{
    Knob slider (name, juce::Colours::white,
                 [] (double v) { return juce::String (v, 1); },
                 false, false);
    slider.setRange (rangeMin, rangeMax, 0.0);
    if (skew != 1.0)
        slider.setSkewFactor (skew);
    slider.setValue (rangeMin, juce::dontSendNotification);
    slider.setBounds (0, 0, width, height);

    const float travelStart = 6.0f;
    const float travelEnd = float (width) - 6.0f;
    const float travelMid = (travelStart + travelEnd) * 0.5f;
    const float trackY = 28.0f;
    const double span = rangeMax - rangeMin;

    slider.mouseDown (clickAt (slider, travelMid, trackY));
    expect (near (slider.getValue(), valueAtProportion (0.5, rangeMin, rangeMax, skew), span * 0.04),
            std::string (name) + " middle of groove",
            slider.getValue(),
            "expected the midpoint, not the maximum");

    slider.setValue (rangeMin, juce::dontSendNotification);
    slider.mouseDown (clickAt (slider, travelStart + 8.0f, trackY));
    const double lowProportion = 8.0 / double (width - 12);
    expect (near (slider.getValue(), valueAtProportion (lowProportion, rangeMin, rangeMax, skew), span * 0.03),
            std::string (name) + " near left of groove",
            slider.getValue(),
            "expected a low value, not the maximum");

    slider.setValue (rangeMin + span * 0.25, juce::dontSendNotification);
    slider.mouseDown (clickAt (slider, travelStart, trackY));
    expect (near (slider.getValue(), rangeMin, span * 0.02),
            std::string (name) + " left end of groove",
            slider.getValue(),
            "expected the minimum");

    slider.mouseDown (clickAt (slider, travelEnd, trackY));
    expect (near (slider.getValue(), rangeMax, span * 0.02),
            std::string (name) + " right end of groove",
            slider.getValue(),
            "expected the maximum only at the right end");
}

void checkRotary ()
{
    Knob rotary ("THRESHOLD", juce::Colours::white,
                 [] (double v) { return juce::String (v, 1); },
                 true, false);
    rotary.setRange (-60.0, 0.0, 0.0);
    rotary.setValue (-18.0, juce::dontSendNotification);
    rotary.setBounds (0, 0, 196, 64);
    rotary.mouseDown (clickAt (rotary, 20.0f, 32.0f));
    expect (near (rotary.getValue(), -18.0, 0.05),
            "rotary click keeps its value",
            rotary.getValue(),
            "a click with no drag must not jump");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    // Compression / harmonics / sidechain / transient module faders.
    checkLinear ("module 122x42", 122, 42, 0.0, 100.0, 1.0);
    // Attack uses a skewed 0.1..300 ms range. A left click must stay near 0.1 ms.
    checkLinear ("attack skew", 122, 42, 0.1, 300.0, 0.3);
    // Input and output gain faders under the meters.
    checkLinear ("gain 84x44", 84, 44, -12.0, 24.0, 1.0);
    checkRotary();

    if (gFailures != 0)
    {
        std::cout << gFailures << " failed\n";
        return 1;
    }

    std::cout << "PASS\n";
    return 0;
}

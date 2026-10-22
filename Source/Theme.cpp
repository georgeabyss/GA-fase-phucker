#include "Theme.h"

namespace fp
{
ga::Theme theme()
{
    ga::Theme t;
    t.type.title = { 25.0f, true, 0.075f };      // the mockup's 34 px (JUCE sizes are the line height)
    t.type.subtitle = { 8.0f, false, 0.37f };    // ... and 11 px
    t.sizes.titleBaseline = 17.0f;
    t.sizes.logoScale = 1.28f;                   // 84 px in the mockup
    t.sizes.sectionBodyCentre = 53.0f;           // the mockup's sections: title line, 18 px, then the controls

    auto echo = std::make_shared<ga::EchoSmear>();
    echo->echoes = { { 10.2f, 0.0f, 0.04f }, { 6.4f, 0.0f, 0.07f }, { 3.5f, 0.0f, 0.11f }, { 1.6f, 0.0f, 0.16f } };
    t.looks.title = echo;
    return t;
}
}

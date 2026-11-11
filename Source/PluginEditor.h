#pragma once
#include "PluginProcessor.h"
#include "Sprites.h"
#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------- палитра
namespace pal
{
inline const juce::Colour ink { 0xff040533 }, night { 0xff08095B }, indigo { 0xff232284 }, violetDk { 0xff362A8E },
    mist { 0xff60579E }, glow { 0xffB7A6F0 }, moon { 0xffEDE4FF }, starBlue { 0xff4F8FD0 },
    crimson { 0xffC10F1D }, blood { 0xffE93426 }, coral { 0xffE8634A }, peach { 0xffEEA380 },
    maroon { 0xff730426 }, deepRed { 0xff530121 },
    paper { 0xff0B0C46 }, track { 0xff1D1F78 }, text { 0xffE9E4FF }, textSoft { 0xffEEA380 },
    frameBlue { 0xff4F72E6 }, frameBlood { 0xffE93426 }, frameViolet { 0xff7F6FE0 }, frameCoral { 0xffE8634A },
    cyan { 0xff7FE7FF }, pink { 0xffFF7FBF }, red { 0xffFF4A5A };
}

namespace pixelfont
{
juce::Font vhs();                     // VT323, 25
juce::Font title (float h = 30.0f);   // Press Start 2P
}

// ---------------------------------------------------------------- помощники рисования
void pixelBox (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour frame, juce::Colour fill, bool shadow = true);

template <size_t N>
void drawSprite (juce::Graphics& g, const char* const (&rows)[N], int x, int y, int scale, juce::Colour c)
{
    g.setColour (c);
    for (size_t j = 0; j < N; ++j)
    {
        const char* row = rows[j];
        int i = 0;
        const int w = (int) std::strlen (row);
        while (i < w)
        {
            if (row[i] != '#') { ++i; continue; }
            int k = i; while (k < w && row[k] == '#') ++k;
            g.fillRect (x + i * scale, y + (int) j * scale, (k - i) * scale, scale);
            i = k;
        }
    }
}

class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PixelLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 0; }
};

class ParamRow : public juce::Component
{
public:
    ParamRow (juce::AudioProcessorValueTreeState&, const juce::String& id, PixelLookAndFeel&);
    ~ParamRow() override { slider.setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
    int page = 0;
private:
    juce::Slider slider;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class PixelButton : public juce::Component
{
public:
    enum class Icon { None, Rec, Stop, Play, Clear };
    PixelButton (const juce::String& t, juce::Colour c, Icon i = Icon::None) : text (t), col (c), icon (i) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; dragFired = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    juce::String text; juce::Colour col; Icon icon;
    bool toggled = false, hasDot = false;
    std::function<void()> onClick, onDragOut;
private:
    bool pressed = false, dragFired = false;
};

class PepelVoxEditor;
class RollView : public juce::Component
{
public:
    explicit RollView (PepelVoxAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    PepelVoxAudioProcessor& proc;
};

class PepelVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PepelVoxEditor (PepelVoxAudioProcessor&);
    ~PepelVoxEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // для инструмента скриншотов
    void debugSetPage (int p) { setPage (p); }
    void debugForceDirt (float d) { forcedDirt = d; }
    void debugAdvance (int frames) { for (int i = 0; i < frames; ++i) tick(); }

private:
    struct Panel { juce::String title; juce::Colour col; juce::Rectangle<int> r; int page; };
    struct Star { int x, y, per, ph; bool big; };
    struct Cloud { float x, y, sp; int size; };
    struct Comet { float x, y, vx, vy; int life; };
    struct Drop { float x, y, sp; };

    void timerCallback() override { tick(); }
    void tick();
    void setPage (int p);
    void addPanel (int page, const juce::String& title, juce::Colour col, int x, int y, std::initializer_list<const char*> ids);
    void buildBackground();
    void drawHeader (juce::Graphics&);
    void drawWeather (juce::Graphics&);
    void drawRecPanel (juce::Graphics&, juce::Rectangle<int> r);
    void say (const juce::String& s) { status = s; statusFrames = 40; }
    void doSave();

    PepelVoxAudioProcessor& proc;
    PixelLookAndFeel laf;
    std::vector<Panel> panels;
    std::vector<std::unique_ptr<ParamRow>> rows;
    juce::OwnedArray<PixelButton> buttons;
    std::unique_ptr<RollView> roll;
    PixelButton *btnRec = nullptr, *btnStop = nullptr, *btnPlay = nullptr, *btnClear = nullptr;
    PixelButton* slotBtn[4] {};
    juce::Rectangle<int> presetRect, tab0, tab1, weatherRect, rollPanel, recPanel;
    juce::Image bg, scan;
    std::unique_ptr<juce::FileChooser> chooser;

    int page = 0, fr = 0, glitchLeft = 0, nextGlitch = 50, glitchY = 0, glitchH = 20, flash = 0, lastGrid = -1;
    float dirtUI = 0, forcedDirt = -1, lvlUI = 0, outUI = 0;
    juce::String status; int statusFrames = 0;
    juce::Random rnd;
    std::vector<Star> stars; std::vector<Cloud> hClouds, wClouds; std::vector<Comet> comets; std::vector<Drop> drops;
    std::vector<juce::Point<int>> bolt;
    std::vector<int> cityBase; std::vector<float> cityDance;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelVoxEditor)
};

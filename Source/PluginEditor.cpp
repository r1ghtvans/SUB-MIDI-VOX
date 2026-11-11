#include "PluginEditor.h"

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

// ================================================================ шрифты
namespace pixelfont
{
static juce::Typeface::Ptr vtFace()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::VT323Regular_ttf, (size_t) BinaryData::VT323Regular_ttfSize);
    return t;
}
static juce::Typeface::Ptr psFace()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::PressStart2PRegular_ttf, (size_t) BinaryData::PressStart2PRegular_ttfSize);
    return t;
}
juce::Font vhs() { return juce::Font (juce::FontOptions (vtFace()).withHeight (25.0f)); }
juce::Font title (float h) { return juce::Font (juce::FontOptions (psFace()).withHeight (h)); }
}

static Colour lerpC (Colour a, Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.0f, 1.0f, t)); }

// ================================================================ плашки
static void fillStep (Graphics& g, Rectangle<int> r, int steps, int s = 3)
{
    for (int i = 0; i < steps; ++i)
    {
        const int inset = (steps - i) * s;
        g.fillRect (r.getX() + inset, r.getY() + i * s, r.getWidth() - 2 * inset, s);
        g.fillRect (r.getX() + inset, r.getBottom() - (i + 1) * s, r.getWidth() - 2 * inset, s);
    }
    g.fillRect (r.getX(), r.getY() + steps * s, r.getWidth(), r.getHeight() - 2 * steps * s);
}

void pixelBox (Graphics& g, Rectangle<int> r, Colour frame, Colour fill, bool shadow)
{
    if (shadow) { g.setColour (pal::ink.withAlpha (0.55f)); fillStep (g, r.translated (4, 4), 2); }
    g.setColour (frame);        fillStep (g, r, 2);
    g.setColour (fill);         fillStep (g, r.reduced (3), 1);
    g.setColour (frame);        fillStep (g, r.reduced (5), 1);
    g.setColour (pal::paper);   fillStep (g, r.reduced (7), 1);
}

// ================================================================ LookAndFeel / строка параметра
PixelLookAndFeel::PixelLookAndFeel()
{
    setColour (juce::Slider::backgroundColourId, pal::track);
}

void PixelLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s)
{
    constexpr int segW = 6, segH = 14, gap = 2;
    const int n = std::max (1, (w + gap) / (segW + gap));
    const float prop = (float) s.valueToProportionOfLength (s.getValue());
    const int filled = juce::jlimit (0, n, (int) std::lround (prop * (float) n));
    const int top = y + (h - segH) / 2;
    for (int i = 0; i < n; ++i)
    {
        const int sx = x + i * (segW + gap);
        if (i < filled)
        {
            const float t = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
            const Colour c = t < 0.5f ? lerpC (pal::blood, pal::frameViolet, t * 2.0f) : lerpC (pal::frameViolet, pal::starBlue, (t - 0.5f) * 2.0f);
            g.setColour (c);                         g.fillRect (sx, top, segW, segH);
            g.setColour (c.brighter (0.45f));        g.fillRect (sx, top, segW, 2);
        }
        else { g.setColour (pal::track); g.fillRect (sx, top, segW, segH); }
    }
    const int mi = std::max (0, filled - 1), mx = x + mi * (segW + gap);
    g.setColour (pal::ink);  g.fillRect (mx - 1, top - 2, segW + 2, segH + 4);
    g.setColour (pal::moon); g.fillRect (mx + 1, top, segW - 2, segH);
}

ParamRow::ParamRow (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, PixelLookAndFeel& laf)
{
    param = apvts.getParameter (id);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setLookAndFeel (&laf);
    slider.setSliderSnapsToMousePosition (true);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    setBufferedToImage (true);
}

void ParamRow::resized() { slider.setBounds (106, 0, getWidth() - 106 - 86, getHeight()); }

void ParamRow::paint (Graphics& g)
{
    const auto name = param->getName (32);
    const auto val = param->getText (param->getValue(), 20);
    g.setFont (pixelfont::vhs());
    g.setColour (pal::ink);       g.drawText (name, 1, 1, 100, getHeight(), juce::Justification::centredLeft, false);
    g.setColour (pal::text);      g.drawText (name, 0, 0, 100, getHeight(), juce::Justification::centredLeft, false);
    const auto vr = Rectangle<int> (getWidth() - 80, 0, 80, getHeight());
    g.setColour (pal::ink);       g.drawText (val, vr.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::textSoft);  g.drawText (val, vr, juce::Justification::centredRight, false);
}

// ================================================================ кнопка
void PixelButton::mouseUp (const juce::MouseEvent& e)
{
    const bool fire = pressed && ! dragFired && contains (e.getPosition());
    pressed = false; repaint();
    if (fire && onClick) onClick();
}
void PixelButton::mouseDrag (const juce::MouseEvent& e)
{
    if (onDragOut && ! dragFired && e.getDistanceFromDragStart() > 6)
    {
        dragFired = true; pressed = false; repaint();
        onDragOut();
    }
}

void PixelButton::paint (Graphics& g)
{
    static const char* const REC[] = { ".#####.", "#######", "#######", "#######", "#######", "#######", ".#####." };
    static const char* const STOP[] = { "#######", "#######", "#######", "#######", "#######", "#######", "#######" };
    static const char* const PLAY[] = { "#......", "###....", "#####..", "#######", "#####..", "###....", "#......" };
    static const char* const CLR[] = { "##...##", ".##.##.", "..###..", "...#...", "..###..", ".##.##.", "##...##" };

    auto r = getLocalBounds().withTrimmedRight (4).withTrimmedBottom (4);
    g.setColour (pal::ink.withAlpha (0.55f)); fillStep (g, getLocalBounds().withTrimmedLeft (4).withTrimmedTop (4), 1);
    if (pressed) r.translate (2, 2);
    g.setColour (col);
    fillStep (g, r, 1);
    g.setColour (toggled ? col.darker (0.25f) : pal::paper);
    fillStep (g, r.reduced (3), 1);
    const Colour fg = toggled ? pal::moon : pal::text;
    if (icon != Icon::None)
    {
        const int ix = r.getCentreX() - 10, iy = r.getY() + 8;
        const Colour ic = icon == Icon::Rec ? (toggled ? pal::moon : pal::blood) : fg;
        if (icon == Icon::Rec) drawSprite (g, REC, ix, iy, 3, ic);
        else if (icon == Icon::Stop) drawSprite (g, STOP, ix, iy, 3, ic);
        else if (icon == Icon::Play) drawSprite (g, PLAY, ix, iy, 3, ic);
        else drawSprite (g, CLR, ix, iy, 3, ic);
        g.setFont (pixelfont::title (8.0f)); g.setColour (fg);
        g.drawText (text, r.withTrimmedTop (r.getHeight() - 16), juce::Justification::centred, false);
    }
    else
    {
        g.setFont (pixelfont::title (10.0f)); g.setColour (fg);
        g.drawText (text, r, juce::Justification::centred, false);
    }
    if (hasDot) { g.setColour (pal::moon); g.fillRect (r.getRight() - 6, r.getY() + 2, 3, 3); }
}

// ================================================================ piano roll
void RollView::paint (Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (pal::ink); g.fillRect (b);
    g.setColour (Colour (0xff070842)); g.fillRect (b.reduced (2));

    float len = 4.0f;
    const auto notes = proc.getDisplayNotes (len);
    int lo = 48, hi = 72;
    if (! notes.empty())
    {
        lo = 127; hi = 0;
        for (auto& n : notes) { lo = std::min (lo, n.note); hi = std::max (hi, n.note); }
        lo -= 2; hi += 2;
        if (hi - lo < 14) { const int c = (hi + lo) / 2; lo = c - 7; hi = c + 7; }
    }
    const auto in = b.reduced (3);
    const float rowH = (float) in.getHeight() / (float) (hi - lo + 1);

    // горизонтальные линии октав и чёрные клавиши
    for (int nn = lo; nn <= hi; ++nn)
    {
        const int y = in.getBottom() - (int) ((float) (nn - lo + 1) * rowH);
        const int pc = nn % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        if (black) { g.setColour (pal::ink.withAlpha (0.35f)); g.fillRect (in.getX(), y, in.getWidth(), std::max (1, (int) rowH)); }
        if (pc == 0) { g.setColour (pal::mist.withAlpha (0.55f)); g.fillRect (in.getX(), y + (int) rowH - 1, in.getWidth(), 1); }
    }
    // сетка по долям
    const int beats = (int) std::ceil (len);
    for (int bt = 0; bt <= beats; ++bt)
    {
        const int x = in.getX() + (int) ((float) bt / len * (float) in.getWidth());
        g.setColour (bt % 4 == 0 ? pal::frameViolet.withAlpha (0.8f) : pal::indigo.withAlpha (0.9f));
        g.fillRect (x, in.getY(), bt % 4 == 0 ? 2 : 1, in.getHeight());
    }
    for (auto& n : notes)
    {
        const int x = in.getX() + (int) (n.start / len * (float) in.getWidth());
        const int w = std::max (4, (int) (n.len / len * (float) in.getWidth()) - 1);
        const int y = in.getBottom() - (int) ((float) (n.note - lo + 1) * rowH);
        const int h = std::max (4, (int) rowH - 1);
        const Colour c = lerpC (pal::frameViolet, pal::coral, (float) n.vel / 127.0f);
        g.setColour (pal::ink);  g.fillRect (x - 1, y - 1, w + 2, h + 2);
        g.setColour (c);         g.fillRect (x, y, w, h);
        g.setColour (c.brighter (0.5f)); g.fillRect (x, y, w, std::min (2, h));
    }
    // плейхед
    const bool rec = proc.recording.load(), play = proc.playing.load();
    if (rec || play)
    {
        const double pos = rec ? proc.recBeats.load() : proc.playBeats.load();
        const int x = in.getX() + (int) (pos / len * in.getWidth());
        g.setColour (pal::ink);  g.fillRect (x - 1, in.getY(), 5, in.getHeight());
        g.setColour (rec ? pal::blood : pal::moon); g.fillRect (x, in.getY(), 3, in.getHeight());
    }
    if (notes.empty() && ! rec)
    {
        g.setFont (pixelfont::title (11.0f)); g.setColour (pal::glow.withAlpha (0.8f));
        g.drawText (juce::String::fromUTF8 ("ЖМИ REC И ПОЙ В МИКРОФОН"), b, juce::Justification::centred, false);
    }
}

// ================================================================ редактор
static const char* kOpen = "";

PepelVoxEditor::PepelVoxEditor (PepelVoxAudioProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    (void) kOpen;
    setLookAndFeel (&laf);
    const int cx[3] = { 20, 410, 800 };
    auto& A = pal::frameBlue; auto& B = pal::frameBlood; auto& V = pal::frameViolet; auto& C = pal::frameCoral;
    namespace id = pepel::ids;

    // страница 0: голосовые эффекты
    addPanel (0, "AUTO-TUNE", A, cx[0], 132, { id::atKey, id::atScale, id::atAmount, id::atSpeed, id::atDetune });
    addPanel (0, "GRANULAR", B, cx[0], 344, { id::grSize, id::grDens, id::grScat, id::grMix });
    addPanel (0, "BITCRUSHER", V, cx[0], 526, { id::bcBits, id::bcRate, id::bcMix });
    addPanel (0, "PITCH + HARMONY", B, cx[1], 132, { id::pitShift, id::pitHarm, id::pitInt1, id::pitInt2, id::pitVoc });
    addPanel (0, "PHASER", V, cx[1], 344, { id::phRate, id::phDepth, id::phMix });
    addPanel (0, "FLANGER", C, cx[1], 526, { id::flRate, id::flDepth, id::flMix });
    addPanel (0, "MASTER", V, cx[2], 132, { id::inGain, id::outGain, id::dryWet });
    addPanel (0, "DISTORTION", C, cx[2], 284, { id::diDrive, id::diTone, id::diMix });
    addPanel (0, "EXCITER", A, cx[2], 436, { id::exAmt, id::exFreq });
    addPanel (0, "TREMOLO", B, cx[2], 558, { id::trRate, id::trDepth });

    // страница 1: SubVox + MIDI
    addPanel (1, "SUBVOX", B, cx[0], 132, { id::sbMode, id::sbOct, id::sbLevel, id::sbVoice, id::sbLock, id::sbGlide, id::sbDrive, id::sbPunch });
    addPanel (1, "SUB FILTER", A, cx[1], 132, { id::sbCut, id::sbReso, id::sbFollow, id::sbGate });
    addPanel (1, "WOBBLE", V, cx[1], 314, { id::sbWobRate, id::sbWobDepth, id::sbDetune });
    addPanel (1, "MIDI SETUP", C, cx[2], 132, { id::mdGrid, id::mdBpm, id::mdTransp, id::mdMinLen, id::mdOut });
    recPanel = { cx[2], 344, 370, 138 };
    rollPanel = { 20, 498, 1150, 214 };
    panels.push_back ({ "MIDI REC", B, recPanel, 1 });
    panels.push_back ({ "MIDI ROLL", A, rollPanel, 1 });

    auto mk = [this] (const juce::String& t, Colour c, PixelButton::Icon ic, Rectangle<int> r, std::function<void()> f)
    {
        auto* b = buttons.add (new PixelButton (t, c, ic));
        b->setBounds (r); b->onClick = std::move (f); addAndMakeVisible (b); return b;
    };

    // REC-панель
    const int by = recPanel.getY() + 42;
    btnRec = mk ("REC", pal::frameBlood, PixelButton::Icon::Rec, { 814, by, 78, 48 }, [this] { proc.recording.load() ? proc.stopRecording() : proc.startRecording(); });
    btnStop = mk ("STOP", pal::frameViolet, PixelButton::Icon::Stop, { 902, by, 78, 48 }, [this] { proc.stopRecording(); proc.stopPlayback(); });
    btnPlay = mk ("PLAY", pal::frameBlue, PixelButton::Icon::Play, { 990, by, 78, 48 }, [this] { proc.playing.load() ? proc.stopPlayback() : proc.startPlayback(); });
    btnClear = mk ("CLEAR", pal::frameCoral, PixelButton::Icon::Clear, { 1078, by, 78, 48 }, [this] { proc.clearCurrent(); });

    // ROLL-панель: слоты и действия
    const int ry = rollPanel.getY() + 9;
    for (int i = 0; i < 4; ++i)
        slotBtn[i] = mk (juce::String::charToString ((juce::juce_wchar) ('A' + i)), pal::frameViolet, PixelButton::Icon::None, { 270 + i * 40, ry, 34, 24 },
                         [this, i] { proc.currentSlot.store (i); proc.rebuildPlay(); say (juce::String ("SLOT ") + (char) ('A' + i)); });
    mk ("COPY", pal::frameBlue, PixelButton::Icon::None, { 750, ry, 70, 24 }, [this]
    {
        const auto c = proc.getClip (proc.currentSlot.load());
        if (c.empty()) { say ("EMPTY"); return; }
        juce::SystemClipboard::copyTextToClipboard (juce::String (pepel::clipToText (c)));
        say ("COPIED");
    });
    mk ("PASTE", pal::frameBlue, PixelButton::Icon::None, { 828, ry, 76, 24 }, [this]
    {
        pepel::Clip c;
        if (pepel::clipFromText (juce::SystemClipboard::getTextFromClipboard().toStdString(), c)) { proc.setClip (proc.currentSlot.load(), c); say ("PASTED"); }
        else say ("NO MIDI IN CLIPBOARD");
    });
    mk ("SAVE .MID", pal::frameCoral, PixelButton::Icon::None, { 912, ry, 96, 24 }, [this] { doSave(); });
    auto* drag = mk ("DRAG MIDI", pal::frameBlood, PixelButton::Icon::None, { 1016, ry, 140, 24 }, nullptr);
    drag->onClick = [this] { say ("DRAG ME TO FL"); };
    drag->onDragOut = [this]
    {
        const int slot = proc.currentSlot.load();
        const auto f = proc.tempMidiFile (slot);
        if (proc.writeMidiFile (slot, f)) juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        else say ("RECORD FIRST");
    };

    roll = std::make_unique<RollView> (proc);
    roll->setBounds (rollPanel.getX() + 12, rollPanel.getY() + 42, rollPanel.getWidth() - 24, rollPanel.getHeight() - 52);
    addAndMakeVisible (*roll);

    // фиксированные области
    presetRect = { 22, 70, 300, 34 };
    tab0 = { 900, 26, 126, 30 };
    tab1 = { 1036, 26, 126, 30 };
    weatherRect = { 20, 726, 1150, 96 };

    // анимация
    for (int i = 0; i < 46; ++i) stars.push_back ({ rnd.nextInt (1190), 4 + rnd.nextInt (100), 3 + rnd.nextInt(6), rnd.nextInt (40), i % 6 == 0 });
    for (int i = 0; i < 7; ++i) hClouds.push_back ({ (float) rnd.nextInt (1190), (float) (36 + rnd.nextInt (50)), 0.12f + rnd.nextFloat() * 0.3f, i % 3 });
    for (int i = 0; i < 7; ++i) wClouds.push_back ({ (float) (rnd.nextInt (1130)), (float) (4 + rnd.nextInt (28)), 0.2f + rnd.nextFloat() * 0.5f, i % 3 });
    const int cols = 1134 / 14;
    for (int i = 0; i < cols; ++i) { cityBase.push_back (6 + rnd.nextInt (20)); cityDance.push_back (0.0f); }

    setSize (1190, 864);
    setResizable (false, false);
    buildBackground();
    setPage (0);
    startTimerHz (15);
}

PepelVoxEditor::~PepelVoxEditor() { stopTimer(); setLookAndFeel (nullptr); }

void PepelVoxEditor::addPanel (int pg, const juce::String& title, Colour col, int x, int y, std::initializer_list<const char*> ids)
{
    const int n = (int) ids.size();
    panels.push_back ({ title, col, { x, y, 370, 48 + 30 * n }, pg });
    int i = 0;
    for (auto* idp : ids)
    {
        auto row = std::make_unique<ParamRow> (proc.apvts, idp, laf);
        row->page = pg;
        row->setBounds (x + 14, y + 42 + 30 * i++, 342, 30);
        addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }
}

void PepelVoxEditor::setPage (int p)
{
    page = p;
    for (auto& r : rows) r->setVisible (r->page == page);
    for (auto* b : buttons) b->setVisible (page == 1);
    roll->setVisible (page == 1);
    repaint();
}

void PepelVoxEditor::doSave()
{
    const int slot = proc.currentSlot.load();
    if (proc.getClip (slot).empty()) { say ("RECORD FIRST"); return; }
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PEPEL_VOX.mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("mid")) f = f.withFileExtension ("mid");
                              say (proc.writeMidiFile (slot, f) ? "SAVED" : "ERROR");
                          });
}

void PepelVoxEditor::buildBackground()
{
    const int w = getWidth(), h = getHeight();
    bg = juce::Image (juce::Image::RGB, w, h, false);
    Graphics g (bg);
    const Colour s0 = pal::ink, s1 (0xff1E2181), s2 = pal::maroon, s3 = pal::crimson;
    for (int y = 0; y < h; y += 6)
    {
        float t = std::floor ((float) (y + 3) / (float) h * 14.0f) / 13.0f; t = juce::jlimit (0.0f, 1.0f, t);
        Colour c = t < 0.33f ? lerpC (s0, s1, t / 0.33f) : (t < 0.66f ? lerpC (s1, s2, (t - 0.33f) / 0.33f) : lerpC (s2, s3, (t - 0.66f) / 0.34f));
        g.setColour (c); g.fillRect (0, y, w, 6);
    }
    scan = juce::Image (juce::Image::ARGB, w, h, true);
    Graphics sg (scan);
    sg.setColour (pal::ink.withAlpha (0.16f));
    for (int y = 0; y < h; y += 3) sg.fillRect (0, y, w, 1);
}

// ---------------------------------------------------------------- анимация
void PepelVoxEditor::tick()
{
    ++fr;
    const float W = (float) getWidth();
    const float d = forcedDirt >= 0.0f ? forcedDirt : proc.dirt.load();
    dirtUI += (d - dirtUI) * 0.1f;
    lvlUI += (proc.inputLevel.load() - lvlUI) * 0.5f;
    for (auto& c : hClouds) { c.x += c.sp; if (c.x > W + 130) c.x = -130; }
    if (rnd.nextInt (90) == 0 && comets.size() < 2)
        comets.push_back ({ W * 0.35f + rnd.nextFloat() * W * 0.65f, -4.0f, -(5.0f + rnd.nextFloat() * 2.0f), 2.6f + rnd.nextFloat(), 40 });
    for (auto& c : comets) { c.x += c.vx; c.y += c.vy; --c.life; }
    comets.erase (std::remove_if (comets.begin(), comets.end(), [] (const Comet& c) { return c.life <= 0 || c.y > 110; }), comets.end());

    if (glitchLeft > 0) --glitchLeft;
    else if (--nextGlitch <= 0) { glitchLeft = 2 + rnd.nextInt (3); nextGlitch = 45 + rnd.nextInt (30); glitchY = rnd.nextInt (getHeight() - 40); glitchH = 10 + rnd.nextInt (20); }

    // погода
    for (auto& c : wClouds) { c.x += c.sp; if (c.x > 1134) c.x = -130; }
    const int wantDrops = dirtUI >= 0.5f ? std::min (110, (int) ((dirtUI - 0.5f) * 2.0f * 120.0f)) : 0;
    while ((int) drops.size() < wantDrops) drops.push_back ({ rnd.nextFloat() * 1134.0f, rnd.nextFloat() * 70.0f, 7.0f + rnd.nextFloat() * 5.0f });
    while ((int) drops.size() > wantDrops) drops.pop_back();
    for (auto& dr : drops) { dr.y += dr.sp; dr.x -= 2.0f; if (dr.y > 76) { dr.y = -6; dr.x = rnd.nextFloat() * 1134.0f; } if (dr.x < 0) dr.x += 1134.0f; }
    if (dirtUI >= 0.85f)
    {
        if (flash > 0) --flash;
        else if (rnd.nextInt (12) == 0)
        {
            flash = 2; bolt.clear();
            int x = 80 + rnd.nextInt (980), y = 0;
            while (y < 74) { bolt.push_back ({ x, y }); x += rnd.nextInt (19) - 9; y += 9; }
            bolt.push_back ({ x, 76 });
        }
    }
    else { flash = 0; bolt.clear(); }
    for (size_t i = 0; i < cityDance.size(); ++i)
    {
        const float tgt = lvlUI * 70.0f * (0.5f + 0.5f * std::sin ((float) fr * 0.45f + (float) i * 0.7f)) * (0.6f + 0.4f * std::sin ((float) i * 1.9f));
        cityDance[i] += (tgt - cityDance[i]) * 0.6f;
    }

    // MIDI
    if (proc.takeReady.load()) { proc.commitTake(); say ("TAKE SAVED"); }
    if (proc.grid() != lastGrid) { lastGrid = proc.grid(); proc.rebuildPlay(); }
    if (btnRec) { btnRec->toggled = proc.recording.load(); btnPlay->toggled = proc.playing.load(); }
    for (int i = 0; i < 4; ++i)
        if (slotBtn[i]) { slotBtn[i]->toggled = proc.currentSlot.load() == i; slotBtn[i]->hasDot = ! proc.getClip (i).empty(); }
    if (statusFrames > 0) --statusFrames;
    if (page == 1) for (auto* b : buttons) b->repaint();
    repaint();
}

void PepelVoxEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (tab0.contains (p)) setPage (0);
    else if (tab1.contains (p)) setPage (1);
    else if (presetRect.contains (p))
    {
        const int n = proc.getNumPrograms(), cur = proc.getCurrentProgram();
        proc.setCurrentProgram (p.x < presetRect.getX() + 40 ? (cur + n - 1) % n : (cur + 1) % n);
        repaint();
    }
}

// ---------------------------------------------------------------- рисование
void PepelVoxEditor::drawHeader (Graphics& g)
{
    const int W = getWidth();
    for (int y = 0; y < 112; y += 6)
    {
        const float t = std::floor ((float) y / 112.0f * 14.0f) / 13.0f;
        g.setColour (lerpC (Colour (0xff00003E), Colour (0xff2A2490), t));
        g.fillRect (0, y, W, 6);
    }
    g.setColour (pal::blood); g.fillRect (0, 112, W, 3);
    g.setColour (pal::ink);   g.fillRect (0, 115, W, 3);

    for (auto& s : stars)
    {
        if (((fr + s.ph) % (s.per * 3)) >= s.per * 2 + 1) continue;
        if (s.big) drawSprite (g, sprites::SPARKLE, s.x, s.y, 1, pal::moon);
        else { g.setColour ((s.x & 1) ? pal::starBlue : pal::glow); g.fillRect (s.x, s.y, 2, 2); }
    }
    drawSprite (g, sprites::MOON, 773, 29, 4, pal::ink.withAlpha (0.6f));
    drawSprite (g, sprites::MOON, 770, 26, 4, pal::moon);
    for (auto& c : hClouds)
    {
        const Colour col = c.size == 0 ? pal::mist : (c.size == 1 ? pal::violetDk : pal::indigo);
        const int cx = (int) c.x, cy = (int) c.y;
        if (c.size == 0) drawSprite (g, sprites::CLOUD_S, cx, cy, 3, col);
        else if (c.size == 1) drawSprite (g, sprites::CLOUD_M, cx, cy, 3, col);
        else drawSprite (g, sprites::CLOUD_L, cx, cy, 3, col);
    }
    for (auto& c : comets)
        for (int k = 0; k < 10; ++k)
        {
            g.setColour (pal::moon.withAlpha (1.0f - (float) k / 10.0f));
            const int sz = k == 0 ? 4 : 3;
            g.fillRect ((int) (c.x - c.vx * 0.5f * (float) k), (int) (c.y - c.vy * 0.5f * (float) k), sz, sz);
        }

    // иероглифы
    const int kx = (W - 108) / 2, ky = 30;
    drawSprite (g, sprites::KANJI_HAI, kx + 3, ky + 3, 3, pal::ink.withAlpha (0.75f));
    drawSprite (g, sprites::KANJI_AI, kx + 60 + 3, ky + 3, 3, pal::ink.withAlpha (0.75f));
    drawSprite (g, sprites::KANJI_HAI, kx, ky, 3, pal::moon);
    drawSprite (g, sprites::KANJI_AI, kx + 60, ky, 3, pal::blood);

    // заголовок с хроматической аберрацией: «SUB-MIDI vox» + «by ПЕПЕL»
    g.setFont (pixelfont::title (28.0f));
    const int off = glitchLeft > 0 ? 4 : 2;
    const juce::String ttl ("SUB-MIDI vox");
    const Rectangle<int> tr (22, 22, 340, 40);
    g.setColour (pal::ink.withAlpha (0.8f)); g.drawText (ttl, tr.translated (3, 3), juce::Justification::centredLeft, false);
    g.setColour (pal::cyan.withAlpha (0.85f)); g.drawText (ttl, tr.translated (-off, 0), juce::Justification::centredLeft, false);
    g.setColour (pal::red.withAlpha (0.85f));  g.drawText (ttl, tr.translated (off, 0), juce::Justification::centredLeft, false);
    g.setColour (pal::moon);                   g.drawText (ttl, tr, juce::Justification::centredLeft, false);
    g.setFont (pixelfont::title (13.0f));
    const auto by = juce::String::fromUTF8 ("by ПЕПЕL");
    const Rectangle<int> br (372, 42, 120, 20);
    g.setColour (pal::ink.withAlpha (0.8f)); g.drawText (by, br.translated (1, 1), juce::Justification::centredLeft, false);
    g.setColour (pal::blood); g.drawText (by, br, juce::Justification::centredLeft, false);

    // выбор пресета
    pixelBox (g, presetRect, pal::frameViolet, pal::paper);
    g.setFont (pixelfont::title (13.0f));
    g.setColour (pal::blood); g.drawText ("<", presetRect.withTrimmedLeft (14), juce::Justification::centredLeft, false);
    g.drawText (">", presetRect.withTrimmedRight (14), juce::Justification::centredRight, false);
    const int cur = proc.getCurrentProgram();
    g.setColour (pal::text);
    g.drawText (juce::String (cur + 1).paddedLeft ('0', 2) + " " + proc.getProgramName (cur), presetRect.reduced (30, 0), juce::Justification::centred, false);

    // вкладки
    auto tab = [&] (Rectangle<int> r, const char* t, bool on)
    {
        pixelBox (g, r, on ? pal::frameCoral : pal::frameBlue, on ? pal::crimson : pal::paper);
        g.setFont (pixelfont::title (11.0f)); g.setColour (on ? pal::moon : pal::glow);
        g.drawText (t, r, juce::Justification::centred, false);
    };
    tab (tab0, "VOICE FX", page == 0);
    tab (tab1, "SUBVOX", page == 1);
}

static Colour keyColour (const Colour* k, float d)
{
    static const float pos[5] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.85f };
    if (d <= pos[0]) return k[0];
    for (int i = 1; i < 5; ++i) if (d <= pos[i]) return lerpC (k[i - 1], k[i], (d - pos[i - 1]) / (pos[i] - pos[i - 1]));
    return k[4];
}

static void pixelCircle (Graphics& g, int cx, int cy, int r, Colour c, int step = 3)
{
    g.setColour (c);
    for (int dy = -r; dy < r; dy += step)
    {
        const float yy = (float) std::abs (dy + step / 2);
        const int hw = (int) (std::sqrt (std::max (0.0f, (float) (r * r) - yy * yy)) / (float) step) * step;
        g.fillRect (cx - hw, cy + dy, hw * 2, step);
    }
}

void PepelVoxEditor::drawWeather (Graphics& g)
{
    pixelBox (g, weatherRect, pal::frameBlue, pal::paper);
    const auto in = weatherRect.reduced (8);
    const float d = juce::jlimit (0.0f, 1.0f, dirtUI);
    g.saveState();
    g.reduceClipRegion (in);

    static const Colour top[5] = { Colour (0xff8FC8F0), Colour (0xff3A5FB0), Colour (0xff5A3F9A), Colour (0xff3A0A32), Colour (0xff5A0612) };
    static const Colour hor[5] = { Colour (0xffD8F0FF), Colour (0xffF08A50), Colour (0xffE8634A), Colour (0xff8A1030), Colour (0xffC10F1D) };
    const Colour ct = keyColour (top, d), ch = keyColour (hor, d);
    for (int y = 0; y < in.getHeight(); y += 4)
    {
        const float t = std::floor ((float) y / (float) in.getHeight() * 8.0f) / 7.0f;
        g.setColour (lerpC (ct, ch, t));
        g.fillRect (in.getX(), in.getY() + y, in.getWidth(), 4);
    }
    const int horizon = in.getBottom() - 14;

    // солнце
    const float sunA = juce::jlimit (0.0f, 1.0f, 1.0f - (d - 0.35f) / 0.25f);
    if (sunA > 0.0f)
    {
        const float k = juce::jlimit (0.0f, 1.0f, d / 0.42f);
        const int sy = (int) ((float) (in.getY() + 24) + k * (float) (horizon - in.getY() - 26));
        pixelCircle (g, in.getX() + 260, sy, 15, lerpC (Colour (0xffFFF6C8), Colour (0xffFF9A50), d / 0.4f).withAlpha (sunA));
    }
    // луна
    const float moonA = juce::jlimit (0.0f, 1.0f, (d - 0.5f) / 0.25f);
    if (moonA > 0.0f)
        pixelCircle (g, in.getRight() - 230, in.getY() + 28, 21, lerpC (pal::moon, pal::blood, (d - 0.5f) / 0.35f).withAlpha (moonA));

    // облака
    const Colour cc = d < 0.4f ? lerpC (Colour (0xffEAF4FF), pal::frameViolet, d / 0.4f) : lerpC (pal::frameViolet, Colour (0xff8A1030), (d - 0.4f) / 0.45f);
    for (auto& c : wClouds)
    {
        const int cx = in.getX() + (int) c.x, cy = in.getY() + (int) c.y;
        if (c.size == 0) drawSprite (g, sprites::CLOUD_S, cx, cy, 3, cc);
        else if (c.size == 1) drawSprite (g, sprites::CLOUD_M, cx, cy, 3, cc);
        else drawSprite (g, sprites::CLOUD_L, cx, cy, 3, cc);
    }
    // дождь
    g.setColour (Colour (0xffA8C0FF).withAlpha (0.65f));
    for (auto& dr : drops)
    {
        const int x = in.getX() + (int) dr.x, y = in.getY() + (int) dr.y;
        g.fillRect (x, y, 2, 4); g.fillRect (x - 1, y + 4, 2, 3);
    }
    // город-эквалайзер
    g.setColour (pal::ink); g.fillRect (in.getX(), horizon + 4, in.getWidth(), in.getBottom() - horizon);
    for (size_t i = 0; i < cityBase.size(); ++i)
    {
        const int h = cityBase[i] + (int) cityDance[i], x = in.getX() + (int) i * 14;
        const int top2 = horizon + 4 - h;
        g.setColour (pal::ink);       g.fillRect (x, top2, 12, h);
        g.setColour (pal::violetDk);  g.fillRect (x, top2, 12, 3);
        if (((int) i + fr / 8) % 5 == 0) { g.setColour (pal::peach); g.fillRect (x + 4, top2 + 8, 2, 2); }
    }
    // молния
    if (flash > 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.32f)); g.fillRect (in);
        for (size_t i = 0; i + 1 < bolt.size(); ++i)
            for (int j = 0; j < 9; ++j)
            {
                const int x = bolt[i].x + (bolt[i + 1].x - bolt[i].x) * j / 9, y = bolt[i].y + j;
                g.setColour (pal::ink); g.fillRect (in.getX() + x - 1, in.getY() + y, 6, 1);
                g.setColour (Colour (0xffFFFBD0)); g.fillRect (in.getX() + x, in.getY() + y, 4, 1);
            }
    }
    g.restoreState();
}

void PepelVoxEditor::drawRecPanel (Graphics& g, Rectangle<int> r)
{
    const auto line = Rectangle<int> (r.getX() + 14, r.getY() + 42 + 54, 342, 30);
    g.setFont (pixelfont::vhs());
    const int ln = proc.liveNote.load();
    juce::String note = "---";
    if (ln >= 0) note = juce::String (pepel::noteName (ln)) + juce::String (ln / 12 - 1);
    g.setColour (pal::ink); g.drawText ("NOTE " + note, line.translated (1, 1).withWidth (120), juce::Justification::centredLeft, false);
    g.setColour (pal::text); g.drawText ("NOTE", line.withWidth (60), juce::Justification::centredLeft, false);
    g.setColour (pal::textSoft); g.drawText (note, line.withX (line.getX() + 60).withWidth (60), juce::Justification::centredLeft, false);

    const int lv = (int) std::lround (juce::jlimit (0.0f, 1.0f, std::sqrt (lvlUI) * 1.2f) * 10.0f);
    for (int i = 0; i < 10; ++i)
    {
        g.setColour (i < lv ? (i > 7 ? pal::blood : (i > 5 ? pal::coral : pal::frameViolet)) : pal::track);
        g.fillRect (line.getX() + 124 + i * 8, line.getY() + 8, 6, 14);
    }
    juce::String right;
    if (statusFrames > 0) right = status;
    else if (proc.recording.load()) right = juce::String::formatted ("REC %d:%02d", (int) (proc.recBeats.load() * 60.0 / proc.currentBpm()) / 60, (int) (proc.recBeats.load() * 60.0 / proc.currentBpm()) % 60);
    else if (proc.playing.load()) right = "PLAY";
    else right = juce::String ((int) proc.getClip (proc.currentSlot.load()).notes.size()) + " NOTES";
    const bool blink = proc.recording.load() && statusFrames == 0 && (fr / 4) % 2 == 0;
    g.setColour (pal::ink); g.drawText (right, line.withTrimmedLeft (212).translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (blink ? pal::blood : pal::textSoft); g.drawText (right, line.withTrimmedLeft (212), juce::Justification::centredRight, false);
}

void PepelVoxEditor::paint (Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    drawHeader (g);
    for (auto& p : panels)
    {
        if (p.page != page) continue;
        pixelBox (g, p.r, p.col, pal::paper);
        g.setColour (p.col.withAlpha (0.45f));
        g.fillRect (p.r.getX() + 7, p.r.getY() + 7, p.r.getWidth() - 14, 29);
        drawSprite (g, sprites::HEART, p.r.getX() + 16, p.r.getY() + 14, 3, pal::ink.withAlpha (0.7f));
        drawSprite (g, sprites::HEART, p.r.getX() + 15, p.r.getY() + 13, 3, pal::blood);
        g.setFont (pixelfont::title (13.0f));
        g.setColour (pal::ink); g.drawText (p.title, p.r.getX() + 45, p.r.getY() + 8, 300, 28, juce::Justification::centredLeft, false);
        g.setColour (pal::text); g.drawText (p.title, p.r.getX() + 44, p.r.getY() + 7, 300, 28, juce::Justification::centredLeft, false);
    }
    if (page == 1) drawRecPanel (g, recPanel);
    drawWeather (g);

    g.setFont (pixelfont::title (10.0f));
    const auto sig = Rectangle<int> (getWidth() - 220, getHeight() - 26, 200, 16);
    const auto txt = juce::String::fromUTF8 ("ПЕПЕЛ");
    g.setColour (pal::ink.withAlpha (0.8f)); g.drawText (txt, sig.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::text.withAlpha (0.85f)); g.drawText (txt, sig, juce::Justification::centredRight, false);
}

void PepelVoxEditor::paintOverChildren (Graphics& g)
{
    g.drawImageAt (scan, 0, 0);
    for (int i = 0; i < 24; ++i)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : pal::ink).withAlpha (0.12f + rnd.nextFloat() * 0.2f));
        g.fillRect (rnd.nextInt (getWidth()), rnd.nextInt (getHeight()), 2, 1);
    }
    if (glitchLeft > 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.1f)); g.fillRect (0, glitchY, getWidth(), glitchH);
        for (int i = 0; i < 4; ++i)
        {
            g.setColour ((i % 2 ? pal::cyan : pal::pink).withAlpha (0.4f));
            g.fillRect (rnd.nextInt (getWidth() - 300), glitchY + rnd.nextInt (glitchH), 120 + rnd.nextInt (300), 2);
        }
    }
}

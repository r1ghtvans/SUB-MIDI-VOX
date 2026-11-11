// PepelScreenshot: рисует редактор в PNG без показа окна (для xvfb-run).
#include "PluginEditor.h"
#include <juce_events/juce_events.h>

static void shot (PepelVoxAudioProcessor& proc, int page, float dirt, int preset, const juce::File& out, bool withNotes)
{
    proc.setCurrentProgram (preset);
    if (withNotes)
    {
        pepel::Clip c; c.lengthBeats = 8;
        const int notes[] = { 33, 33, 36, 38, 31, 31, 36, 40, 29, 33, 36, 41 };
        const float starts[] = { 0, 0.75f, 1.5f, 2.5f, 3.5f, 4.25f, 5, 5.5f, 6, 6.5f, 7, 7.5f };
        for (int i = 0; i < 12; ++i) c.notes.push_back ({ starts[i], 0.5f, notes[i], 70 + (i * 11) % 55 });
        proc.setClip (0, c);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* e = dynamic_cast<PepelVoxEditor*> (ed.get());
    e->debugSetPage (page);
    e->debugForceDirt (dirt);
    e->debugAdvance (60);
    auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? argv[1] : "shots");
    dir.createDirectory();
    PepelVoxAudioProcessor proc;
    shot (proc, 0, 0.0f, 0, dir.getChildFile ("1_clear.png"), false);
    shot (proc, 0, 0.42f, 3, dir.getChildFile ("2_sunset.png"), false);
    shot (proc, 0, 1.0f, 6, dir.getChildFile ("3_storm.png"), false);
    shot (proc, 1, 0.2f, 9, dir.getChildFile ("4_subvox.png"), true);
    return 0;
}

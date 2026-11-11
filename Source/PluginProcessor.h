#pragma once
#include "Params.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PepelVoxAudioProcessor : public juce::AudioProcessor
{
public:
    PepelVoxAudioProcessor();
    ~PepelVoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SUB-MIDI vox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return (int) pepel::presets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- MIDI-клипы (message thread)
    static constexpr int kSlots = 4;
    pepel::Clip getClip (int slot) const;
    void setClip (int slot, const pepel::Clip& c);
    void commitTake();                         // запись -> текущий слот
    void rebuildPlay();                        // обновить снимок для воспроизведения
    std::vector<pepel::MidiNote> getDisplayNotes (float& lengthBeats) const;
    bool writeMidiFile (int slot, const juce::File& dest) const;
    juce::File tempMidiFile (int slot) const;
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void clearCurrent();
    double currentBpm() const { return usedBpm.load(); }
    int grid() const { return (int) std::lround (mdGridRaw->load()); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inputLevel { 0 }, outputLevel { 0 }, dirt { 0 };
    std::atomic<int> liveNote { -1 }, currentSlot { 0 };
    std::atomic<bool> recording { false }, playing { false }, takeReady { false };
    std::atomic<double> recBeats { 0 }, playBeats { 0 }, usedBpm { 140.0 }, lastTakeBeats { 0 };
    int currentPreset = 0;

private:
    void refreshParams();

    pepel::Engine engine;
    pepel::Params params;
    pepel::MidiRecorder recorder;
    double sr = 48000.0;
    bool wasRec = false, wasPlaying = false;

    // клипы
    mutable juce::SpinLock clipLock;
    pepel::Clip clips[kSlots];
    pepel::Clip playClipQ;
    std::atomic<int> clipVersion { 0 };
    int seenVersion = -1;
    static constexpr int kPlayCap = 1024;
    pepel::MidiNote pn[kPlayCap];
    int pnCount = 0;
    float pnLen = 4.0f;
    bool pnActive[kPlayCap] {};
    double playPos = 0.0;
    int playNoteCur = -1;

#define X(id, label, mn, mx, def, step, skew, fmt) std::atomic<float>* id##Raw = nullptr;
    PEPEL_PARAMS (X)
#undef X

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PepelVoxAudioProcessor)
};

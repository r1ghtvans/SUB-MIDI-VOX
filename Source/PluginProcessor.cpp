#include "PluginProcessor.h"
#include "PluginEditor.h"

PepelVoxAudioProcessor::PepelVoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PEPELVOX", pepel::createLayout())
{
#define X(id, label, mn, mx, def, step, skew, fmt) id##Raw = apvts.getRawParameterValue (#id);
    PEPEL_PARAMS (X)
#undef X
}

bool PepelVoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet() == o;
}

void PepelVoxAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sampleRate);
    setLatencySamples (engine.latency());
    rebuildPlay();
    seenVersion = -1;
    wasRec = wasPlaying = false;
}

void PepelVoxAudioProcessor::refreshParams()
{
#define X(id, label, mn, mx, def, step, skew, fmt) params.id = id##Raw->load();
    PEPEL_PARAMS (X)
#undef X
}

void PepelVoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int nCh = buf.getNumChannels(), n = buf.getNumSamples();
    if (nCh < 1 || n < 1) return;

    refreshParams();
    engine.setParams (params);

    double bpm = params.mdBpm;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0 && *b < 400.0) bpm = *b;
    usedBpm.store (bpm);

    // снимок клипа для воспроизведения
    if (clipVersion.load() != seenVersion)
    {
        const juce::SpinLock::ScopedTryLockType lk (clipLock);
        if (lk.isLocked())
        {
            pnCount = std::min ((int) playClipQ.notes.size(), kPlayCap);
            for (int i = 0; i < pnCount; ++i) pn[i] = playClipQ.notes[(size_t) i];
            pnLen = playClipQ.lengthBeats;
            std::fill (std::begin (pnActive), std::end (pnActive), false);
            seenVersion = clipVersion.load();
        }
    }

    // запись
    const bool rec = recording.load();
    if (rec && ! wasRec) recorder.begin();
    if (! rec && wasRec)
    {
        recorder.finishAll (sr, bpm);
        lastTakeBeats.store (recorder.beats (sr, bpm));
        takeReady.store (true);
    }
    wasRec = rec;

    const bool play = playing.load() && ! rec;
    const bool mdOut = params.mdOut > 0.5f;

    // события воспроизведения
    struct PEv { int off, note, vel; bool on; int idx; };
    PEv pev[256]; int np = 0;
    auto addEv = [&] (int off, int note, int vel, bool on, int idx) { if (np < 256) pev[np++] = { off, note, vel, on, idx }; };

    if (play && ! wasPlaying)
    {
        playPos = 0; std::fill (std::begin (pnActive), std::end (pnActive), false); playNoteCur = -1;
        engine.setExternal (true, 36, false, 0.0f);
    }
    if (play)
    {
        const double bps = bpm / 60.0 / sr;
        double p0 = playPos; int off = 0, remaining = n;
        const double len = std::max (1.0f, pnLen);
        int guard = 0;
        while (remaining > 0 && guard++ < 16)
        {
            int seg = std::max (1, (int) std::ceil ((len - p0) / bps));
            seg = std::min (seg, remaining);
            const double p1 = p0 + seg * bps;
            for (int k = 0; k < pnCount; ++k)
            {
                const double s = pn[k].start, e = s + pn[k].len;
                if (pnActive[k] && e >= p0 && e < p1) { addEv (off + std::min (seg - 1, (int) ((e - p0) / bps)), pn[k].note, 0, false, k); pnActive[k] = false; }
                if (! pnActive[k] && s >= p0 && s < p1) { addEv (off + std::min (seg - 1, (int) ((s - p0) / bps)), pn[k].note, pn[k].vel, true, k); pnActive[k] = true; }
            }
            p0 = p1; off += seg; remaining -= seg;
            if (p0 >= len - 1e-9)
            {
                p0 -= len;
                for (int k = 0; k < pnCount; ++k) if (pnActive[k]) { addEv (off, pn[k].note, 0, false, k); pnActive[k] = false; }
            }
        }
        playPos = p0; playBeats.store (p0);
    }
    else if (wasPlaying)
    {
        for (int k = 0; k < pnCount; ++k)
            if (pnActive[k]) { addEv (0, pn[k].note, 0, false, k); pnActive[k] = false; }
        engine.setExternal (false, 0, false, 0);
    }
    wasPlaying = play;
    // сортировка: по смещению, note-off раньше note-on
    for (int i = 1; i < np; ++i)
    {
        PEv x = pev[i]; int j = i - 1;
        auto less = [] (const PEv& a, const PEv& b) { return a.off < b.off || (a.off == b.off && ! a.on && b.on); };
        while (j >= 0 && less (x, pev[j])) { pev[j + 1] = pev[j]; --j; }
        pev[j + 1] = x;
    }

    float* L = buf.getWritePointer (0);
    float* R = nCh > 1 ? buf.getWritePointer (1) : nullptr;
    float pkIn = 0, pkOut = 0;

    auto runSeg = [&] (int from, int to)
    {
        if (to <= from) return;
        const double recStart = recorder.sampleCount();
        engine.process (L + from, R ? R + from : nullptr, L + from, R ? R + from : nullptr, to - from);
        pkIn = std::max (pkIn, engine.inLevel); pkOut = std::max (pkOut, engine.outLevel);
        for (int k = 0; k < engine.nEv; ++k)
        {
            const auto& e = engine.ev[k];
            if (rec)
            {
                const double at = recStart + e.pos - e.back;
                if (e.on) recorder.noteOn (e.note, e.vel, at, sr, bpm); else recorder.noteOff (e.note, at, sr, bpm);
            }
            if (mdOut && ! play)
            {
                if (e.on) midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), from + e.pos);
                else midi.addEvent (juce::MidiMessage::noteOff (1, e.note), from + e.pos);
            }
        }
        if (rec) recorder.advance (to - from);
    };

    int pos = 0, ip = 0;
    while (pos < n)
    {
        const int next = ip < np ? std::clamp (pev[ip].off, pos, n) : n;
        runSeg (pos, next);
        pos = next;
        while (ip < np && std::clamp (pev[ip].off, 0, n) == pos)
        {
            const auto& e = pev[ip++];
            if (e.on) { playNoteCur = e.note; engine.setExternal (true, e.note, true, e.vel / 127.0f); }
            else if (e.note == playNoteCur) { engine.setExternal (true, playNoteCur, false, 0.8f); }
            if (mdOut)
            {
                if (e.on) midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), std::min (pos, n - 1));
                else midi.addEvent (juce::MidiMessage::noteOff (1, e.note), std::min (pos, n - 1));
            }
        }
    }

    // моно -> дубль в остальные каналы (если хост даст > 2)
    for (int c = 2; c < nCh; ++c) buf.clear (c, 0, n);

    if (rec) recBeats.store (recorder.beats (sr, bpm));
    inputLevel.store (pkIn); outputLevel.store (pkOut);
    dirt.store (engine.dirt); liveNote.store (engine.liveNote);
}

// ---------------------------------------------------------------- пресеты и состояние
void PepelVoxAudioProcessor::setCurrentProgram (int index)
{
    currentPreset = juce::jlimit (0, getNumPrograms() - 1, index);
    pepel::applyPreset (apvts, currentPreset);
}
const juce::String PepelVoxAudioProcessor::getProgramName (int index)
{
    const auto& p = pepel::presets();
    return index >= 0 && index < (int) p.size() ? juce::String (p[(size_t) index].name) : juce::String();
}

void PepelVoxAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("slot", currentSlot.load(), nullptr);
    state.removeChild (state.getChildWithName ("CLIPS"), nullptr);
    juce::ValueTree cl ("CLIPS");
    for (int i = 0; i < kSlots; ++i) cl.setProperty (juce::Identifier ("c" + juce::String (i)), juce::String (pepel::clipToText (getClip (i))), nullptr);
    state.appendChild (cl, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PepelVoxAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        currentPreset = juce::jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("preset", 0));
        currentSlot.store (juce::jlimit (0, kSlots - 1, (int) state.getProperty ("slot", 0)));
        auto cl = state.getChildWithName ("CLIPS");
        for (int i = 0; i < kSlots; ++i)
        {
            pepel::Clip c;
            if (cl.isValid() && pepel::clipFromText (cl.getProperty (juce::Identifier ("c" + juce::String (i))).toString().toStdString(), c))
                setClip (i, c);
        }
        state.removeChild (cl, nullptr);
        apvts.replaceState (state);
    }
}

// ---------------------------------------------------------------- клипы
pepel::Clip PepelVoxAudioProcessor::getClip (int slot) const
{
    const juce::SpinLock::ScopedLockType l (clipLock);
    return clips[juce::jlimit (0, kSlots - 1, slot)];
}
void PepelVoxAudioProcessor::setClip (int slot, const pepel::Clip& c)
{
    { const juce::SpinLock::ScopedLockType l (clipLock); clips[juce::jlimit (0, kSlots - 1, slot)] = c; }
    rebuildPlay();
}
void PepelVoxAudioProcessor::rebuildPlay()
{
    const auto c = getClip (currentSlot.load());
    const auto q = pepel::quantizeClip (c, grid());
    { const juce::SpinLock::ScopedLockType l (clipLock); playClipQ = q; }
    clipVersion.fetch_add (1);
}
void PepelVoxAudioProcessor::commitTake()
{
    pepel::Clip c;
    const int cnt = recorder.count.load (std::memory_order_acquire);
    for (int i = 0; i < cnt; ++i) c.notes.push_back (recorder.notes[i]);
    c.lengthBeats = std::max (4.0f, std::ceil ((float) lastTakeBeats.load() / 4.0f) * 4.0f);
    setClip (currentSlot.load(), c);
    takeReady.store (false);
}
void PepelVoxAudioProcessor::startRecording() { stopPlayback(); takeReady.store (false); recording.store (true); }
void PepelVoxAudioProcessor::stopRecording() { recording.store (false); }
void PepelVoxAudioProcessor::startPlayback() { if (! recording.load()) playing.store (true); }
void PepelVoxAudioProcessor::stopPlayback() { playing.store (false); }
void PepelVoxAudioProcessor::clearCurrent() { if (! recording.load()) setClip (currentSlot.load(), {}); }

std::vector<pepel::MidiNote> PepelVoxAudioProcessor::getDisplayNotes (float& len) const
{
    std::vector<pepel::MidiNote> out;
    if (recording.load())
    {
        const int cnt = recorder.count.load (std::memory_order_acquire);
        for (int i = 0; i < cnt; ++i) out.push_back (recorder.notes[i]);
        len = std::max (4.0f, std::ceil ((float) recBeats.load() / 4.0f + 0.01f) * 4.0f);
        return out;
    }
    const auto q = pepel::quantizeClip (getClip (currentSlot.load()), grid());
    len = q.lengthBeats;
    return q.notes;
}

bool PepelVoxAudioProcessor::writeMidiFile (int slot, const juce::File& dest) const
{
    const auto q = pepel::quantizeClip (getClip (slot), grid());
    if (q.empty()) return false;
    constexpr int ppq = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    juce::MidiMessageSequence tr;
    tr.addEvent (juce::MidiMessage::textMetaEvent (3, "SUB-MIDI vox"), 0);
    tr.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / std::max (20.0, usedBpm.load()))), 0);
    tr.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    for (auto& n : q.notes)
    {
        tr.addEvent (juce::MidiMessage::noteOn (1, n.note, (juce::uint8) n.vel), std::round (n.start * ppq));
        tr.addEvent (juce::MidiMessage::noteOff (1, n.note), std::round ((n.start + n.len) * ppq));
    }
    tr.sort(); tr.updateMatchedPairs();
    mf.addTrack (tr);
    dest.deleteFile();
    if (auto os = dest.createOutputStream()) return mf.writeTo (*os);
    return false;
}
juce::File PepelVoxAudioProcessor::tempMidiFile (int slot) const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SUB-MIDI_vox");
    dir.createDirectory();
    return dir.getChildFile (juce::String ("SUB-MIDI_vox_") + (char) ('A' + slot) + ".mid");
}

juce::AudioProcessorEditor* PepelVoxAudioProcessor::createEditor() { return new PepelVoxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PepelVoxAudioProcessor(); }

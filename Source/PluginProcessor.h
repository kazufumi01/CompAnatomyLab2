#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "MultiCore.h"
#include "Loudness.h"
#include "Presets.h"

// Per-sample history for the displays (≈10.9 s at 48 kHz). The audio thread writes, the editor reads; a torn read only
// affects a few pixels, so there is no lock. The same struct holds the frozen preview (filled on the message thread).
struct History
{
    static constexpr int kSize = 1 << 19, kMask = kSize - 1;
    std::vector<float> gr, mem, up, pre, inAbs, outAbs, outM, inL, inR;      // outM = mono output (for the spectrum)
    //       // mem = slow release part (dB), up = upward boost (dB), inL / inR = raw input
    std::atomic<uint64_t> pos { 0 };
    void allocate (bool withInput)
    {
        for (auto* v : { &gr, &mem, &up, &pre, &inAbs, &outAbs, &outM }) if (v->empty()) v->assign ((size_t) kSize, 0.0f);
        if (withInput) for (auto* v : { &inL, &inR }) if (v->empty()) v->assign ((size_t) kSize, 0.0f);
    }
};

class CompProcessor : public juce::AudioProcessor,
                      private juce::Timer
{
public:
    CompProcessor();
    ~CompProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Compressor Anatomy Lab 2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };        // editor-only state, saved with the plugin

    // ---- for the editor ----
    mod::Params currentParams() const;
    int latencyFor (const mod::Params&) const;
    double getSampleRateOrDefault() const { return sr > 0 ? sr : 48000.0; }
    bool sidechainConnected() const { return scConnected.load(); }
    float autoGainDbNow() const { return autoGainDb.load(); }

    // models and presets
    void applyModel (int index);              // one undo step; clears the preset
    int  currentModel() const { return (int) uiState.getProperty ("model", -1); }
    bool modelIsExact() const;
    void applyPreset (int index);             // factory preset: model + changes, one undo step
    juce::String presetLabel (bool ja) const; // "" when none; ends with " *" once edited
    juce::String presetNote (bool ja) const;
    static juce::File userPresetDir();
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::String& name, const juce::String& note);
    bool loadUserPreset (const juce::File&);
    void deleteUserPreset (const juce::File&);

    // parameter lock: a locked parameter keeps its value when a model or a preset is applied (saved with the session)
    bool isLocked (const juce::String& id) const { return locks.contains (id); }
    void setLocked (const juce::String& id, bool on);
    void clearLocks();
    int  numLocked() const { return locks.size(); }

    // A – D: four snapshots of the settings, saved with the session. copyCurrent = take the current settings along into that slot
    int  currentSlot() const { return juce::jlimit (0, 3, (int) uiState.getProperty ("slot", 0)); }
    void selectSlot (int index, bool copyCurrent);
    // IDEAL: about the same amount of reduction from a plain compressor — peak detector, an ideal curve fitted to this one,
    // one-stage timing, clean element and output. Not saved, not automated: a listening switch.
    std::atomic<bool> idealOn { false };
    // ear training: one hidden setting replaces the shown one (0 off, 1 attack, 2 release, 3 ratio, 4 whole model). Not saved, not automated.
    std::atomic<int> quizKind { 0 }, quizModel { 0 }; std::atomic<float> quizVal { 0.0f };
    // loudness (BS.1770): [0] = input, [1] = output; each M, S, I (LUFS), peak of the last 3 s, peak since the reset (dBFS)
    std::array<std::array<std::atomic<float>, 5>, 2> loud {};
    float pvLoud[2][5] {};                      // the same for the frozen preview
    std::atomic<bool> loudReset { false };
    bool idealAvailable() const { const int l = (int) apvts.getRawParameterValue ("law")->load(); return l != mod::LawRide && l != mod::LawSpectral && l != mod::LawLimit && l != mod::LawClip; }

    // displays: the live history, or — when the input has stopped — the last sound re-processed with the current settings
    History history;
    const History& display() const { return frozen.load() ? preview : history; }
    void updatePreview (double spanSeconds);  // message thread, from the editor's timer
    bool isFrozen() const { return frozen.load(); }
    bool frozenIsBuiltin() const { return snapBuiltin; }
    int  previewVersion() const { return pvVersion.load(); }
    float pvInPk = 0, pvOutPk = 0, pvGrPk = 0, pvUpPk = 0;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, grPeak { 0.0f }, grLast { 0.0f }, upPeak { 0.0f };    // linear / dB, max since the editor last read
    std::array<std::atomic<float>, 3> bandGr {}, bandUp {};
    std::array<std::atomic<float>, mod::Spectral::kBands> specGr {};       // spectral compression: GR per display band (live)
    std::array<float, mod::Spectral::kBands> pvSpec {};                    // the same for the frozen preview

private:
    void timerCallback() override;
    float v (int i) const { return raw[(size_t) i]->load(); }
    std::vector<float> presetValuesNow() const;
    void markPreset (int kind, int index, const juce::String& name, const juce::String& note);
    bool presetIsExact() const;

    juce::StringArray locks;
    std::unique_ptr<mod::Multi> core;
    double sr = 0;
    std::vector<const std::atomic<float>*> raw;
    std::atomic<bool> scConnected { false };
    std::atomic<float> autoGainDb { 0.0f };

    // dry path (delayed like the engine) for Delta / Bypass / Auto gain
    std::vector<float> dryRingL, dryRingR; int dryW = 0;
    static constexpr int kChunk = 1024;
    std::vector<float> dryL, dryR, delL, delR, tGr, tMem, tUp, tIn, tDetL, tDetR, scratchR, tPre;
    double pwIn = 0, pwOut = 0, agNow = 0, outNow = 0;

    // frozen preview
    History preview; std::atomic<bool> frozen { false }; std::atomic<int> pvVersion { 0 };
    std::atomic<uint64_t> lastLoud { 0 };
    std::unique_ptr<mod::Multi> pvCore; double pvSr = 0;
    mod::LoudMeter loudIn, loudOut;
    std::vector<float> snapL, snapR, pvKey, presetSnap; bool snapValid = false, snapBuiltin = false; uint64_t snapLoud = 0, lastSeenPos = 0;
    juce::uint32 lastAdvanceMs = 0, lastPvMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompProcessor)
};

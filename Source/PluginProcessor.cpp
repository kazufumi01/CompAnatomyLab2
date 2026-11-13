#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* kIds[] = { pid::input, pid::inXfmr, pid::scSource, pid::scHpf, pid::scTilt, pid::link, pid::band, pid::bandHz, pid::bandQ, pid::bandOnly,
                       pid::detector, pid::rmsWin, pid::topology, pid::law, pid::threshold, pid::ratio, pid::knee,
                       pid::timing, pid::attack, pid::release, pid::slowMult, pid::slowAmt, pid::hold, pid::lookahead, pid::avg,
                       pid::element, pid::drive, pid::os, pid::outStage, pid::makeup, pid::mix,
                       pid::scListen, pid::output, pid::autoGain, pid::delta, pid::bypass,
                       pid::upOn, pid::upThr, pid::upRatio, pid::upRange, pid::mb, pid::xLo, pid::xHi, pid::mbLo, pid::mbMid, pid::mbHi, pid::relCurve, pid::crest, pid::grLimit, pid::stereo, pid::scMb, pid::scLo, pid::scMid, pid::scHi, pid::pre, pid::preThr };
enum { iInput, iInX, iScSrc, iHpf, iTilt, iLink, iBand, iBandHz, iBandQ, iBandOnly, iDet, iRms, iTopo, iLaw, iThr, iRatio, iKnee,
       iTiming, iAtt, iRel, iSlowM, iSlowA, iHold, iLook, iAvg, iElem, iDrive, iOs, iOut, iMakeup, iMix, iScListen, iOutput, iAuto, iDelta, iByp,
       iUpOn, iUpThr, iUpRatio, iUpRange, iMb, iXLo, iXHi, iMbLo, iMbMid, iMbHi, iRelCurve, iCrest, iGrLimit, iStereo, iScMb, iScLo, iScMid, iScHi, iPre, iPreThr };

// 8 s of a drum-like loop for the preview before any audio has arrived
void builtinLoop (std::vector<float>& L, std::vector<float>& R, double fs)
{
    const int n = (int) (fs * 8.0); L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f); unsigned s = 7;
    auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (double) s / 4294967296.0 * 2.0 - 1.0; };
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / fs, beat = t * 92.0 / 60.0, ph = std::fmod (beat, 1.0), ph2 = std::fmod (beat + 0.5, 1.0), b4 = std::fmod (beat, 4.0);
        const double kick = std::sin (6.2831853 * (48.0 * ph * 60.0 / 92.0 + 4.0 * (1.0 - std::exp (-ph * 18.0)))) * std::exp (-ph * 7.0) * ((int) b4 % 2 == 0 ? 1.0 : 0.0);
        const double snare = (0.6 * rnd() + 0.4 * std::sin (6.2831853 * 190.0 * t)) * std::exp (-ph * 16.0) * ((int) b4 % 2 == 1 ? 1.0 : 0.0);
        const double hat = rnd() * std::exp (-ph2 * 60.0) * 0.12, bass = 0.22 * std::sin (6.2831853 * 55.0 * t) * (0.6 + 0.4 * std::exp (-ph * 3.0));
        const double pad = 0.05 * (std::sin (6.2831853 * 220.0 * t) + std::sin (6.2831853 * 277.2 * t) + std::sin (6.2831853 * 329.6 * t));
        const double v = 0.7 * kick + 0.5 * snare + hat + bass + pad;
        L[(size_t) i] = (float) (0.6 * v); R[(size_t) i] = (float) (0.6 * (v + 0.3 * hat - 0.02 * pad));
    }
}
}

CompProcessor::CompProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", createLayout())
{
    for (auto* id : kIds) raw.push_back (apvts.getRawParameterValue (id));
    history.allocate (true);
    startTimerHz (10);
}
CompProcessor::~CompProcessor() { stopTimer(); }

bool CompProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (l.getMainInputChannelSet() != out) return false;
    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

mod::Params CompProcessor::currentParams() const
{
    mod::Params p;
    p.input = v (iInput); p.inXfmr = v (iInX) > 0.5f;
    p.scHpfHz = hpfHz ((int) v (iHpf)); p.scTilt = (int) v (iTilt); p.link = v (iLink) / 100.0;
    p.band = (int) v (iBand); p.bandHz = v (iBandHz); p.bandQ = v (iBandQ); p.bandOnly = v (iBandOnly) > 0.5f;
    p.det = (int) v (iDet); p.rmsMs = v (iRms); p.feedback = (int) v (iTopo) == 1;
    p.law = (int) v (iLaw); p.threshold = v (iThr); p.ratio = v (iRatio); p.knee = v (iKnee);
    p.upOn = v (iUpOn) > 0.5f; p.upThreshold = v (iUpThr); p.upRatio = v (iUpRatio); p.upRange = v (iUpRange);
    p.timing = (int) v (iTiming); p.attack = v (iAtt); p.release = v (iRel); p.slowMult = v (iSlowM); p.slowAmt = v (iSlowA) / 100.0;
    p.holdMs = v (iHold); p.lookaheadMs = v (iLook); p.avgMs = v (iAvg);
    p.element = (int) v (iElem); p.driveDb = v (iDrive); p.os = 1 << juce::jlimit (0, 2, (int) v (iOs));
    p.out = (int) v (iOut); p.makeup = v (iMakeup); p.mix = v (iMix) / 100.0;
    p.mb = v (iMb) > 0.5f; p.xLo = v (iXLo); p.xHi = v (iXHi); p.bandGain[0] = v (iMbLo); p.bandGain[1] = v (iMbMid); p.bandGain[2] = v (iMbHi);
    p.relCurve = v (iRelCurve) / 100.0; p.crestDb = v (iCrest); p.grLimit = v (iGrLimit);
    p.stereo = (int) v (iStereo); p.scMb = v (iScMb) > 0.5f; p.scSens[0] = v (iScLo); p.scSens[1] = v (iScMid); p.scSens[2] = v (iScHi);
    p.pre = v (iPre) > 0.5f; p.preThr = v (iPreThr);
    if (p.law == mod::LawSpectral) p.mb = false;                    // spectral compression already works per frequency
    switch (quizKind.load (std::memory_order_relaxed))               // ear training: the question's value, whatever the screen shows
    {
        case 1: p.attack = quizVal.load(); break;
        case 2: p.release = quizVal.load(); break;
        case 3: p.ratio = quizVal.load(); break;
        case 4: { const int qm = quizModel.load(); if (qm >= 0 && qm < (int) mod::models().size()) { auto q = mod::models()[(size_t) qm].p; q.input = p.input; q.bypass = p.bypass; p = q; } break; }
        default: break;
    }
    if (idealOn.load (std::memory_order_relaxed) && p.law != mod::LawRide && p.law != mod::LawSpectral && p.law != mod::LawLimit && p.law != mod::LawClip)
    {
        mod::Params q = p; q.grLimit = 40;                              // fit an ideal curve through this one's static curve: the GR at 0 dB and the level where it is half of that
        auto gAt = [&q] (double x)                                      // feedback: the detector hears the reduced signal, so the reduction settles where g = law (x - g)
        {
            const double ff = mod::lawGr (x, q); if (! q.feedback) return ff;
            double a = 0.0, b = ff; for (int i = 0; i < 40; ++i) { const double mid = 0.5 * (a + b); (mod::lawGr (x - mid, q) > mid ? a : b) = mid; }
            return 0.5 * (a + b);
        };
        const double g1 = gAt (0.0);
        if (g1 > 0.5)
        {
            double lo = -80.0, hi = 0.0; for (int i = 0; i < 40; ++i) { const double mid = 0.5 * (lo + hi); (gAt (mid) < 0.5 * g1 ? lo : hi) = mid; }
            const double s = juce::jlimit (0.02, 0.99, 0.5 * g1 / std::max (0.01, -hi));
            p.ratio = juce::jlimit (1.0, 100.0, 1.0 / (1.0 - s)); p.threshold = juce::jlimit (-60.0, 0.0, -g1 / s);
        }
        if (p.law != mod::LawIdeal) p.knee = 6;
        p.law = mod::LawIdeal; p.det = mod::DetPeak; p.feedback = false; p.timing = mod::TimOne; p.element = mod::ElClean; p.out = mod::OutClean;
        p.inXfmr = false; p.pre = false; p.driveDb = 0; p.relCurve = 0; p.holdMs = 0;
    }
    return p;
}

void CompProcessor::selectSlot (int index, bool copyCurrent)
{
    index = juce::jlimit (0, 3, index); const int cur = currentSlot(); if (index == cur) return;
    auto global = [] (const char* id) { return std::strcmp (id, pid::scListen) == 0 || std::strcmp (id, pid::delta) == 0 || std::strcmp (id, pid::bypass) == 0; };   // listening switches stay as they are
    juce::StringArray a; for (auto* id : kIds) a.add (juce::String (apvts.getParameter (id)->getValue(), 6)); a.add (juce::String (currentModel()));
    uiState.setProperty ("slotData" + juce::String (cur), a.joinIntoString (","), nullptr);
    const auto s = copyCurrent ? juce::String() : uiState.getProperty ("slotData" + juce::String (index)).toString();       // an empty slot starts as a copy
    juce::StringArray t; t.addTokens (s, ",", ""); const int n = (int) std::size (kIds);
    if (t.size() >= n)
    {
        undo.beginNewTransaction();
        for (int k = 0; k < n; ++k) if (! global (kIds[k])) if (auto* q = apvts.getParameter (kIds[k])) { q->beginChangeGesture(); q->setValueNotifyingHost (t[k].getFloatValue()); q->endChangeGesture(); }
        if (t.size() > n) uiState.setProperty ("model", t[n].getIntValue(), nullptr);
    }
    uiState.setProperty ("slot", index, nullptr);
}

int CompProcessor::latencyFor (const mod::Params& p) const
{
    if (p.law == mod::LawSpectral) return mod::Spectral::N;
    return (int) std::lround (std::max (0.0, p.lookaheadMs) * 0.001 * getSampleRateOrDefault()) + (p.os >= 4 ? 35 : p.os >= 2 ? 31 : 0);
}

// ---------------------------------------------------------------------------- models and presets
std::vector<float> CompProcessor::presetValuesNow() const
{
    std::vector<float> o;
    for (auto& [id, val] : modelValues (mod::Params {})) { juce::ignoreUnused (val); if (auto* q = apvts.getParameter (id)) o.push_back (q->getValue()); }
    return o;
}
void CompProcessor::markPreset (int kind, int index, const juce::String& name, const juce::String& note)
{
    uiState.setProperty ("presetKind", kind, nullptr); uiState.setProperty ("preset", index, nullptr);
    uiState.setProperty ("userPreset", name, nullptr); uiState.setProperty ("presetNote", note, nullptr);
    presetSnap = presetValuesNow();
}
bool CompProcessor::presetIsExact() const
{
    const auto now = presetValuesNow();
    if (now.size() != presetSnap.size()) return false;
    for (size_t i = 0; i < now.size(); ++i) if (std::abs (now[i] - presetSnap[i]) > 1.0e-4f) return false;
    return true;
}

void CompProcessor::applyModel (int index)
{
    const auto& ms = mod::models();
    if (index < 0 || index >= (int) ms.size()) return;
    undo.beginNewTransaction();
    for (auto& [id, val] : modelValues (ms[(size_t) index].p))
        if (auto* q = apvts.getParameter (id); q != nullptr && ! isLocked (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (val)); q->endChangeGesture(); }
    uiState.setProperty ("model", index, nullptr);
    markPreset (0, -1, {}, {});
}

bool CompProcessor::modelIsExact() const
{
    const int i = currentModel(); const auto& ms = mod::models();
    if (i < 0 || i >= (int) ms.size()) return false;
    for (auto& [id, val] : modelValues (ms[(size_t) i].p))
        if (auto* q = apvts.getParameter (id))
            if (std::abs (q->getValue() - q->convertTo0to1 (val)) > 1.0e-4f) return false;
    return true;
}

void CompProcessor::applyPreset (int index)
{
    const auto& ps = preset::all();
    if (index < 0 || index >= (int) ps.size()) return;
    const auto& d = ps[(size_t) index];
    applyModel (preset::modelIndex (d.model));                      // starts the undo step
    for (auto& [id, val] : d.set)
        if (auto* q = apvts.getParameter (id); q != nullptr && ! isLocked (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (val)); q->endChangeGesture(); }
    markPreset (1, index, {}, {});
}

juce::String CompProcessor::presetLabel (bool ja) const
{
    const int kind = (int) uiState.getProperty ("presetKind", 0); juce::String s;
    if (kind == 1) { const int i = (int) uiState.getProperty ("preset", -1); if (i >= 0 && i < (int) preset::all().size()) s = juce::String::fromUTF8 (ja ? preset::all()[(size_t) i].ja : preset::all()[(size_t) i].en); }
    else if (kind == 2) s = uiState.getProperty ("userPreset", "").toString();
    if (s.isNotEmpty() && ! presetIsExact()) s += " *";
    return s;
}
juce::String CompProcessor::presetNote (bool ja) const
{
    const int kind = (int) uiState.getProperty ("presetKind", 0);
    if (kind == 1) { const int i = (int) uiState.getProperty ("preset", -1); if (i >= 0 && i < (int) preset::all().size()) return juce::String::fromUTF8 (ja ? preset::all()[(size_t) i].noteJa : preset::all()[(size_t) i].noteEn); }
    if (kind == 2) return uiState.getProperty ("presetNote", "").toString();
    return {};
}

juce::File CompProcessor::userPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("JPK").getChildFile ("Compressor Anatomy Lab 2").getChildFile ("Presets");
}
juce::Array<juce::File> CompProcessor::userPresets() const
{
    auto a = userPresetDir().findChildFiles (juce::File::findFiles, false, "*.xml");
    a.sort();
    return a;
}
bool CompProcessor::saveUserPreset (const juce::String& name, const juce::String& note)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    juce::XmlElement x ("CAL2Preset");
    x.setAttribute ("name", name.trim()); x.setAttribute ("note", note); x.setAttribute ("model", currentModel());
    for (auto& [id, val] : modelValues (mod::Params {}))
    { juce::ignoreUnused (val); if (auto* q = apvts.getParameter (id)) x.setAttribute (juce::String ("p_") + id, (double) q->convertFrom0to1 (q->getValue())); }
    userPresetDir().createDirectory();
    if (! x.writeTo (userPresetDir().getChildFile (clean + ".xml"))) return false;
    markPreset (2, -1, name.trim(), note);
    return true;
}
bool CompProcessor::loadUserPreset (const juce::File& f)
{
    auto x = juce::XmlDocument::parse (f);
    if (x == nullptr || ! x->hasTagName ("CAL2Preset")) return false;
    undo.beginNewTransaction();
    for (auto& [id, val] : modelValues (mod::Params {}))
    {
        juce::ignoreUnused (val); const auto key = juce::String ("p_") + id;
        if (auto* q = apvts.getParameter (id); q != nullptr && x->hasAttribute (key) && ! isLocked (id))
        { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 ((float) x->getDoubleAttribute (key))); q->endChangeGesture(); }
    }
    uiState.setProperty ("model", x->getIntAttribute ("model", -1), nullptr);
    markPreset (2, -1, x->getStringAttribute ("name", f.getFileNameWithoutExtension()), x->getStringAttribute ("note"));
    return true;
}
void CompProcessor::deleteUserPreset (const juce::File& f)
{
    const bool current = (int) uiState.getProperty ("presetKind", 0) == 2
                      && juce::File::createLegalFileName (uiState.getProperty ("userPreset", "").toString()) == f.getFileNameWithoutExtension();
    f.moveToTrash();
    if (current) markPreset (0, -1, {}, {});
}

void CompProcessor::setLocked (const juce::String& id, bool on)
{
    if (on) locks.addIfNotAlreadyThere (id); else locks.removeString (id);
    uiState.setProperty ("locks", locks.joinIntoString (","), nullptr);
}
void CompProcessor::clearLocks() { locks.clear(); uiState.setProperty ("locks", juce::String(), nullptr); }

// ---------------------------------------------------------------------------- audio
void CompProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    core = std::make_unique<mod::Multi> (sampleRate);
    const auto p = currentParams(); core->set (p);
    const int ring = (int) std::ceil (sampleRate * 0.012) + 64 + mod::Spectral::N;
    dryRingL.assign ((size_t) ring, 0.0f); dryRingR.assign ((size_t) ring, 0.0f); dryW = 0;
    for (auto* b : { &dryL, &dryR, &delL, &delR, &tGr, &tMem, &tUp, &tIn, &tDetL, &tDetR, &scratchR, &tPre }) b->assign ((size_t) kChunk, 0.0f);
    pwIn = pwOut = 0; agNow = 0; outNow = v (iOutput);
    loudIn.prepare (sampleRate); loudOut.prepare (sampleRate);
    setLatencySamples (core->latencySamples());
}

void CompProcessor::timerCallback()
{
    const int la = latencyFor (currentParams());
    if (la != getLatencySamples()) setLatencySamples (la);
}

void CompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainBus = getBusBuffer (buffer, true, 0);
    const int total = buffer.getNumSamples(), nch = mainBus.getNumChannels();
    if (nch == 0 || sr <= 0 || core == nullptr) return;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);

    auto scBus = getBusCount (true) > 1 ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();
    const bool scOn = getBusCount (true) > 1 && getBus (true, 1)->isEnabled() && scBus.getNumChannels() > 0;
    scConnected = scOn;
    const bool useExt = scOn && (int) v (iScSrc) == 1;

    core->set (currentParams());
    const bool bypass = v (iByp) > 0.5f, delta = v (iDelta) > 0.5f, listen = v (iScListen) > 0.5f, autoOn = v (iAuto) > 0.5f;
    const int ring = (int) dryRingL.size(), la = std::min (ring - 1, core->latencySamples());
    const double outTarget = v (iOutput);
    float* Lw = mainBus.getWritePointer (0);
    float* Rw = nch > 1 ? mainBus.getWritePointer (1) : scratchR.data();
    float inPk = 0, outPk = 0, grPk = 0, upPk = 0, grEnd = 0, bg[3] { 0, 0, 0 }, bu[3] { 0, 0, 0 };

    for (int o = 0; o < total; o += kChunk)
    {
        const int m = std::min (kChunk, total - o);
        float* L = Lw + o; float* R = nch > 1 ? Rw + o : scratchR.data();
        if (nch == 1) std::memcpy (R, L, (size_t) m * sizeof (float));
        const uint64_t p0 = history.pos.load (std::memory_order_relaxed);
        float rawPk = 0;
        for (int i = 0; i < m; ++i)                              // raw input, kept for the frozen preview
        {
            const size_t idx = (size_t) ((p0 + (uint64_t) i) & (uint64_t) History::kMask);
            history.inL[idx] = L[i]; history.inR[idx] = R[i]; rawPk = std::max (rawPk, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        const float* scL = nullptr; const float* scR = nullptr;
        if (useExt) { scL = scBus.getReadPointer (0) + o; scR = scBus.getNumChannels() > 1 ? scBus.getReadPointer (1) + o : scL; }
        mod::Taps tap; tap.gr = tGr.data(); tap.slow = tMem.data(); tap.up = tUp.data(); tap.inAbs = tIn.data(); tap.pre = tPre.data();
        if (listen) { tap.detL = tDetL.data(); tap.detR = tDetR.data(); }
        float cg[3], cu[3];
        core->run (L, R, L, R, m, scL, scR, tap, dryL.data(), dryR.data(), cg, cu);
        for (int b = 0; b < 3; ++b) { bg[b] = std::max (bg[b], cg[b]); bu[b] = std::max (bu[b], cu[b]); }
        if ((int) v (iLaw) == mod::LawSpectral) { const float* sb = core->specBands(); for (size_t j = 0; j < specGr.size(); ++j) if (sb[j] > specGr[j].load (std::memory_order_relaxed)) specGr[j].store (sb[j], std::memory_order_relaxed); }
        for (int i = 0; i < m; ++i)                              // dry reference, delayed like the engine
        {
            dryRingL[(size_t) dryW] = dryL[(size_t) i]; dryRingR[(size_t) dryW] = dryR[(size_t) i];
            int r = dryW - la; if (r < 0) r += ring;
            delL[(size_t) i] = dryRingL[(size_t) r]; delR[(size_t) i] = dryRingR[(size_t) r];
            if (++dryW >= ring) dryW = 0;
        }

        // Auto gain: bring the output's short-term loudness back to the input's
        double pi = 0, po = 0;
        for (int i = 0; i < m; ++i) { pi += 0.5 * ((double) delL[(size_t) i] * delL[(size_t) i] + (double) delR[(size_t) i] * delR[(size_t) i]);
                                      po += 0.5 * ((double) L[i] * L[i] + (double) R[i] * R[i]); }
        const double kP = 1.0 - std::exp (-(double) m / (sr * 1.5));
        pwIn += (pi / m - pwIn) * kP; pwOut += (po / m - pwOut) * kP;
        double agTarget = 0;
        if (autoOn && pwIn > 1e-10 && pwOut > 1e-10) agTarget = juce::jlimit (-24.0, 24.0, 10.0 * std::log10 (pwIn / pwOut));
        const double agPrev = agNow; agNow += (agTarget - agNow) * (1.0 - std::exp (-(double) m / (sr * 0.25)));
        const double outPrev = outNow; outNow += (outTarget - outNow) * (1.0 - std::exp (-(double) m / (sr * 0.02)));
        autoGainDb = (float) agNow;

        for (int i = 0; i < m; ++i)
        {
            const double f = (double) (i + 1) / m;
            const float g = (float) std::pow (10.0, ((agPrev + (agNow - agPrev) * f) + (outPrev + (outNow - outPrev) * f)) / 20.0);
            float yl = L[i] * g, yr = R[i] * g;
            if (bypass) { yl = delL[(size_t) i]; yr = delR[(size_t) i]; }
            else if (listen) { yl = tDetL[(size_t) i]; yr = tDetR[(size_t) i]; }
            else if (delta) { yl = delL[(size_t) i] - yl; yr = delR[(size_t) i] - yr; }
            L[i] = yl; if (nch > 1) R[i] = yr;
            const float ao = std::max (std::abs (yl), std::abs (yr));
            outPk = std::max (outPk, ao);
            const size_t idx = (size_t) ((p0 + (uint64_t) i) & (uint64_t) History::kMask);
            history.gr[idx] = tGr[(size_t) i]; history.mem[idx] = tMem[(size_t) i]; history.up[idx] = tUp[(size_t) i]; history.pre[idx] = tPre[(size_t) i];
            history.inAbs[idx] = tIn[(size_t) i]; history.outAbs[idx] = ao; history.outM[idx] = 0.5f * (yl + yr);
            inPk = std::max (inPk, tIn[(size_t) i]); grPk = std::max (grPk, tGr[(size_t) i]); upPk = std::max (upPk, tUp[(size_t) i]);
        }
        if (loudReset.exchange (false)) { loudIn.reset(); loudOut.reset(); }
        loudIn.process (delL.data(), delR.data(), m); loudOut.process (L, nch > 1 ? R : L, m);
        grEnd = tGr[(size_t) (m - 1)];
        history.pos.store (p0 + (uint64_t) m, std::memory_order_release);
        if (rawPk > 1.0e-4f) lastLoud.store (p0 + (uint64_t) m, std::memory_order_release);
    }
    inPeak = std::max (inPeak.load(), inPk); outPeak = std::max (outPeak.load(), outPk);
    grPeak = std::max (grPeak.load(), grPk); grLast = grEnd; upPeak = std::max (upPeak.load(), upPk);
    { const auto a = loudIn.vals(), b = loudOut.vals(); const float va[5] { a.m, a.s, a.i, a.pk, a.pkMax }, vb[5] { b.m, b.s, b.i, b.pk, b.pkMax };
      for (size_t k = 0; k < 5; ++k) { loud[0][k] = va[k]; loud[1][k] = vb[k]; } }
    for (size_t b = 0; b < 3; ++b) { bandGr[b] = std::max (bandGr[b].load(), bg[b]); bandUp[b] = std::max (bandUp[b].load(), bu[b]); }
}

// ---------------------------------------------------------------------------- frozen preview
// When the input has been silent for a second (or the host has stopped calling us), the displays switch to the last
// few seconds of input, re-processed off-line with the current settings every time a setting changes.
void CompProcessor::updatePreview (double spanSeconds)
{
    const double fs = getSampleRateOrDefault();
    const uint64_t pos = history.pos.load (std::memory_order_acquire), loud = lastLoud.load (std::memory_order_acquire);
    const auto now = juce::Time::getMillisecondCounter();
    if (pos != lastSeenPos) { lastSeenPos = pos; lastAdvanceMs = now; }
    const bool running = pos > 0 && now - lastAdvanceMs < 400, silent = pos - loud > (uint64_t) fs;
    if (running && ! silent) { frozen = false; snapValid = false; return; }

    if (! snapValid || snapLoud != loud)
    {
        const uint64_t want = std::min ((uint64_t) (fs * 8.3), (uint64_t) History::kSize - 4096);
        const uint64_t end = std::min (pos, loud + (uint64_t) (fs * 0.4)), n = std::min (want, end);
        if (loud == 0 || n < (uint64_t) (fs * 0.25)) { builtinLoop (snapL, snapR, fs); snapBuiltin = true; }
        else
        {
            snapL.resize ((size_t) n); snapR.resize ((size_t) n);
            for (uint64_t i = 0; i < n; ++i) { const size_t k = (size_t) ((end - n + i) & (uint64_t) History::kMask); snapL[(size_t) i] = history.inL[k]; snapR[(size_t) i] = history.inR[k]; }
            snapBuiltin = false;
        }
        snapValid = true; snapLoud = loud; pvKey.clear();
    }
    std::vector<float> key; key.reserve (raw.size() + 1);
    for (auto* r : raw) key.push_back (r->load());
    key.push_back ((float) spanSeconds); key.push_back (idealOn.load() ? 1.0f : 0.0f); key.push_back ((float) quizKind.load()); key.push_back (quizVal.load()); key.push_back ((float) quizModel.load());
    if (key == pvKey) { frozen = true; return; }
    if (now - lastPvMs < 60) return;                                  // while dragging: at most ~16 per second
    lastPvMs = now; pvKey = key;

    preview.allocate (true);
    std::copy (snapL.begin(), snapL.end(), preview.inL.begin()); std::copy (snapR.begin(), snapR.end(), preview.inR.begin());
    if (pvCore == nullptr || pvSr != fs) { pvCore = std::make_unique<mod::Multi> (fs); pvSr = fs; }
    auto p = currentParams(); p.os = 1;                               // the display does not need oversampling
    pvCore->reset(); pvCore->set (p);
    const int N = (int) snapL.size(), from = std::max (0, N - (int) (fs * (spanSeconds + 1.5)));    // the visible span plus a run-in
    const float outG = juce::Decibels::decibelsToGain (v (iOutput)); const bool bypass = v (iByp) > 0.5f;
    std::vector<float> ol ((size_t) kChunk), orr ((size_t) kChunk), dl ((size_t) kChunk), dr ((size_t) kChunk), tl ((size_t) kChunk), tr ((size_t) kChunk);
    mod::LoudMeter li, lo; li.prepare (fs); lo.prepare (fs);
    float ip = 0, op = 0, gp = 0, upk = 0; std::array<float, mod::Spectral::kBands> ps {};
    for (int o = from; o < N; o += kChunk)
    {
        const int m = std::min (kChunk, N - o);
        mod::Taps t; t.gr = preview.gr.data() + o; t.up = preview.up.data() + o; t.slow = preview.mem.data() + o; t.inAbs = preview.inAbs.data() + o; t.pre = preview.pre.data() + o;
        pvCore->run (snapL.data() + o, snapR.data() + o, ol.data(), orr.data(), m, nullptr, nullptr, t, dl.data(), dr.data());
        for (size_t i = 0; i < (size_t) m; ++i) { tl[i] = bypass ? dl[i] : outG * ol[i]; tr[i] = bypass ? dr[i] : outG * orr[i]; }
        li.process (dl.data(), dr.data(), m); lo.process (tl.data(), tr.data(), m);
        if (p.law == mod::LawSpectral && o + m > N - (int) (fs * spanSeconds)) { const float* sb = pvCore->specBands(); for (size_t j = 0; j < ps.size(); ++j) ps[j] = std::max (ps[j], sb[j]); }
        for (int i = 0; i < m; ++i)
        {
            const float a = bypass ? std::max (std::abs (dl[(size_t) i]), std::abs (dr[(size_t) i])) : outG * std::max (std::abs (ol[(size_t) i]), std::abs (orr[(size_t) i]));
            preview.outAbs[(size_t) (o + i)] = a;
            preview.outM[(size_t) (o + i)] = bypass ? 0.5f * (dl[(size_t) i] + dr[(size_t) i]) : outG * 0.5f * (ol[(size_t) i] + orr[(size_t) i]);
            if (o + i > N - (int) (fs * spanSeconds)) { op = std::max (op, a); ip = std::max (ip, preview.inAbs[(size_t) (o + i)]); gp = std::max (gp, preview.gr[(size_t) (o + i)]); upk = std::max (upk, preview.up[(size_t) (o + i)]); }
        }
    }
    for (int i = 0; i < from; ++i) { preview.pre[(size_t) i] = preview.gr[(size_t) i] = preview.up[(size_t) i] = preview.mem[(size_t) i] = preview.inAbs[(size_t) i] = preview.outAbs[(size_t) i] = preview.outM[(size_t) i] = 0.0f; }
    preview.pos.store ((uint64_t) N, std::memory_order_release);
    { const auto a = li.vals(), b = lo.vals(); const float va[5] { a.m, a.s, a.i, a.pk, a.pkMax }, vb[5] { b.m, b.s, b.i, b.pk, b.pkMax }; for (int k = 0; k < 5; ++k) { pvLoud[0][k] = va[k]; pvLoud[1][k] = vb[k]; } }
    pvSpec = ps; pvInPk = ip; pvOutPk = op; pvGrPk = gp; pvUpPk = upk;
    frozen = true; ++pvVersion;
}

juce::AudioProcessorEditor* CompProcessor::createEditor() { return new CompEditor (*this); }

void CompProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    uiState.setProperty ("presetEdited", ! presetIsExact(), nullptr);
    auto st = apvts.copyState();
    st.addChild (uiState.createCopy(), -1, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void CompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto st = juce::ValueTree::fromXml (*xml);
        if (! st.isValid()) return;
        auto ui = st.getChildWithName ("UI");
        if (ui.isValid()) { uiState.copyPropertiesFrom (ui, nullptr); st.removeChild (ui, nullptr); }
        apvts.replaceState (st);
        locks = juce::StringArray::fromTokens (uiState.getProperty ("locks", "").toString(), ",", ""); locks.removeEmptyStrings();
        presetSnap.clear();
        if (! (bool) uiState.getProperty ("presetEdited", false)) presetSnap = presetValuesNow();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompProcessor(); }

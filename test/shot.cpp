// Renders the editor to a PNG after feeding a drum-like test signal. Parameters from the environment:
//   ATT REL THR RATIO KNEE ELEM(0-3) DET(0/1) TOPO(0/1) RMSWIN HPF LOOK OVER(6/12/18) LIVE=1 JA=1 DELTA=1 SCALE
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    auto* cp = dynamic_cast<CompProcessor*> (p.get());
    p->setPlayConfigDetails (2, 2, 48000, 512);
    p->prepareToPlay (48000, 512);
    auto setP = [&] (const char* id, float v)
    { for (auto* q : p->getParameters()) if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (q)) if (w->paramID == id) w->setValueNotifyingHost (w->convertTo0to1 (v)); };
    auto env = [&] (const char* name, const char* id) { if (auto* v = getenv (name)) setP (id, (float) atof (v)); };
    setP ("threshold", -30); setP ("ratio", 4); setP ("knee", 2);
    env ("THR", "threshold"); env ("RATIO", "ratio"); env ("KNEE", "knee"); env ("ATT", "attack"); env ("REL", "release"); env ("ELEM", "element");
    env ("DET", "detector"); env ("TOPO", "topology"); env ("RMSWIN", "rmswin"); env ("HPF", "schpf"); env ("LOOK", "lookahead"); env ("MAKEUP", "makeup");
    env ("LAW", "law"); env ("TIMING", "timing"); env ("OS", "os"); env ("OUTSTAGE", "outstage");
    if (getenv ("LOCKLEVELS")) { setP ("input", 9); setP ("threshold", -33); cp->setLocked ("input", true); cp->setLocked ("threshold", true); }
    if (auto* mo = getenv ("MODEL")) cp->applyModel (atoi (mo));
    if (auto* pr = getenv ("PRESET")) cp->applyPreset (atoi (pr));
    env ("THR2", "threshold"); env ("INPUT", "input");
    if (auto* o2 = getenv ("OS2")) setP ("os", (float) atof (o2));      // after the model
    if (auto* h2 = getenv ("HPF2")) setP ("schpf", (float) atof (h2));
    if (getenv ("DELTA")) setP ("delta", 1);
    if (getenv ("AUTO")) setP ("autogain", 1);
    cp->uiState.setProperty ("lang", getenv ("JA") ? "ja" : "en", nullptr);
    cp->uiState.setProperty ("zoom", 1.0f, nullptr);
    if (getenv ("LIVE")) cp->uiState.setProperty ("grMode", 1, nullptr);
    if (getenv ("DYN")) cp->uiState.setProperty ("lowerTab", 1, nullptr);
    if (getenv ("MINI")) cp->uiState.setProperty ("lowerTab", 2, nullptr);
    if (auto* cv = getenv ("CARD")) cp->uiState.setProperty ("cardMode", atoi (cv), nullptr);
    if (auto* lv = getenv ("LESSON")) cp->uiState.setProperty ("lesson", atoi (lv), nullptr);
    if (getenv ("HARM")) cp->uiState.setProperty ("lowerTab", 3, nullptr);
    if (getenv ("SCF")) cp->uiState.setProperty ("lowerTab", 4, nullptr);
    if (auto* hv = getenv ("HARMF")) cp->uiState.setProperty ("harmF", atoi (hv), nullptr);
    if (auto* hv = getenv ("HARML")) cp->uiState.setProperty ("harmL", atoi (hv), nullptr);
    if (getenv ("SPEC")) cp->uiState.setProperty ("upperTab", 1, nullptr);
    if (getenv ("PANEL")) cp->uiState.setProperty ("panelMode", 1, nullptr);
    if (auto* sv = getenv ("SEE")) { cp->uiState.setProperty ("panelSee", atoi (sv), nullptr); cp->uiState.setProperty ("panelMode", 1, nullptr); }
    if (getenv ("IDEAL")) cp->idealOn = true;
    env ("MIX", "mix"); env ("STEREO", "stereo"); env ("SCMB", "scmb"); env ("SCLO", "sclo");
    if (auto* o = getenv ("OVER")) cp->uiState.setProperty ("stepOver", atoi (o), nullptr);

    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; juce::Random r (7);
    long n = 0;
    auto feed = [&] (int blocks)
    {
        for (int k = 0; k < blocks; ++k)
        {
            for (int i = 0; i < 512; ++i, ++n)
            {
                const double t = (double) n / 48000.0, ph = std::fmod (t * 92.0 / 60.0 * 2.0, 1.0);
                const double e = std::exp (-ph * 9.0) * (ph < 0.004 ? ph / 0.004 : 1.0);
                const float v = (float) (0.55 * e * (0.6 * std::sin (6.2831853 * 110.0 * t) + 0.4 * (r.nextFloat() * 2 - 1)) + 0.03 * std::sin (6.2831853 * 440.0 * t));
                b.setSample (0, i, v); b.setSample (1, i, v * 0.9f);
            }
            p->processBlock (b, m);
        }
    };
    if (! getenv ("NOFEED")) feed (420);
    const auto oldSkin = ZoomPrefs::file().getValue ("companatomy2.skin");
    if (auto* sk = getenv ("SKIN")) ZoomPrefs::file().setValue ("companatomy2.skin", atoi (sk));
    std::unique_ptr<juce::AudioProcessorEditor> e (cp->createEditor());
    if (getenv ("SKIN")) { ZoomPrefs::file().setValue ("companatomy2.skin", oldSkin); ZoomPrefs::file().saveIfNeeded(); }
    if (getenv ("FREEZE")) { for (int k = 0; k < 60; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (30);      // the input stops: frozen preview
                             if (auto* a = getenv ("ATT2")) setP ("attack", (float) atof (a));
                             for (int k = 0; k < 12; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (30); }
    else for (int k = 0; k < 24; ++k) { feed (4); juce::MessageManager::getInstance()->runDispatchLoopUntil (30); }
    const float sc = getenv ("SCALE") ? (float) atof (getenv ("SCALE")) : 1.0f;
    auto img = e->createComponentSnapshot (e->getLocalBounds(), true, sc);
    if (auto* c = getenv ("CROP")) { int x, y, w, h; if (std::sscanf (c, "%d,%d,%d,%d", &x, &y, &w, &h) == 4) img = img.getClippedImage ({ (int) (x * sc), (int) (y * sc), (int) (w * sc), (int) (h * sc) }); }
    juce::File out (argc > 1 ? argv[1] : "/tmp/comp_shot.png"); out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("latency %d  autogain %.2f\n", p->getLatencySamples(), cp->autoGainDbNow());
    e.reset(); p.reset();
    return 0;
}

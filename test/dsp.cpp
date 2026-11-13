// Processor-level checks: bypass is bit-exact (also with lookahead), defaults are finite and quiet-safe,
// the wet path equals mod::Core, Delta = dry − wet, latency follows Lookahead.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int bad = 0;
static void check (const char* what, bool ok, double a = 0, double b = 0)
{ std::printf ("  %-46s %s  (%g, %g)\n", what, ok ? "OK" : "FAIL", a, b); if (! ok) ++bad; }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const int FS = 48000, BS = 480, NB = 400;
    auto make = [&] { auto p = std::unique_ptr<juce::AudioProcessor> (createPluginFilter()); p->setPlayConfigDetails (2, 2, FS, BS); p->prepareToPlay (FS, BS); return p; };
    auto setP = [] (juce::AudioProcessor& p, const char* id, float v)
    { for (auto* q : p.getParameters()) if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (q)) if (w->paramID == id) w->setValueNotifyingHost (w->convertTo0to1 (v)); };
    std::vector<float> sigL ((size_t) (BS * NB)), sigR (sigL.size());
    juce::Random r (3);
    for (size_t i = 0; i < sigL.size(); ++i)
    {
        const double t = (double) i / FS, ph = std::fmod (t * 3.0, 1.0);
        sigL[i] = (float) (0.6 * std::exp (-ph * 7.0) * (r.nextFloat() * 2 - 1) + 0.1 * std::sin (6.2831853 * 220.0 * t)); sigR[i] = sigL[i] * 0.8f;
    }
    auto run = [&] (juce::AudioProcessor& p, std::vector<float>& oL, std::vector<float>& oR)
    {
        oL.resize (sigL.size()); oR.resize (sigL.size());
        juce::AudioBuffer<float> b (2, BS); juce::MidiBuffer m;
        for (int k = 0; k < NB; ++k)
        {
            std::memcpy (b.getWritePointer (0), sigL.data() + (size_t) k * BS, BS * 4); std::memcpy (b.getWritePointer (1), sigR.data() + (size_t) k * BS, BS * 4);
            p.processBlock (b, m);
            std::memcpy (oL.data() + (size_t) k * BS, b.getReadPointer (0), BS * 4); std::memcpy (oR.data() + (size_t) k * BS, b.getReadPointer (1), BS * 4);
        }
    };
    std::vector<float> aL, aR, bL, bR;

    std::printf ("bypass\n");
    { auto p = make(); setP (*p, "bypass", 1); setP (*p, "threshold", -40); run (*p, aL, aR);
      double d = 0; for (size_t i = 0; i < aL.size(); ++i) d = std::max (d, (double) std::abs (aL[i] - sigL[i]));
      check ("bypass == input (no lookahead)", d == 0.0, d); }
    { auto p = make(); setP (*p, "bypass", 1); setP (*p, "lookahead", 5); p->prepareToPlay (FS, BS); run (*p, aL, aR);
      const int la = (int) std::lround (5.0 * 0.001 * FS); double d = 0;
      for (size_t i = (size_t) la; i < aL.size(); ++i) d = std::max (d, (double) std::abs (aL[i] - sigL[i - (size_t) la]));
      check ("bypass == input delayed by lookahead", d == 0.0, d, la); }

    std::printf ("wet path\n");
    { auto p = make(); setP (*p, "threshold", -30); setP (*p, "ratio", 4); setP (*p, "knee", 2); setP (*p, "makeup", 0); run (*p, aL, aR);
      mod::Core c ((double) FS); mod::Params q; q.threshold = -30; q.ratio = 4; q.knee = 2; q.makeup = 0; q.attack = 10; q.release = 120; c.set (q);
      std::vector<float> oL (aL.size()), oR (aL.size());
      for (int k = 0; k < NB; ++k) c.run (sigL.data() + (size_t) k * BS, sigR.data() + (size_t) k * BS, oL.data() + (size_t) k * BS, oR.data() + (size_t) k * BS, BS);
      double d = 0; for (size_t i = 0; i < aL.size(); ++i) d = std::max (d, (double) std::abs (aL[i] - oL[i]));
      check ("plugin == mod::Core (default makeup 0)", d < 1e-6, d); }

    std::printf ("delta\n");
    { auto p = make(); setP (*p, "threshold", -30); run (*p, aL, aR); auto p2 = make(); setP (*p2, "threshold", -30); setP (*p2, "delta", 1); run (*p2, bL, bR);
      double d = 0; for (size_t i = 0; i < aL.size(); ++i) d = std::max (d, (double) std::abs ((sigL[i] - aL[i]) - bL[i]));
      check ("delta == dry - wet", d < 1e-6, d); }

    std::printf ("latency\n");
    { auto p = make(); setP (*p, "lookahead", 5); p->prepareToPlay (FS, BS); check ("latency reported after prepare", p->getLatencySamples() == 240, p->getLatencySamples(), 240); }

    std::printf ("finite / extremes\n");
    for (int el = 0; el < 4; ++el) for (int topo = 0; topo < 2; ++topo)
    {
        auto p = make(); setP (*p, "element", (float) el); setP (*p, "topology", (float) topo); setP (*p, "ratio", 20); setP (*p, "threshold", -60);
        setP (*p, "attack", 0.05f); setP (*p, "release", 10); setP (*p, "makeup", 24); setP (*p, "input", 12); setP (*p, "schpf", 6); setP (*p, "autogain", 1); run (*p, aL, aR);
        bool ok = true; for (float v : aL) if (! std::isfinite (v)) ok = false;
        check (("finite  element " + juce::String (el) + " topo " + juce::String (topo)).toRawUTF8(), ok);
    }
    std::printf (bad == 0 ? "ALL OK\n" : "FAILED %d\n", bad);
    return bad;
}

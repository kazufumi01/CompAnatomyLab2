#pragma once
#include "ModCore.h"

// =============================================================================
//  Models: a hardware type = a choice of method per stage + constants.
//  Names avoid trademarks; `ref` says which family the behaviour is patterned on.
//  Constants are starting values from published descriptions, not measurements of a unit.
// =============================================================================
namespace mod
{
struct Model { const char* id; const char* name; const char* ref; Params p; };

inline const std::vector<Model>& models()
{
    static const std::vector<Model> m = [] {
        std::vector<Model> v;
        auto add = [&v] (const char* id, const char* name, const char* ref, auto f) { Params p; f (p); v.push_back ({ id, name, ref, p }); };

        add ("digital", "Digital Precision", "ideal digital compressor", [] (Params& p) {
            p.det = DetPeak; p.law = LawIdeal; p.threshold = -24; p.ratio = 4; p.knee = 6;
            p.timing = TimPrecise; p.attack = 0.05; p.release = 120; p.holdMs = 5; p.lookaheadMs = 3; p.os = 4; });

        add ("fet76", "FET 76", "1176 family", [] (Params& p) {
            p.inXfmr = true; p.det = DetDiode; p.feedback = true; p.law = LawFET; p.threshold = -24; p.ratio = 4;
            p.timing = TimProg2; p.attack = 0.4; p.release = 150; p.slowMult = 6; p.slowAmt = 0.4;
            p.element = ElFET; p.out = OutXfmr; p.makeup = 6; p.os = 4; });

        add ("opto2a", "Opto 2A", "LA-2A family", [] (Params& p) {
            p.inXfmr = true; p.det = DetRMS; p.rmsMs = 5; p.feedback = true; p.law = LawOpto; p.threshold = -26; p.ratio = 3;
            p.timing = TimOptoMem; p.attack = 10; p.release = 60; p.element = ElOpto; p.out = OutTube; p.makeup = 5; p.os = 2; });

        add ("mu670", "Vari-Mu 670", "Fairchild 670 family", [] (Params& p) {
            p.inXfmr = true; p.det = DetPeak; p.feedback = true; p.law = LawVariMu; p.threshold = -18; p.ratio = 6;
            p.timing = TimOne; p.attack = 0.2; p.release = 300; p.element = ElTube; p.out = OutXfmr; p.makeup = 3; p.os = 4; });

        add ("busg", "Bus VCA G", "SSL G bus compressor family", [] (Params& p) {
            p.det = DetPeak; p.law = LawIdeal; p.threshold = -16; p.ratio = 4; p.knee = 6;
            p.timing = TimOne; p.attack = 10; p.release = 300; p.makeup = 2; p.os = 1; });

        add ("vca160", "VCA 160", "dbx 160 family", [] (Params& p) {              // true-RMS detector sets the timing: release is linear in dB
            p.det = DetRMS; p.rmsMs = 36; p.law = LawIdeal; p.threshold = -20; p.ratio = 4; p.knee = 0;
            p.timing = TimOne; p.attack = 1; p.release = 10; p.makeup = 4; p.os = 1; });

        add ("diode254", "Diode 254", "Neve 2254 / 33609 family", [] (Params& p) {
            p.inXfmr = true; p.det = DetPeak; p.feedback = true; p.law = LawDiode; p.threshold = -20; p.ratio = 3;
            p.timing = TimOne; p.attack = 5; p.release = 400; p.element = ElDiode; p.out = OutXfmr; p.makeup = 4; p.os = 4; });

        add ("vca2500", "VCA 2500", "API 2500 family", [] (Params& p) {
            p.det = DetRMS; p.rmsMs = 5; p.feedback = false; p.scTilt = 1; p.link = 1.0; p.law = LawIdeal; p.threshold = -18; p.ratio = 4; p.knee = 6;
            p.timing = TimOne; p.attack = 3; p.release = 200; p.out = OutXfmr; p.makeup = 3; p.os = 2; });

        add ("el8", "EL 8", "Distressor family", [] (Params& p) {
            p.det = DetPeak; p.law = LawIdeal; p.threshold = -22; p.ratio = 6; p.knee = 8;
            p.timing = TimProg2; p.attack = 1; p.release = 150; p.slowMult = 5; p.slowAmt = 0.3; p.out = OutH2; p.makeup = 5; p.os = 4; });

        add ("opto1b", "Tube Opto 1B", "Tube-Tech CL 1B family", [] (Params& p) {
            p.inXfmr = true; p.det = DetRMS; p.rmsMs = 3; p.law = LawIdeal; p.threshold = -22; p.ratio = 4; p.knee = 10;
            p.timing = TimOptoMem; p.attack = 5; p.release = 200; p.element = ElOpto; p.out = OutTube; p.makeup = 4; p.os = 2; });

        add ("ds1", "Digital DS-1", "Weiss DS1-MK3 family", [] (Params& p) {
            p.det = DetPeak; p.law = LawIdeal; p.threshold = -24; p.ratio = 3; p.knee = 4;
            p.timing = TimDualAvg; p.attack = 1; p.release = 80; p.slowMult = 10; p.avgMs = 100; p.lookaheadMs = 1; p.os = 4; });
        add ("limiter", "Limiter", "look-ahead brick-wall limiter", [] (Params& p) {
            p.det = DetPeak; p.law = LawLimit; p.threshold = -1; p.ratio = 100; p.knee = 0;
            p.timing = TimPrecise; p.attack = 0.02; p.release = 80; p.holdMs = 1.5; p.lookaheadMs = 1.5; p.os = 4; });

        add ("upward", "Upward", "upward compressor (raises quiet parts)", [] (Params& p) {
            p.det = DetRMS; p.rmsMs = 20; p.law = LawIdeal; p.threshold = 0; p.ratio = 1; p.knee = 0;
            p.upOn = true; p.upThreshold = -40; p.upRatio = 2; p.upRange = 12; p.timing = TimOne; p.attack = 10; p.release = 150; });

        add ("ott", "OTT 3-Band", "3-band upward + downward (OTT style)", [] (Params& p) {
            p.mb = true; p.xLo = 88; p.xHi = 2500; p.bandGain[0] = 4; p.bandGain[1] = 2; p.bandGain[2] = 4;
            p.det = DetRMS; p.rmsMs = 10; p.law = LawIdeal; p.threshold = -30; p.ratio = 10; p.knee = 6;
            p.upOn = true; p.upThreshold = -42; p.upRatio = 3; p.upRange = 18; p.timing = TimOne; p.attack = 12; p.release = 150; });
        add ("opto3a", "Opto 3A", "LA-3A family (solid-state opto)", [] (Params& p) {
            p.inXfmr = true; p.det = DetRMS; p.rmsMs = 2; p.feedback = true; p.law = LawOpto; p.threshold = -24; p.ratio = 3;
            p.timing = TimOptoMem; p.attack = 0.4; p.release = 100; p.element = ElOpto; p.out = OutXfmr; p.makeup = 4; p.os = 2; });

        add ("chane", "Channel VCA E", "SSL 4000 E channel compressor family", [] (Params& p) {
            p.det = DetPeak; p.law = LawIdeal; p.threshold = -20; p.ratio = 4; p.knee = 8;
            p.timing = TimOne; p.attack = 3; p.release = 300; p.makeup = 3; p.os = 1; });

        add ("crest", "Peak + RMS", "Kotelnikov-style crest-factor compressor", [] (Params& p) {
            p.det = DetCrest; p.rmsMs = 30; p.crestDb = 5; p.law = LawIdeal; p.threshold = -22; p.ratio = 3; p.knee = 6;
            p.timing = TimOne; p.attack = 3; p.release = 60; p.slowMult = 5; p.os = 2; });
        add ("rider", "Rider", "gain riding (Vocal Rider style, not compression)", [] (Params& p) {
            p.det = DetRMS; p.rmsMs = 300; p.law = LawRide; p.threshold = -20; p.ratio = 1; p.knee = 0;
            p.upThreshold = -45; p.upRange = 9; p.timing = TimOne; p.attack = 60; p.release = 300; });
        add ("dual", "Dual Opto + VCA", "Shadow Hills Mastering Compressor family (two stages in series)", [] (Params& p) {
            p.pre = true; p.preThr = -24; p.scHpfHz = 100; p.det = DetPeak; p.law = LawIdeal; p.threshold = -16; p.ratio = 2; p.knee = 4;
            p.timing = TimOne; p.attack = 10; p.release = 500; p.out = OutIron; p.makeup = 2; p.os = 2; });

        add ("spectral", "Spectral", "spectral compressor (a compressor per frequency)", [] (Params& p) {
            p.law = LawSpectral; p.threshold = -40; p.ratio = 3; p.knee = 6; p.attack = 10; p.release = 150; });
        add ("clipper", "Clipper", "clipper (a compressor with no attack and no release)", [] (Params& p) {
            p.law = LawClip; p.link = 0.0; p.threshold = -6; p.ratio = 100; p.knee = 3; p.attack = 0.02; p.release = 5; p.os = 4; });
        return v;
    }();
    return m;
}

// MODEL menu: grouped by how the unit works (ids, not positions: the order above is the saved model number and never changes)
struct MenuGroup { const char* title; std::vector<const char*> ids; };
inline const std::vector<MenuGroup>& menuGroups()
{
    static const std::vector<MenuGroup> g {
        { "Digital\xef\xbc\x88\xe3\x83\x87\xe3\x82\xb8\xe3\x82\xbf\xe3\x83\xab\xef\xbc\x89", { "digital", "limiter", "clipper", "spectral", "ds1", "crest" } },
        { "FET", { "fet76" } },
        { "Opto\xef\xbc\x88\xe5\x85\x89\xe5\xad\xa6\xe5\xbc\x8f\xef\xbc\x89", { "opto2a", "opto3a", "opto1b" } },
        { "Vari-Mu\xef\xbc\x88\xe7\x9c\x9f\xe7\xa9\xba\xe7\xae\xa1\xef\xbc\x89", { "mu670" } },
        { "VCA", { "busg", "chane", "vca160", "vca2500", "el8" } },
        { "Diode bridge\xef\xbc\x88\xe3\x83\x80\xe3\x82\xa4\xe3\x82\xaa\xe3\x83\xbc\xe3\x83\x89\xef\xbc\x89", { "diode254" } },
        { "2 stages\xef\xbc\x88\xe7\x9b\xb4\xe5\x88\x97 2 \xe6\xae\xb5\xef\xbc\x89", { "dual" } },
        { "Upward / Multiband / Riding", { "upward", "ott", "rider" } } };
    return g;
}
inline int modelIndexOf (const char* id) { const auto& m = models(); for (size_t i = 0; i < m.size(); ++i) if (std::strcmp (m[i].id, id) == 0) return (int) i; return -1; }
} // namespace mod

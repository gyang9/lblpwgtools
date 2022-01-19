#include "CAFAna/PRISM/PRISMAnalysisDefinitions.h"
#include "CAFAna/PRISM/PRISMUtils.h"

#include "CAFAna/Analysis/AnalysisBinnings.h"
#include "CAFAna/Analysis/AnalysisVars.h"

#include "CAFAna/Cuts/AnaCuts.h"
#include "CAFAna/Cuts/TruthCuts.h"

#include "CAFAna/Core/Utilities.h"

#include "StandardRecord/StandardRecord.h"

#include "TGraph.h"

#include <cmath>
#include <iostream>

using namespace ana;

namespace PRISM {

const Var kTrueOffAxisPos_m = SIMPLEVAR(abspos_x) * Constant(1.0E-2);

const Cut kETrueLT10GeV([](const caf::StandardRecord *sr) {
  return (sr->Ev < 10);
});

const Cut kETrue8GeV([](const caf::StandardRecord *sr) {
  return (sr->Ev < 8);
});

const Cut kERecoProxy8GeV([](const caf::StandardRecord *sr) {
  return (sr->eRecProxy <= 8);
}); 

Binning GetBinning(std::string const &xbinning) {
  if (xbinning == "uniform_fine") {
    return Binning::Simple(100, 0, 10);
  }
  if (xbinning == "uniform") {
    return Binning::Simple(150, 0, 25);
  }
  if (xbinning == "uniform_smallrange") {
    return Binning::Simple(50, 0, 10);
  }
  if (xbinning == "uniform_coarse") {
    return Binning::Simple(25, 0, 10); // used to be 25, 10 (bad tail going out to 10)
  }
  if (xbinning == "testopt") {
    std::vector<double> BE = {
        0,
    };

    while (BE.back() < 4) {
      BE.push_back(BE.back() + 0.25);
    }

    while (BE.back() < 8) {
      BE.push_back(BE.back() + 0.5);
    }

    return Binning::Custom(BE);
  } else if (xbinning == "default") {
    std::vector<double> BE = { 0, };
    while(BE.back() < 1) {
      BE.push_back(BE.back() + 0.5);
    }
    while(BE.back() < 4.0) {
      BE.push_back(BE.back() + 0.25);
    }
    while(BE.back() < 6.0) { 
      BE.push_back(BE.back() + 1.0);
    }
    while(BE.back() < 10.0) {
      BE.push_back(BE.back() + 4.0);
    }
    return Binning::Custom(BE);
  } else if (xbinning == "event_rate_match") {
    std::vector<double> BE = { 0, };

    while (BE.back() < 10) {
      BE.push_back(BE.back() + 0.2);
    }  

    while (BE.back() < 20) {
      BE.push_back(BE.back() + 1.0); 
    }

    while (BE.back() < 50) {
      BE.push_back(BE.back() + 5.0);   
    }

    while (BE.back() < 100) {
      BE.push_back(BE.back() + 10.0);
    }

    while (BE.back() < 120) {
      BE.push_back(BE.back() + 20.0);
    }

    return Binning::Custom(BE);
  } else {
    std::cout << "[ERROR]: Unknown PRISM binning definition: " << xbinning
              << std::endl;
    abort();
  }
}

Binning GetOABinning(std::string const &oabinning) {
  if (oabinning == "default") {
    std::array<double, 3> OABinning = {0.5, -30, 0}; // -32
    double OA_bin_width_m = OABinning[0];
    double OA_min_m = OABinning[1];
    double OA_max_m = OABinning[2];
    size_t NOABins = (OA_max_m - OA_min_m) / OA_bin_width_m;
    return Binning::Simple(NOABins, OA_min_m, OA_max_m);
  } else {
    std::cout << "[ERROR]: Unknown PRISM OA binning definition: " << oabinning
              << std::endl;
    abort();
  }
}

std::pair<std::string, Var> GetVar(std::string const &varname) {

  if (varname == "ETrue") {
    return std::make_pair("True E_{#nu} (GeV)", kTrueEnergy);
  } else if (varname == "ELep") {
    return std::make_pair("True E_{lep.} (GeV)", SIMPLEVAR(LepE));
  } else if (varname == "EHad") {
    return std::make_pair("True E_{had.} (GeV)", SIMPLEVAR(HadE));
  } else if (varname == "EP") {
    return std::make_pair("True E_{p} (GeV)", SIMPLEVAR(eP));
  } else if (varname == "EPipm") {
    return std::make_pair("True E_{#pi^{+/-}} (GeV)", SIMPLEVAR(ePipm));
  } else if (varname == "EPi0") {
    return std::make_pair("True E_{#pi^{0}} (GeV)", SIMPLEVAR(eTotalPi0));
  } else if (varname == "EOther") {
    return std::make_pair("True E_{other} (GeV)", SIMPLEVAR(eOther));
  } else if (varname == "EProxy") {
    return std::make_pair("Truth proxy E_{#nu} (GeV)", kProxyERec);
  } else if (varname == "ERec") {
    return std::make_pair("E_{Dep.} (GeV)", kRecoE_FromDep);
  } else if (varname == "EVisReco") {
    return std::make_pair("Reco E_{vis.} (GeV)", kEVisReco);
  } else if (varname == "EVisTrue") {
    return std::make_pair("True E_{vis.} (GeV)", kEVisTrue);
  } else if (varname == "RecoELep") {
    return std::make_pair("Reco E_{lep.} (GeV)", kLepEReco);
  } else if (varname == "RecoEHad") {
    return std::make_pair("Reco E_{had.} (GeV)", kHadEReco);
  }  else if (varname == "RecoEP") {
    return std::make_pair("Reco E_{p} (GeV)", kPEReco);
  }  else if (varname == "RecoEPipm") {
    return std::make_pair("Reco E_{#pi^{+/-}} (GeV)", kPipmEReco);
  }  else if (varname == "RecoEPi0") {
    return std::make_pair("Reco E_{#pi^{0}} (GeV)", kPi0EReco);
  }  else if (varname == "RecoEOther") {
    return std::make_pair("Reco E_{other} (GeV)", SIMPLEVAR(eRecoOther));
  } else {
    std::cout << "[ERROR]: Unknown PRISM var definition: " << varname
              << std::endl;
    abort();
  }
}

HistAxis GetEventRateMatchAxes(std::string const &binning) {
  auto vardef = GetVar("ETrue");
  return HistAxis(vardef.first, GetBinning(binning), vardef.second);
}

PRISMAxisBlob GetPRISMAxes(std::string const &varname,
                           std::string const &xbinning,
                           std::string const &oabinning) {

  HistAxis axOffAxisPos("Off axis position (m)", GetOABinning(oabinning),
                        kTrueOffAxisPos_m);

  HistAxis axOffAxis280kAPos("Off axis position (m)", Binning::Simple(1, -2, 0),
                             kTrueOffAxisPos_m);
  
  // Seperate ND and FD axes for ND->FD extrapolation.
  // Possible fine binning for ND axis, just keep them the
  // same for now.
  HistAxis xaxND = RecoObservable(varname, xbinning); // xbinning default_fine
  HistAxis xaxFD = RecoObservable(varname, xbinning);

  return {xaxND, xaxFD, axOffAxisPos, axOffAxis280kAPos};
}

// Return HistAxis for true energy version of observable
HistAxis TrueObservable(std::string const &obsvarname, 
                        std::string const &binning) {
  auto truevardef = GetVar("ETrue");
  std::vector<std::string> labels;
  std::vector<Binning> bins;
  std::vector<Var> vars;

  if (obsvarname == "EProxy") {
    truevardef = GetVar("ETrue");
  } else if (obsvarname == "ETrue") {
    truevardef = GetVar("ETrue");
  } else if (obsvarname == "RecoELep") {
    truevardef = GetVar("ELep");
  } else if (obsvarname == "ELep") {
    truevardef = GetVar("ELep");
  } else if (obsvarname == "EHad") {
    truevardef = GetVar("EHad");
  } else if (obsvarname == "RecoEHad") { 
    truevardef = GetVar("EHad");
  } else if (obsvarname == "EVisReco") {
    truevardef = GetVar("EVisTrue");
  } else if (obsvarname == "EVisTrue") {
    truevardef = GetVar("ETrue");
  } else if (obsvarname == "RecoEP") {
    truevardef = GetVar("EP");
  } else if (obsvarname == "RecoEPipm") { 
    truevardef = GetVar("EPipm");
  } else if (obsvarname == "RecoEPi0") {
    truevardef = GetVar("EPi0");
  } else if (obsvarname == "RecoEOther") {
    truevardef = GetVar("EOther");
  } else if (obsvarname == "ELepEHad") {
    auto truevardefLep = GetVar("ELep");
    labels.push_back(truevardefLep.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(truevardefLep.second);
    auto truevardefHad = GetVar("EHad");
    labels.push_back(truevardefHad.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(truevardefHad.second);
  } else if (obsvarname == "ELepEHadReco") {
    auto truevardefLep = GetVar("ELep");
    labels.push_back(truevardefLep.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(truevardefLep.second);
    auto truevardefHad = GetVar("EHad");
    labels.push_back(truevardefHad.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(truevardefHad.second);
  } else {
    std::cout << "[ERROR] Unknown var name: " << obsvarname << std::endl;
    abort();
  }   
  
  if (obsvarname == "ELepEHad" || obsvarname == "ELepEHadReco") {
    return HistAxis(labels, bins, vars);
  } else {
    return HistAxis(truevardef.first, GetBinning(binning), truevardef.second);
  }
}

HistAxis RecoObservable(std::string const &obsvarname,
                        std::string const &binning) {              
  std::vector<std::string> labels;
  std::vector<Binning> bins;
  std::vector<Var> vars;

  if (obsvarname == "ELepEHad") { // 2D ELep EHad Prediction
    auto vardefLep = GetVar("ELep");   
    labels.push_back(vardefLep.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(vardefLep.second);   
    auto vardefHad = GetVar("EHad");
    labels.push_back(vardefHad.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(vardefHad.second);
  } else if (obsvarname == "ELepEHadReco") {   
    auto vardefLep = GetVar("RecoELep");
    labels.push_back(vardefLep.first);
    bins.push_back(GetBinning(binning));
    vars.push_back(vardefLep.second); 
    auto vardefHad = GetVar("RecoEHad"); 
    labels.push_back(vardefHad.first);
    bins.push_back(GetBinning(binning)); 
    vars.push_back(vardefHad.second);   
  } else {
    auto vardef = GetVar(obsvarname);
    labels.push_back(vardef.first);
    bins.push_back(GetBinning(binning));  
    vars.push_back(vardef.second);
  }

  return HistAxis(labels, bins, vars);
}

bool isRecoND(std::string var) {
  if (var == "RecoELep" || var == "EVisReco" || var == "ELepEHadReco" ||
      var == "RecoEHad") {
    return true;
  } else {
    return false;
  }
}

const Cut kIsOutOfTheDesert([](const caf::StandardRecord *sr) {
  return (fabs(sr->vtx_x) < 200);
});

const Cut kCut280kARun([](const caf::StandardRecord *sr) {
  return (std::abs(sr->SpecialHCRunId) == 293);
});
const Cut kSel280kARun([](const caf::StandardRecord *sr) {
  return (std::abs(sr->SpecialHCRunId) == 280);
});
const Cut kIsReco([](const caf::StandardRecord *sr) {
  return (sr->Elep_reco != 0);
});
const Weight kSpecHCRunWeight([](const caf::StandardRecord *sr) { 
  return sr->SpecialRunWeight;
});

std::vector<double> const FDnumuFHCSelEff_enu = {
    0.546, 0.683, 0.821, 0.945, 1.07, 1.19, 1.44, 1.67,
    1.94,  2.25,  2.86,  4.17,  4.58, 5.44, 6.57, 7.65};

std::vector<double> const FDnumuFHCSelEff_eff = {
    0.624, 0.708, 0.761, 0.808, 0.839, 0.869, 0.902, 0.914,
    0.930, 0.944, 0.956, 0.958, 0.960, 0.943, 0.929, 0.912};

std::vector<double> const ND_UnGeoCorrectible_enu = {
    0.183, 0.502, 0.870, 1.25, 1.68, 2.45, 3.27, 4.19, 5.28, 6.48, 9.05};

std::vector<double> const ND_UnGeoCorrectible_eff = {0.996, 0.996, 0.985, 0.966,
                                                     0.958, 0.932, 0.898, 0.867,
                                                     0.845, 0.807, 0.742};

TGraph FDnumuFHCSelEff(FDnumuFHCSelEff_enu.size(), FDnumuFHCSelEff_enu.data(),
                       FDnumuFHCSelEff_eff.data());
const Weight kFDEff([](const caf::StandardRecord *sr) -> double {
  return FDnumuFHCSelEff.Eval(sr->Ev);
});

TGraph ND_UnGeoCorrectibleEff(ND_UnGeoCorrectible_enu.size(),
                              ND_UnGeoCorrectible_enu.data(),
                              ND_UnGeoCorrectible_eff.data());
const Weight kNDEff([](const caf::StandardRecord *sr) -> double {
  return ND_UnGeoCorrectibleEff.Eval(sr->Ev);
});

// Use to weight by Exposure
const Weight kRunPlanWeight([](const caf::StandardRecord *sr) -> double {
  return sr->perPOTWeight * sr->perFileWeight;
});

Cut GetNDSignalCut(bool UseOnAxisSelection, bool isNuMode) {

  return UseOnAxisSelection 
             ? ((isNuMode ? kPassND_FHC_NUMU : kPassND_RHC_NUMU) && kIsTrueFV &&
                kIsOutOfTheDesert) 
             : (kIsNumuCC && (isNuMode ? !kIsAntiNu : kIsAntiNu) && kIsTrueFV &&
                kIsOutOfTheDesert); 
}
Cut GetFDSignalCut(bool UseOnAxisSelection, bool isNuMode, bool isNuMu) {
  return UseOnAxisSelection
             ? ((isNuMu ? kPassFD_CVN_NUMU : kPassFD_CVN_NUE) && kIsTrueFV)
             : ((isNuMode ? !kIsAntiNu : kIsAntiNu) &&
                (isNuMu ? kIsNumuCC : kIsSig) && kIsTrueFV);
}

Weight GetAnalysisWeighters(std::string const &eweight, bool isNuMode) {
  Weight weight = kUnweighted;

  if (eweight.find("CVXSec") != std::string::npos) {
    weight = weight * kCVXSecWeights;
  }
  if (eweight.find("NDEff") != std::string::npos) {
    if (!isNuMode) {
      std::cout << "ERROR: Do not have efficiency curves for RHC, please add "
                   "them to PRISMAnalysisDefinitions.cxx or remove this weight"
                << std::endl;
      abort();
    }
    weight = weight * kNDEff;
  }
  if (eweight.find("FDEff") != std::string::npos) {
    if (!isNuMode) {
      std::cout << "ERROR: Do not have efficiency curves for RHC, please add "
                   "them to PRISMAnalysisDefinitions.cxx or remove this weight"
                << std::endl;
      abort();
    }
    weight = weight * kFDEff;
  }
  return weight;
}

Weight GetNDWeight(std::string const &eweight, bool isNuMode) {
  return kRunPlanWeight * kMassCorrection * kSpecHCRunWeight *
         GetAnalysisWeighters(eweight, isNuMode);
}
Weight GetFDWeight(std::string const &eweight, bool isNuMode) {
  return GetAnalysisWeighters(eweight, isNuMode);
}

static TH1 *numode_280kA, *nubarmode_280kA;

double Get280kAWeight_numu(double enu, bool isNu) {

  // lazy load the inputs
  static bool first = true;
  if (first) {

    TDirectory *gdc = gDirectory;
    TFile wf(
        (ana::FindCAFAnaDir() + "/Systs/OnAxis4m280kAWeights.root").c_str());
    wf.GetObject("ND_nu_HC280-HC293_ratio/LBNF_numu_flux", numode_280kA);
    if (!numode_280kA) {
      std::cout << "[ERROR]: Failed to read: "
                   "\"ND_nu_HC280-HC293_ratio/LBNF_numu_flux\" from "
                << (ana::FindCAFAnaDir() + "/OnAxis4m280kAWeights.root")
                << std::endl;
      throw;
    }
    numode_280kA->SetDirectory(nullptr);

    wf.GetObject("ND_nubar_HC280-HC293_ratio/LBNF_numubar_flux",
                 nubarmode_280kA);
    if (!nubarmode_280kA) {
      std::cout << "[ERROR]: Failed to read: "
                   "\"ND_nubar_HC280-HC293_ratio/LBNF_numubar_flux\" from "
                << (ana::FindCAFAnaDir() + "/OnAxis4m280kAWeights.root")
                << std::endl;
      throw;
    }
    nubarmode_280kA->SetDirectory(nullptr);
    if (gdc) {
      gdc->cd();
    }
    first = false;
  }

  TH1 *whist = (isNu ? numode_280kA : nubarmode_280kA);
  return whist->GetBinContent(whist->GetXaxis()->FindFixBin(enu));
}

const Weight k280kAWeighter([](const caf::StandardRecord *sr) -> double {
  if (sr->isFD || sr->det_x || (sr->vtx_x > 0)) {
    return 1;
  }
  // Only want to weight 'signal' numu species.
  if ((sr->isFHC && (sr->nuPDG != 14)) || (!sr->isFHC && (sr->nuPDG != -14))) {
    return 1;
  }
  return Get280kAWeight_numu(sr->Ev, sr->isFHC);
});

Weight GetNDSpecialRun(std::string const &SRDescriptor) {

  if (!SRDescriptor.length()) {
    return kUnweighted;
  }

  if (SRDescriptor == "OnAxis280kA") {
    return k280kAWeighter;
  } else {
    std::cout << "[ERROR]: Unknown special run type " << SRDescriptor
              << std::endl;
    abort();
  }
}

/*size_t GetNDConfigFromPred(Flavors::Flavors_t NDflav, Sign::Sign_t NDsign, 
                                  bool is280kA) const {
  size_t conf;
  assert(NDflav == Flavors::kAllNumu); // Only considering numu at ND.
  if (!is280kA) conf = (NDsign == Sign::kNu) ? kND_293kA_nu : kND_293kA_nub;
  else conf = (NDsign == Sign::kNu) ? kND_280kA_nu : kND_280kA_nub;
  return conf;
}
                                                                                
size_t GetFDConfigFromPred(Flavors::Flavors_t FDflav, Sign::Sign_t FDsign) const {
  size_t conf;
  if (FDflav == Flavors::kNuMuToNuMu) {
    conf = (FDsign == Sign::kNu) ? kFD_nu_numu : kFD_nub_numu;
  } else if (FDflav == Flavors::kNuMuToNuE) {
    conf = (FDsign == Sign::kNu) ? kFD_nu_nue : kFD_nub_nue;
  } else { abort(); }
                                                                                
  return GetFDConfig(conf);
}*/                                                                                          

bool operator&(NuChan const &l, NuChan const &r) {
  return bool(static_cast<int>(l) & static_cast<int>(r));
}

std::ostream &operator<<(std::ostream &os, NuChan const &nc) {
  switch (nc) {
  case NuChan::kNumuApp: {
    return os << "kNumuApp";
  }
  case NuChan::kNumuBarApp: {
    return os << "kNumuBarApp";
  }
  case NuChan::kNueApp: {
    return os << "kNueApp";
  }
  case NuChan::kNueBarApp: {
    return os << "kNueBarApp";
  }
  case NuChan::kNumuIntrinsic: {
    return os << "kNumuIntrinsic";
  }
  case NuChan::kNumuBarIntrinsic: {
    return os << "kNumuBarIntrinsic";
  }
  case NuChan::kNueIntrinsic: {
    return os << "kNueIntrinsic";
  }
  case NuChan::kNueBarIntrinsic: {
    return os << "kNueBarIntrinsic";
  }
  case NuChan::kNumu: {
    return os << "kNumu";
  }
  case NuChan::kNumuBar: {
    return os << "kNumuBar";
  }
  case NuChan::kNue: {
    return os << "kNue";
  }
  case NuChan::kNueBar: {
    return os << "kNueBar";
  }
  case NuChan::kNumuNumuBar: {
    return os << "kNumuNumuBar";
  }
  case NuChan::kNueNueBar: {
    return os << "kNueNueBar";
  }
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, BeamMode const &bm) {
  switch (bm) {
  case BeamMode::kNuMode: {
    return os << "kNuMode";
  }
  case BeamMode::kNuBarMode: {
    return os << "kNuBarMode";
  }
  case BeamMode::kNuMode_293kA: {
    return os << "kNuMode_293kA";
  }
  case BeamMode::kNuMode_280kA: {
    return os << "kNuMode_280kA";
  }
  case BeamMode::kNuBarMode_293kA: {
    return os << "kNuBarMode_293kA";
  }
  case BeamMode::kNuBarMode_280kA: {
    return os << "kNuBarMode_280kA";
  }
  }
  return os;
}
std::ostream &operator<<(std::ostream &os, BeamChan const &bm) {
  return os << "{ " << bm.mode << ", " << bm.chan << " }";
}
bool operator==(BeamChan const &l, BeamChan const &r) {
  return (l.mode == r.mode) && (l.chan == r.chan);
}

bool operator<(BeamChan const &l, BeamChan const &r) {
  if (l.mode == r.mode) {
    return l.chan < r.chan;
  }
  return l.mode < r.mode;
}

bool operator<(MatchChan const &l, MatchChan const &r) {
  if (l.from == r.from) {
    return l.to < r.to;
  }
  return l.from < r.from;
}
std::ostream &operator<<(std::ostream &os, MatchChan const &mc) {
  return os << mc.from << " -> " << mc.to;
}

} // namespace PRISM

#include "CAFAna/PRISM/PRISMAnalysisDefinitions.h"

#include "CAFAna/PRISM/PRISMUtils.h"

#include "CAFAna/Analysis/AnalysisBinnings.h"
#include "CAFAna/Analysis/AnalysisVars.h"

#include "CAFAna/Cuts/AnaCuts.h"
#include "CAFAna/Cuts/TruthCuts.h"

#include "StandardRecord/StandardRecord.h"

#include "TGraph.h"

#include <cmath>
#include <iostream>

using namespace ana;

namespace PRISM {

const Var kTrueOffAxisPos =
    (SIMPLEVAR(dune.det_x) + (SIMPLEVAR(dune.vtx_x) * Constant(1.0E-2)));

const Var kProxyE({}, [&](const caf::StandardRecord *sr) -> double {
  double eother = 0;
  if (std::isnormal(sr->dune.eOther)) {
    eother = sr->dune.eOther;
  }
  double eprox = sr->dune.LepE + sr->dune.eP + sr->dune.ePip + sr->dune.ePim +
                 sr->dune.ePi0 + 0.135 * sr->dune.nipi0 + eother;
  return eprox;
});

const Var kProxyE_20pclpe({}, [&](const caf::StandardRecord *sr) -> double {
  double eother = 0;
  if (std::isnormal(sr->dune.eOther)) {
    eother = sr->dune.eOther;
  }
  double eprox = sr->dune.LepE + 0.8 * sr->dune.eP + sr->dune.ePip +
                 sr->dune.ePim + sr->dune.ePi0 + 0.135 * sr->dune.nipi0 +
                 eother;
  return eprox;
});

const Cut kETrueLT10GeV({"kETrueLT10GeV"}, [](const caf::StandardRecord *sr) {
  return (sr->dune.Ev < 10);
});

Binning GetBinning(std::string const &xbinning) {
  if (xbinning == "uniform") {
    return Binning::Simple(100, 0, 10);
  }
  if (xbinning == "testopt") {
    std::vector<double> BE = {0, 0.25, 0.5};

    while (BE.back() < 3) {
      BE.push_back(BE.back() + 0.1);
    }

    while (BE.back() < 5) {
      BE.push_back(BE.back() + 0.25);
    }

    while (BE.back() < 10) {
      BE.push_back(BE.back() + 0.5);
    }

    return Binning::Custom(BE);
  } else if (xbinning == "default") {
    return binsNDEreco;
  } else {
    std::cout << "[ERROR]: Unknown PRISM binning definition: " << xbinning
              << std::endl;
    abort();
  }
}

Binning GetOABinning(std::string const &oabinning) {
  if (oabinning == "default") {
    std::array<double, 3> OABinning = {0.5, -3, 33};
    double OA_bin_width_m = OABinning[0];
    double OA_min_m = OABinning[1];
    double OA_max_m = OABinning[2];
    size_t NOABins = (OA_max_m - OA_min_m) / OA_bin_width_m;
    return Binning::Simple(NOABins, OA_min_m, OA_max_m);
  } else if (oabinning == "OneNegXBin") {
    std::vector<double> BE = {-3, 0};
    while (BE.back() < 32.75) {
      BE.push_back(BE.back() + 0.5);
    }
    return Binning::Custom(BE);
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
    return std::make_pair("True E_{lep.} (GeV)", SIMPLEVAR(dune.LepE));
  } else if (varname == "EProxy") {
    return std::make_pair("Truth proxy E_{#nu} (GeV)", kProxyE);
  } else if (varname == "EProxy_protonFD") {
    return std::make_pair("Truth proxy (20%% less proton energy) E_{#nu} (GeV)",
                          kProxyE_20pclpe);
  } else if (varname == "ERec") {
    return std::make_pair("E_{Dep.} (GeV)", kRecoE_FromDep);
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
                        kTrueOffAxisPos);

  auto vardef = GetVar(varname);
  HistAxis xax(vardef.first, GetBinning(xbinning), vardef.second);

  return {xax, axOffAxisPos};
}

const Cut kIsOutOfTheDesert({"kIsOutOfTheDesert"},
                            [](const caf::StandardRecord *sr) {
                              return (fabs(sr->dune.vtx_x) < 200);
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
const Var kFDEff({}, [&](const caf::StandardRecord *sr) -> double {
  return FDnumuFHCSelEff.Eval(sr->dune.Ev);
});

TGraph ND_UnGeoCorrectibleEff(ND_UnGeoCorrectible_enu.size(),
                              ND_UnGeoCorrectible_enu.data(),
                              ND_UnGeoCorrectible_eff.data());
const Var kNDEff({}, [&](const caf::StandardRecord *sr) -> double {
  return ND_UnGeoCorrectibleEff.Eval(sr->dune.Ev);
});

// Use to weight by Exposure
const Var kRunPlanWeight({}, [&](const caf::StandardRecord *sr) -> double {
  return sr->dune.perPOTWeight * sr->dune.perFileWeight;
});

Cut GetNDSignalCut(bool UseOnAxisSelection) {
  return UseOnAxisSelection
             ? (kPassND_FHC_NUMU && kIsTrueFV && kIsOutOfTheDesert)
             : (kIsNumuCC && !kIsAntiNu && kIsTrueFV && kIsOutOfTheDesert);
}
Cut GetFDSignalCut(bool UseOnAxisSelection) {
  return UseOnAxisSelection ? (kPassFD_CVN_NUMU && kIsTrueFV)
                            : (kIsNumuCC && !kIsAntiNu && kIsTrueFV);
}

Var GetAnalysisWeighters(std::string const &eweight) {
  Var weight = Constant(1);

  if (eweight.find("CVXSec") != std::string::npos) {
    weight = weight * kCVXSecWeights;
  }
  if (eweight.find("Eff") != std::string::npos) {
    weight = weight * kNDEff * kFDEff;
  }

  return weight;
}

Var GetNDWeight(std::string const &eweight) {
  return kRunPlanWeight * kMassCorrection * GetAnalysisWeighters(eweight);
}
Var GetFDWeight(std::string const &eweight) {
  return GetAnalysisWeighters(eweight);
}

} // namespace PRISM

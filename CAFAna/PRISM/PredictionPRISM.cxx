#include "CAFAna/Analysis/common_fit_definitions.h"

#include "CAFAna/PRISM/PredictionPRISM.h"
#include "CAFAna/PRISM/PRISMUtils.h"

#include "CAFAna/Prediction/PredictionsForPRISM.h"

#include "CAFAna/Core/Loaders.h"
#include "CAFAna/Core/OscCurve.h"
#include "CAFAna/Core/Spectrum.h"

#include "CAFAna/Analysis/AnalysisVars.h"

#include "CAFAna/Cuts/TruthCuts.h"

#include "TAxis.h"
#include "TDirectory.h"
#include "TH2.h"
#include "TObjString.h"

using namespace PRISM;

namespace ana {

static std::unique_ptr<ReweightableSpectrum> kNoSuchNDDataSpectrum{nullptr};
static std::unique_ptr<PredictionInterp> kNoSuchNDPredictionSpectrum{nullptr};
static std::unique_ptr<PredictionInterp> kNoSuchFDPredictionSpectrum{nullptr};

PredictionPRISM::PredictionPRISM(const HistAxis &AnalysisAxisND,
                                 const HistAxis &AnalysisAxisFD, 
                                 const HistAxis &NDOffAxis,
                                 const HistAxis &ND280kAAxis, 
                                 const HistAxis &TrueAnalysisAxis, 
                                 const HistAxis &NDFDEnergyMatchAxis,
                                 const HistAxis &CovarianceAxis,
                                 const HistAxis &OffPredictionAxis,
                                 const HistAxis &_280kAPredictionAxis,
                                 const HistAxis &FluxMatcherCorrectionAxes) :
                                 fAnalysisAxisND(AnalysisAxisND),
                                 fAnalysisAxisFD(AnalysisAxisFD),
                                 fNDOffAxis(NDOffAxis),
                                 fND280kAAxis(ND280kAAxis),
                                 fTrueAnalysisAxis(TrueAnalysisAxis),
                                 fNDFDEnergyMatchAxis(NDFDEnergyMatchAxis), 
                                 fCovarianceAxis(CovarianceAxis),
                                 fOffPredictionAxis(OffPredictionAxis),
                                 f280kAPredictionAxis(_280kAPredictionAxis),
                                 fFluxMatcherCorrectionAxes(FluxMatcherCorrectionAxes) {} 


void PredictionPRISM::Initialize() {
  fSetNDErrorsFromRate = true;
  fVaryNDFDMCData = false;

  PRISMOUT("PRISM analysis axis: "
           << fAnalysisAxisFD.GetLabels().front() << " with "
           << fAnalysisAxisFD.GetBinnings().front().Edges().size()
           << " bins from "
           << fAnalysisAxisFD.GetBinnings().front().Edges().front() << " to "
           << fAnalysisAxisFD.GetBinnings().front().Edges().back() << ".");

  PRISMOUT("PRISM Off axis axis: "
           << fNDOffAxis.GetLabels().front() << " with "
           << fNDOffAxis.GetBinnings().front().Edges().size() << " bins from "
           << fNDOffAxis.GetBinnings().front().Edges().front() << " to "
           << fNDOffAxis.GetBinnings().front().Edges().back() << ".");

  PRISMOUT("PRISM 280 kA axis: "
           << fND280kAAxis.GetLabels().front() << " with "
           << fND280kAAxis.GetBinnings().front().Edges().size() << " bins from "
           << fND280kAAxis.GetBinnings().front().Edges().front() << " to "
           << fND280kAAxis.GetBinnings().front().Edges().back() << ".");

  PRISMOUT("PRISM NDFDEnergyMatchAxis axis: "
           << fNDFDEnergyMatchAxis.GetLabels().front() << " with "
           << fNDFDEnergyMatchAxis.GetBinnings().front().Edges().size()
           << " bins from "
           << fNDFDEnergyMatchAxis.GetBinnings().front().Edges().front()
           << " to "
           << fNDFDEnergyMatchAxis.GetBinnings().front().Edges().back() << ".");

  fFluxMatcher = nullptr;

  fNDFD_Matrix = nullptr;

  fNCCorrection = true;
  fWSBCorrection = true;
  fWLBCorrection = true;
  fNuTauCCCorrection = true;
  fIntrinsicCorrection = true;

  fAxisAgreement = true;
  for (size_t bin = 0; bin < fAnalysisAxisFD.GetBinnings().size(); bin++) {
    for (size_t ed = 0; ed < fAnalysisAxisFD.GetBinnings().at(bin).Edges().size(); ed++) {     
      double FDedge = fAnalysisAxisFD.GetBinnings().at(bin).Edges().at(ed);
      double NDedge = fAnalysisAxisND.GetBinnings().at(bin).Edges().at(ed);
      if (FDedge != NDedge) {
        fAxisAgreement = false;
        break;
      }        
    }
  }
}

///\brief Call to add a ND Data component
///
///\details This can be called a number of times to add various ND 'data'
/// components that might be used in the PRISM analysis. Components are
/// defined by the combination ofNuChan andBeamMode.
/// The list of valid components is given below:
/// * NumuIntrinsic, NuMode (i.e. right sign mu-like FHC)
/// * NumuBarIntrinsic, NuBarMode (i.e. right sign mu-like RHC)
/// * NumuBarIntrinsic, NuMode (i.e. wrong sign mu-like FHC)
/// * NumuIntrinsic, NuBarMode (i.e. wrong sign mu-like FHC)
/// * NueNueBar, NuMode (i.e. all e-like FHC)
/// * NueNueBar, NuBarMode (i.e. all e-like RHC)
///
/// The SystShifts instance allows the 'data' to be shifted relative to the
/// nominal
void PredictionPRISM::AddNDDataLoader(SpectrumLoaderBase &ND_loader,
                                      const Cut &cut, const Weight &wei,
                                      ana::SystShifts shift,
                                      PRISM::BeamChan NDChannel) {
  std::unique_ptr<ReweightableSpectrum> &NDData = GetNDData(NDChannel);
  if (&NDData == &kNoSuchNDDataSpectrum) {
    std::cout << "ERROR: Invalid ND data type passed: " << NDChannel.chan << ":"
              << NDChannel.mode << std::endl;
    abort();
  }

  // Corrects for non-uniform off-axis binning
  auto slice_width_weight =
      NDSliceCorrection(50, fNDOffAxis.GetBinnings().front().Edges());
  auto slice_width_weight_280kA =
      NDSliceCorrection(50, fND280kAAxis.GetBinnings().front().Edges()); 

  NDData = std::make_unique<ReweightableSpectrum>(
      ND_loader, fAnalysisAxisND, fNDOffAxis, cut && kCut280kARun, shift,
      wei * slice_width_weight); 

  std::unique_ptr<ReweightableSpectrum> &NDData_280kA =
      GetNDData(NDChannel, 280);
  if (&NDData_280kA == &kNoSuchNDDataSpectrum) {
    std::cout << "ERROR: Invalid ND data type passed: " << NDChannel.chan << ":"
              << NDChannel.mode << std::endl;
    abort();
  }
  NDData_280kA = std::make_unique<ReweightableSpectrum>(
      ND_loader, fAnalysisAxisND, fND280kAAxis, cut && kSel280kARun, shift,
      wei * slice_width_weight_280kA); 
}

///\brief Call to add a ND MC component
///
///\details This can be called a number of times to add various ND MC
/// components that might be used in the PRISM analysis. Components are
/// defined by the combination ofNuChan andBeamMode.
/// The list of valid components is given below:
/// * NumuIntrinsic, NuMode (i.e. right sign mu-like FHC)
/// * NumuBarIntrinsic, NuBarMode (i.e. right sign mu-like RHC)
/// * NumuBarIntrinsic, NuMode (i.e. wrong sign mu-like FHC)
/// * NumuIntrinsic, NuBarMode (i.e. wrong sign mu-like FHC)
/// * NueNueBar, NuMode (i.e. all e-like FHC)
/// * NueNueBar, NuBarMode (i.e. all e-like RHC)
///
/// The systlist add allowable systematic shifts to the constructed PredInterp
/// for use in fits and systematic studies.
void PredictionPRISM::AddNDMCLoader(Loaders &loaders, const Cut &cut,
                                    const Weight &wei,
                                    std::vector<ana::ISyst const *> systlist,
                                    osc::IOscCalc* calc, PRISM::BeamChan NDChannel) {

  // Corrects for non-uniform off-axis binning
  auto slice_width_weight =
      NDSliceCorrection(50, fNDOffAxis.GetBinnings().front().Edges());
  auto slice_width_weight_280kA =
      NDSliceCorrection(50, fND280kAAxis.GetBinnings().front().Edges()); 

  std::unique_ptr<PredictionInterp> &NDPrediction = GetNDPrediction(NDChannel);
  if (&NDPrediction == &kNoSuchNDPredictionSpectrum) {
    std::cout << "ERROR: Invalid ND MC type passed: " << NDChannel.chan << ":"
              << NDChannel.mode << std::endl;
    abort();
  }

  //osc::NoOscillations kNoOsc;

  fPredGens.push_back(std::make_unique<NoOscPredictionGenerator>(
      fOffPredictionAxis, cut && kCut280kARun, wei * slice_width_weight));

  NDPrediction = std::make_unique<PredictionInterp>(systlist, calc,
                                                    *fPredGens.back(), loaders, kNoShift, 
                                                    PredictionInterp::kSplitBySign);

  std::unique_ptr<PredictionInterp> &NDPrediction_280kA =
      GetNDPrediction(NDChannel, 280);
  if (&NDPrediction_280kA == &kNoSuchNDPredictionSpectrum) {
    std::cout << "ERROR: Invalid ND MC type passed: " << NDChannel.chan << ":"
              << NDChannel.mode << std::endl;
    abort();
  }

  fPredGens.push_back(std::make_unique<NoOscPredictionGenerator>(
      f280kAPredictionAxis, cut && kSel280kARun,
      wei * slice_width_weight_280kA)); 

  NDPrediction_280kA = std::make_unique<PredictionInterp>(systlist, calc, 
                                                          *fPredGens.back(), loaders, kNoShift,
                                                          PredictionInterp::kSplitBySign);
}

///\brief Call to add a FD MC component
///
///\details This can be called a number of times to add various FD MC
/// components that might be used in the PRISM analysis. Components are
/// defined by the combination ofNuChan andBeamMode.
/// The list of valid components is given below:
/// * NumuNumuBar, NuMode (i.e. mu-like FHC)
/// * NumuNumuBar, NuBarMode (i.e. mu-like RHC)
/// * NueNueBar, NuMode (i.e. all e-like FHC)
/// * NueNueBar, NuBarMode (i.e. all e-like RHC)
///
/// The systlist add allowable systematic shifts to the constructed PredInterp
/// for use in fits and systematic studies.
void PredictionPRISM::AddFDMCLoader(Loaders &loaders, const Cut &cut,
                                    const Weight &wei,
                                    std::vector<ana::ISyst const *> systlist,
                                    osc::IOscCalc* calc, PRISM::BeamChan FDChannel) {

  //osc::NoOscillations kNoOsc;
  //osc::IOscCalcAdjustable *calc = NuFitOscCalc(1);

  std::unique_ptr<PredictionInterp> &FDPrediction = GetFDPrediction(FDChannel);
  if (&FDPrediction == &kNoSuchFDPredictionSpectrum) {
    std::cout << "ERROR: Invalid FD MC type passed: " << FDChannel.chan << ":"
              << FDChannel.mode << std::endl;
    abort();
  }

  fPredGens.push_back(
      std::make_unique<NoExtrapPredictionGenerator>(fAnalysisAxisFD, cut, wei));
  FDPrediction = std::make_unique<PredictionInterp>(systlist, calc,
                                                    *fPredGens.back(), loaders, kNoShift,
                                                    PredictionInterp::kSplitBySign);

  std::unique_ptr<PredictionInterp> &FDUnOscWeightedSigPrediction =
      GetFDUnOscWeightedSigPrediction(FDChannel);
  if (&FDUnOscWeightedSigPrediction == &kNoSuchFDPredictionSpectrum) {
    std::cout << "ERROR: Invalid FD MC type passed: " << FDChannel.chan << ":"
              << FDChannel.mode << std::endl;
    abort();
  }

  // Prediction for MC correction. Make sure there is no WSB!
  fPredGens.push_back(std::make_unique<FDNoOscPredictionGenerator>(
      fFluxMatcherCorrectionAxes, cut, wei)); 
  FDUnOscWeightedSigPrediction = std::make_unique<PredictionInterp>(
      systlist, calc, *fPredGens.back(), loaders, kNoShift, PredictionInterp::kSplitBySign); 

  // Prediction to oscillate numus by appearance probability.
  std::unique_ptr<PredictionInterp> &FDNonSwapAppOscPrediction =
      GetFDNonSwapAppOscPrediction(FDChannel.mode);
  // Always want unselected numu cut for this.
  auto kFDNumuCut = GetFDSignalCut(false, // No selection.
                                   ((FDChannel.mode == BeamMode::kNuMode) ? true : false),
                                   true); // Is Numu.
  fPredGens.push_back(std::make_unique<NonSwapNoExtrapPredictionGenerator>( 
      fTrueAnalysisAxis, kFDNumuCut, wei)); // fAnalysisAxisFD
  FDNonSwapAppOscPrediction = std::make_unique<PredictionInterp>(
      systlist, calc, *fPredGens.back(), loaders, kNoShift, PredictionInterp::kSplitBySign); 
 
  // Prediction to get oscillate nue appearance spectrum.
  std::unique_ptr<PredictionInterp> &FDNueSwapAppOscPrediction =
      GetFDNueSwapAppOscPrediction(FDChannel.mode);  
  // Always want unselected nue cut for this. 
  auto kFDNueCut = GetFDSignalCut(false, // No selection.
                                  ((FDChannel.mode == BeamMode::kNuMode) ? true : false), 
                                  false); // Is Nue.
  fPredGens.push_back( // fAnalysisAxisFD
      std::make_unique<NoExtrapPredictionGenerator>(fTrueAnalysisAxis, kFDNueCut, wei));
  FDNueSwapAppOscPrediction = std::make_unique<PredictionInterp>(
      systlist, calc, *fPredGens.back(), loaders, kNoShift, PredictionInterp::kSplitBySign);  
}

//-----------------------------------------------

std::unique_ptr<ReweightableSpectrum> & 
PredictionPRISM::GetNDData_right_sign_numu(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Measurements.ND_293kA.numu_ccinc_sel_numode
                       : Measurements.ND_280kA.numu_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Measurements.ND_293kA.numubar_ccinc_sel_nubmode
                       : Measurements.ND_280kA.numubar_ccinc_sel_nubmode;
  }
  return kNoSuchNDDataSpectrum;
}
std::unique_ptr<ReweightableSpectrum> &
PredictionPRISM::GetNDData_right_sign_nue(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Measurements.ND_293kA.nue_ccinc_sel_numode
                       : Measurements.ND_280kA.nue_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Measurements.ND_293kA.nuebar_ccinc_sel_nubmode
                       : Measurements.ND_280kA.nuebar_ccinc_sel_nubmode;
  }
  return kNoSuchNDDataSpectrum;
}
std::unique_ptr<ReweightableSpectrum> &
PredictionPRISM::GetNDData_wrong_sign_numu(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Measurements.ND_293kA.numubar_ccinc_sel_numode
                       : Measurements.ND_280kA.numubar_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Measurements.ND_293kA.numu_ccinc_sel_nubmode
                       : Measurements.ND_280kA.numu_ccinc_sel_nubmode;
  }
  return kNoSuchNDDataSpectrum;
}
std::unique_ptr<ReweightableSpectrum> &
PredictionPRISM::GetNDData(PRISM::BeamChan NDChannel, int kA) const {
  if (NDChannel.chan &
      ((NDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuIntrinsic
                                             : NuChan::kNumuBarIntrinsic)) {
    return GetNDData_right_sign_numu(NDChannel.mode, kA);
  } else if (NDChannel.chan & ((NDChannel.mode == BeamMode::kNuMode)
                                   ? NuChan::kNueIntrinsic
                                   : NuChan::kNueBarIntrinsic)) {
    return GetNDData_right_sign_nue(NDChannel.mode, kA);
  } else if (NDChannel.chan &
             ((NDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuBarIntrinsic
                                                    : NuChan::kNumuIntrinsic)) {
    return GetNDData_wrong_sign_numu(NDChannel.mode, kA);
  }
  PRISMOUT("Invalid ND Data request: " << NDChannel.mode << ", "
                                       << NDChannel.chan);
  return kNoSuchNDDataSpectrum;
}

bool PredictionPRISM::HaveNDData(PRISM::BeamChan NDChannel, int kA) const {
  return bool(GetNDData(NDChannel, kA));
}

std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetNDPrediction_right_sign_numu(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Predictions.ND_293kA.numu_ccinc_sel_numode
                       : Predictions.ND_280kA.numu_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Predictions.ND_293kA.numubar_ccinc_sel_nubmode
                       : Predictions.ND_280kA.numubar_ccinc_sel_nubmode;
  }
  return kNoSuchNDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetNDPrediction_right_sign_nue(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Predictions.ND_293kA.nue_ccinc_sel_numode
                       : Predictions.ND_280kA.nue_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Predictions.ND_293kA.nuebar_ccinc_sel_nubmode
                       : Predictions.ND_280kA.nuebar_ccinc_sel_nubmode;
  }
  return kNoSuchNDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetNDPrediction_wrong_sign_numu(BeamMode NDBM, int kA) const {
  if (NDBM == BeamMode::kNuMode) {
    return (kA == 293) ? Predictions.ND_293kA.numubar_ccinc_sel_numode
                       : Predictions.ND_280kA.numubar_ccinc_sel_numode;
  } else if (NDBM == BeamMode::kNuBarMode) {
    return (kA == 293) ? Predictions.ND_293kA.numu_ccinc_sel_nubmode
                       : Predictions.ND_280kA.numu_ccinc_sel_nubmode;
  }
  return kNoSuchNDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetNDPrediction(PRISM::BeamChan NDChannel, int kA) const {
  if (NDChannel.chan &
      ((NDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuIntrinsic
                                             : NuChan::kNumuBarIntrinsic)) {
    return GetNDPrediction_right_sign_numu(NDChannel.mode, kA);
  } else if (NDChannel.chan & ((NDChannel.mode == BeamMode::kNuMode)
                                   ? NuChan::kNueIntrinsic
                                   : NuChan::kNueBarIntrinsic)) {
    return GetNDPrediction_right_sign_nue(NDChannel.mode, kA);
  } else if (NDChannel.chan &
             ((NDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuBarIntrinsic
                                                    : NuChan::kNumuIntrinsic)) {
    return GetNDPrediction_wrong_sign_numu(NDChannel.mode, kA);
  }
  PRISMOUT("Invalid ND Prediction request: " << NDChannel.mode << ", "
                                             << NDChannel.chan);
  return kNoSuchNDPredictionSpectrum;
}

bool PredictionPRISM::HaveNDPrediction(PRISM::BeamChan NDChannel,
                                       int kA) const {
  return bool(GetNDPrediction(NDChannel, kA));
}

std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDPrediction_right_sign_numu(BeamMode FDBM) const {
  if (FDBM == BeamMode::kNuMode) {
    return Predictions.FD.numu_ccinc_sel_numode;
  } else if (FDBM == BeamMode::kNuBarMode) {
    return Predictions.FD.numubar_ccinc_sel_nubmode;
  }
  return kNoSuchFDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDPrediction_right_sign_nue(BeamMode FDBM) const {
  if (FDBM == BeamMode::kNuMode) {
    return Predictions.FD.nue_ccinc_sel_numode;
  } else if (FDBM == BeamMode::kNuBarMode) {
    return Predictions.FD.nuebar_ccinc_sel_nubmode;
  }
  return kNoSuchFDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDPrediction(PRISM::BeamChan FDChannel) const {
  if (FDChannel.chan &
      ((FDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuIntrinsic
                                             : NuChan::kNumuBarIntrinsic)) {
    return GetFDPrediction_right_sign_numu(FDChannel.mode);
  } else if (FDChannel.chan &
             ((FDChannel.mode == BeamMode::kNuMode) ? NuChan::kNueApp
                                                    : NuChan::kNueBarApp)) {
    return GetFDPrediction_right_sign_nue(FDChannel.mode);
  }
  PRISMOUT("Invalid FD Prediction request: " << FDChannel.mode << ", "
                                             << FDChannel.chan);
  return kNoSuchFDPredictionSpectrum;
}

bool PredictionPRISM::HaveFDPrediction(PRISM::BeamChan FDChannel) const {
  return bool(GetFDPrediction(FDChannel));
}

std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDUnOscWeightedSigPrediction_right_sign_numu(
    BeamMode FDBM) const {
  if (FDBM == BeamMode::kNuMode) {
    return Predictions.FD.numu_ccinc_sel_sig_numode;
  } else if (FDBM == BeamMode::kNuBarMode) {
    return Predictions.FD.numubar_ccinc_sel_sig_nubmode;
  }
  return kNoSuchFDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDUnOscWeightedSigPrediction_right_sign_nue(
    BeamMode FDBM) const {
  if (FDBM == BeamMode::kNuMode) {
    return Predictions.FD.nue_ccinc_sel_sig_numode;
  } else if (FDBM == BeamMode::kNuBarMode) {
    return Predictions.FD.nuebar_ccinc_sel_sig_nubmode;
  }
  return kNoSuchFDPredictionSpectrum;
}
std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDUnOscWeightedSigPrediction(
    PRISM::BeamChan FDChannel) const {
  if (FDChannel.chan &
      ((FDChannel.mode == BeamMode::kNuMode) ? NuChan::kNumuIntrinsic
                                             : NuChan::kNumuBarIntrinsic)) {
    return GetFDUnOscWeightedSigPrediction_right_sign_numu(FDChannel.mode);
  } else if (FDChannel.chan &
             ((FDChannel.mode == BeamMode::kNuMode) ? NuChan::kNueApp 
                                                    : NuChan::kNueBarApp)) { 
    return GetFDUnOscWeightedSigPrediction_right_sign_nue(FDChannel.mode);
  } 
  PRISMOUT("Invalid FD UnOscPrediction request: " << FDChannel.mode << ", "
                                                  << FDChannel.chan);

  return kNoSuchFDPredictionSpectrum;
}

bool PredictionPRISM::HaveFDUnOscWeightedSigPrediction(
    PRISM::BeamChan FDChannel) const {
  return bool(GetFDUnOscWeightedSigPrediction(FDChannel));
}

std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDNonSwapAppOscPrediction(PRISM::BeamMode FDBM) const {
  return (FDBM == BeamMode::kNuMode)
             ? Predictions.FD.numu_ccinc_sel_sig_apposc_numode
             : Predictions.FD.numubar_ccinc_sel_sig_apposc_nubmode;
}

std::unique_ptr<PredictionInterp> &
PredictionPRISM::GetFDNueSwapAppOscPrediction(PRISM::BeamMode FDBM) const {
  return (FDBM == BeamMode::kNuMode)
             ? Predictions.FD.nue_ccinc_sel_sig_apposc_numode
             : Predictions.FD.nuebar_ccinc_sel_sig_apposc_nubmode;
}

//----------------------------------------------------------------------
Spectrum PredictionPRISM::Predict(osc::IOscCalc *calc) const {
  return PredictSyst(calc, kNoShift);
}

//----------------------------------------------------------------------
Spectrum PredictionPRISM::PredictSyst(osc::IOscCalc *calc,
                                      SystShifts shift) const {
  std::map<PredictionPRISM::PRISMComponent, Spectrum> Comps =
      PredictPRISMComponents(calc, shift);

  assert(Comps.size());

  return Comps.at(kPRISMPred);
}

std::map<PredictionPRISM::PRISMComponent, Spectrum>
PredictionPRISM::PredictPRISMComponents(osc::IOscCalc *calc,
                                        SystShifts shift,
                                        MatchChan match_chan) const {
  bool WeHaveNDData = HaveNDData(match_chan.from);
  bool WeHaveNDPrediction = HaveNDPrediction(match_chan.from);
  bool WeHaveFDPrediction = HaveFDPrediction(match_chan.to);
  bool WeHaveFDUnOscWeightedSigPrediction =
      HaveFDUnOscWeightedSigPrediction(match_chan.to);

  if (!WeHaveNDData || !WeHaveNDPrediction || !WeHaveFDPrediction ||
      !WeHaveFDUnOscWeightedSigPrediction || !fFluxMatcher) {
    std::cout
        << "[ERROR]: Cannot make a PRISM prediction has we have: HaveNDData: "
        << (WeHaveNDData ? "have" : "do not have") << ", HaveNDPrediction: "
        << (WeHaveNDPrediction ? "have" : "do not have")
        << ", HaveFDPrediction: "
        << (WeHaveFDPrediction ? "have" : "do not have")
        << ", HaveFDUnOscWeightedSigPrediction: "
        << (WeHaveFDUnOscWeightedSigPrediction ? "have" : "do not have")
        << ", FluxMatcher: " << (fFluxMatcher ? "have" : "do not have")
        << std::endl;
    abort();
  }

  DontAddDirectory guard;

  auto &NDData = GetNDData(match_chan.from);
  auto &NDPrediction = GetNDPrediction(match_chan.from);

  auto &NDData_280kA = GetNDData(match_chan.from, 280);
  auto &NDPrediction_280kA = GetNDPrediction(match_chan.from, 280);

  auto &FDPrediction = GetFDPrediction(match_chan.to);
  auto &FDNonSwapAppOscPrediction = GetFDNonSwapAppOscPrediction(match_chan.to.mode); 
  auto &FDNueSwapAppOscPrediction = GetFDNueSwapAppOscPrediction(match_chan.to.mode); 
  auto &FDUnOscWeightedSigPrediction =
      GetFDUnOscWeightedSigPrediction(match_chan.to);

  PRISMOUT("Making PRISM prediction for: \n\t("
           << match_chan.from.mode << ":" << match_chan.from.chan << ") -> ("
           << match_chan.to.mode << ":" << match_chan.to.chan << ")");

  // Sort out the flavors and signs
  auto NDSigFlavor = (match_chan.from.chan & NuChan::kNumuNumuBar)
                         ? Flavors::kAllNuMu
                         : Flavors::kAllNuE;
  auto NDSigSign = ((match_chan.from.chan & NuChan::kNumu) ||
                    (match_chan.from.chan & NuChan::kNue))
                       ? Sign::kNu
                       : Sign::kAntiNu;
  auto NDWrongSign = (NDSigSign == Sign::kNu) ? Sign::kAntiNu : Sign::kNu;
  auto NDWrongFlavor =
      (NDSigFlavor == Flavors::kAllNuMu) ? Flavors::kAllNuE : Flavors::kAllNuMu;

  auto FDSigFlavor = (match_chan.to.chan & NuChan::kNumuNumuBar)
                         ? Flavors::kNuMuToNuMu
                         : Flavors::kNuMuToNuE;
  auto FDSigSign = ((match_chan.to.chan & NuChan::kNumu) ||
                    (match_chan.to.chan & NuChan::kNue))
                       ? Sign::kNu
                       : Sign::kAntiNu;

  auto FDWrongSign = (FDSigSign == Sign::kNu) ? Sign::kAntiNu : Sign::kNu;
  auto FDWrongFlavor = (FDSigFlavor == Flavors::kNuMuToNuMu)
                           ? Flavors::kAllNuE
                           : Flavors::kAllNuMu;
  auto FDIntrinsicFlavor = (FDSigFlavor == Flavors::kNuMuToNuMu)
                               ? Flavors::kNuEToNuMu
                               : Flavors::kNuEToNuE;
  
  PRISMOUT("\n\tNDSigFlavor: "
           << NDSigFlavor << "\n\tNDSigSign: " << NDSigSign
           << "\n\tNDWrongSign: " << NDWrongSign << "\n\tNDWrongFlavor: "
           << NDWrongFlavor << "\n\tFDSigFlavor: " << FDSigFlavor
           << "\n\tFDSigSign: " << FDSigSign << "\n\tFDWrongSign: "
           << FDWrongSign << "\n\tFDWrongFlavor: " << FDWrongFlavor
           << "\n\tFDIntrinsicFlavor: " << FDIntrinsicFlavor);

  // Using maps for non-default constructible classes is awful...
  std::map<PredictionPRISM::PRISMComponent, ReweightableSpectrum> NDComps;
  std::map<PredictionPRISM::PRISMComponent, Spectrum> Comps;

  RunPlan const &NDRunPlan = (match_chan.from.mode == PRISM::BeamMode::kNuMode)
                                 ? RunPlan_nu
                                 : RunPlan_nub;

  double NDPOT = NDRunPlan.GetPlanPOT();
  assert(NDPOT > 0);
  // Unweighted ND data
  NDComps.emplace(kNDData_unweighted_293kA, *NDData);
  // Weight the data to mock-up the proposed run-plan
  std::cout << "Run plan weighting ND data." << std::endl; 
  NDComps.emplace(kNDData_293kA,
                  NDRunPlan.Weight(*NDData, 293, fOffPredictionAxis, fSetNDErrorsFromRate));
  NDComps.emplace(kNDDataCorr2D_293kA, NDComps.at(kNDData_293kA)); 
  std::cout << "done." << std::endl;
  NDComps.emplace(kNDData_unweighted_280kA, *NDData_280kA);
  NDComps.emplace(kNDData_280kA,
                  NDRunPlan.Weight(*NDData_280kA, 280, f280kAPredictionAxis, fSetNDErrorsFromRate));
  NDComps.emplace(kNDDataCorr2D_280kA, NDComps.at(kNDData_280kA)); 
  
  // Start building MC components
  // Try doing background subtraction for MC as well, could be helpful for 
  // 'fake data' studies.
  ReweightableSpectrum NDSig = NDRunPlan.Weight(
    ToReweightableSpectrum(NDPrediction->
                           PredictSyst(calc, (fVaryNDFDMCData ? shift : kNoShift)), 
                           NDPOT, fAnalysisAxisND), 293, fOffPredictionAxis);

  
  NDComps.emplace(kNDSig_293kA, NDSig); 
  NDComps.emplace(kNDSig2D_293kA, NDSig);
  
  ReweightableSpectrum NDSig_280kA = NDRunPlan.Weight(
    ToReweightableSpectrum(NDPrediction_280kA->
                           PredictSyst(calc, (fVaryNDFDMCData ? shift : kNoShift)), 
                           NDPOT, fAnalysisAxisND), 280, f280kAPredictionAxis);

  NDComps.emplace(kNDSig_280kA, NDSig_280kA);
  NDComps.emplace(kNDSig2D_280kA, NDSig_280kA);

  // ND background subtraction:
  if (fNCCorrection) { // NC background subraction.
    ReweightableSpectrum NC = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                               Flavors::kAll, Current::kNC, Sign::kBoth),
            NDPOT, fAnalysisAxisND),
        293, fOffPredictionAxis);

    NDComps.emplace(kNDNCBkg_293kA, NC);

    NDComps.at(kNDDataCorr2D_293kA) -= NDComps.at(kNDNCBkg_293kA);
    NDComps.at(kNDSig2D_293kA) -= NDComps.at(kNDNCBkg_293kA);

    ReweightableSpectrum NC_280kA = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction_280kA->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                                     Flavors::kAll, Current::kNC, Sign::kBoth),
            NDPOT, fAnalysisAxisND),
        280, f280kAPredictionAxis);

    NDComps.emplace(kNDNCBkg_280kA, NC_280kA);
    NDComps.at(kNDDataCorr2D_280kA) -= NDComps.at(kNDNCBkg_280kA);
    NDComps.at(kNDSig2D_280kA) -= NDComps.at(kNDNCBkg_280kA);
  }
 
  if (fWLBCorrection) { // Wrong lepton background subraction.
    ReweightableSpectrum WLB = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                               NDWrongFlavor, Current::kCC, Sign::kBoth),
            NDPOT, fAnalysisAxisND),
        293, fOffPredictionAxis);

    NDComps.emplace(kNDWrongLepBkg_293kA, WLB);
    NDComps.at(kNDDataCorr2D_293kA) -= NDComps.at(kNDWrongLepBkg_293kA);
    NDComps.at(kNDSig2D_293kA) -= NDComps.at(kNDWrongLepBkg_293kA);

    ReweightableSpectrum WLB_280kA = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction_280kA->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                                     NDWrongFlavor, Current::kCC, Sign::kBoth),
            NDPOT, fAnalysisAxisND),
        280, f280kAPredictionAxis);

    NDComps.emplace(kNDWrongLepBkg_280kA, WLB_280kA);
    NDComps.at(kNDDataCorr2D_280kA) -= NDComps.at(kNDWrongLepBkg_280kA);
    NDComps.at(kNDSig2D_280kA) -= NDComps.at(kNDWrongLepBkg_280kA);
  }

  if (fWSBCorrection) { // Wrong sign background subraction.
    ReweightableSpectrum WSB = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                               NDSigFlavor, Current::kCC, NDWrongSign),
            NDPOT, fAnalysisAxisND),
        293, fOffPredictionAxis);

    NDComps.emplace(kNDWSBkg_293kA, WSB);
    NDComps.at(kNDDataCorr2D_293kA) -= NDComps.at(kNDWSBkg_293kA);
    NDComps.at(kNDSig2D_293kA) -= NDComps.at(kNDWSBkg_293kA);

    ReweightableSpectrum WSB_280kA = NDRunPlan.Weight(
        ToReweightableSpectrum(
            NDPrediction_280kA->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                                     NDSigFlavor, Current::kCC, NDWrongSign),
            NDPOT, fAnalysisAxisND),
        280, f280kAPredictionAxis);

    NDComps.emplace(kNDWSBkg_280kA, WSB_280kA);
    NDComps.at(kNDDataCorr2D_280kA) -= NDComps.at(kNDWSBkg_280kA);
    NDComps.at(kNDSig2D_280kA) -= NDComps.at(kNDWSBkg_280kA);
  }
  std::cout << "Backgrounds subtracted." << std::endl;
  //static osc::NoOscillations no;

  // FD MC spectrum with analysis axis/axes + Enu axis.
  Spectrum FDUnWeightedSig_Spec =
      FDUnOscWeightedSigPrediction->PredictComponentSyst(
          (osc::IOscCalc*)0, (fVaryNDFDMCData ? kNoShift : shift), FDSigFlavor,  
          Current::kCC, FDSigSign);
  
  // Convert FD MC to RWSpectrum with Enu as reweight variable.
  ReweightableSpectrum FDUnOscWeightedSig = ToReweightableSpectrum(
                                                FDUnWeightedSig_Spec,
                                                NDPOT, fAnalysisAxisFD); 
  // FD unoscillated prediction. 
  Comps.emplace(kFDUnOscPred, FDUnOscWeightedSig.UnWeighted());

  // Linear combination weight calculation.
  std::pair<TH1 const *, TH1 const *> LinearCombination =
      fFluxMatcher->GetFarMatchCoefficients(calc, match_chan, 
                                            (fVaryNDFDMCData ? kNoShift : shift)); 
  // Scale relative size of the weights to account for the run-plan.
  // E.g. more data taken on-axis means a smaller weight for the 
  // on-axis position weights.
  std::unique_ptr<TH1> UnRunPlannedLinearCombination_293kA =
      std::unique_ptr<TH1>(NDRunPlan.Unweight(LinearCombination.first, 293));
  std::unique_ptr<TH1> UnRunPlannedLinearCombination_280kA =
      std::unique_ptr<TH1>(NDRunPlan.Unweight(LinearCombination.second, 280));
  // We don't want the total POT of the runplan to affect the scale of the
  // coefficients, just the shape.
  UnRunPlannedLinearCombination_293kA->Scale(NDPOT);
  UnRunPlannedLinearCombination_280kA->Scale(NDPOT); 

  /*PRISMOUT("PRISM analysis axis: "
           << fNDOffAxis.GetLabels().front() << " with "
           << fNDOffAxis.GetBinnings().front().Edges().size() << " bins from "
           << fNDOffAxis.GetBinnings().front().Edges().front() << " to "
           << fNDOffAxis.GetBinnings().front().Edges().back() << ".");

  PRISMOUT("PRISM analysis axis: "
           << fND280kAAxis.GetLabels().front() << " with "
           << fND280kAAxis.GetBinnings().front().Edges().size() << " bins from "
           << fND280kAAxis.GetBinnings().front().Edges().front() << " to "
           << fND280kAAxis.GetBinnings().front().Edges().back() << ".");*/
  
  //Spectrum UnRunPlannedLinearCombination_293kA_s(fNDOffAxis.GetLabels(),
  //                                               fNDOffAxis.GetBinnings());
  LabelsAndBins anaAxis = LabelsAndBins(fNDOffAxis.GetLabels()[0], fNDOffAxis.GetBinnings()[0]);
  //Eigen::VectorXd UnRunPlannedLinearCombination_293kA_vec = 
  //    GetEigenFlatVector(UnRunPlannedLinearCombination_293kA.get());
  Eigen::ArrayXd UnRunPlannedLinearCombination_293kA_arr =
      GetEigenFlatArray(UnRunPlannedLinearCombination_293kA);

  Spectrum UnRunPlannedLinearCombination_293kA_s(
      std::move(UnRunPlannedLinearCombination_293kA_arr), anaAxis, NDPOT, 0);

  //UnRunPlannedLinearCombination_293kA_s.Clear();
  //UnRunPlannedLinearCombination_293kA_s.FillFromHistogram(
  //    UnRunPlannedLinearCombination_293kA.get());
  UnRunPlannedLinearCombination_293kA_s.OverridePOT(NDPOT);
  UnRunPlannedLinearCombination_293kA_s.OverrideLivetime(0);
  Comps.emplace(kNDFDWeightings_293kA, UnRunPlannedLinearCombination_293kA_s);


  LabelsAndBins anaAxis280 = LabelsAndBins(fND280kAAxis.GetLabels()[0], 
                                           fND280kAAxis.GetBinnings()[0]);
  //Eigen::VectorXd UnRunPlannedLinearCombination_280kA_vec =
  //    GetEigenFlatVector(UnRunPlannedLinearCombination_280kA.get());
  Eigen::ArrayXd UnRunPlannedLinearCombination_280kA_arr = 
      GetEigenFlatArray(UnRunPlannedLinearCombination_280kA);

  Spectrum UnRunPlannedLinearCombination_280kA_s(
      std::move(UnRunPlannedLinearCombination_280kA_arr), anaAxis280, NDPOT, 0);
  //Spectrum UnRunPlannedLinearCombination_280kA_s(fND280kAAxis.GetLabels(),
  //                                               fND280kAAxis.GetBinnings());
  //UnRunPlannedLinearCombination_280kA_s.Clear();
  //UnRunPlannedLinearCombination_280kA_s.FillFromHistogram(
  //    UnRunPlannedLinearCombination_280kA.get());
  UnRunPlannedLinearCombination_280kA_s.OverridePOT(NDPOT); 
  UnRunPlannedLinearCombination_280kA_s.OverrideLivetime(0);
  Comps.emplace(kNDFDWeightings_280kA, UnRunPlannedLinearCombination_280kA_s);
 
  // Ratio objects for weighting:
  Ratio UnRunPlannedLinearCombination_293kA_r(
      std::move(UnRunPlannedLinearCombination_293kA_arr), 
      fNDOffAxis.GetLabels(), fNDOffAxis.GetBinnings());
  Ratio UnRunPlannedLinearCombination_280kA_r(
      std::move(UnRunPlannedLinearCombination_280kA_arr),
      fND280kAAxis.GetLabels(), fND280kAAxis.GetBinnings());
 
  // Perform linear combination on raw (i.e. not extrapolated) ND data/MC.
  // MC linear combination:
  if (NDComps.count(kNDSig_293kA)) {
    Comps.emplace(
        kNDSig_293kA,
        NDComps.at(kNDSig2D_293kA)
            .WeightedBy(UnRunPlannedLinearCombination_293kA_r));
    Comps.emplace(
        kNDSig_280kA,
        NDComps.at(kNDSig2D_280kA)
            .WeightedBy(UnRunPlannedLinearCombination_280kA_r));

    Comps.emplace(kPRISMMC, Comps.at(kNDSig_293kA));
    Comps.at(kPRISMMC) += Comps.at(kNDSig_280kA);
  }
  // Data linear combination:
  Comps.emplace(
      kNDDataCorr_293kA,
      NDComps.at(kNDDataCorr2D_293kA)
          .WeightedBy(UnRunPlannedLinearCombination_293kA_r));
  Comps.emplace(
      kNDDataCorr_280kA,
      NDComps.at(kNDDataCorr2D_280kA)
          .WeightedBy(UnRunPlannedLinearCombination_280kA_r));

  Comps.emplace(kPRISMPred, Comps.at(kNDDataCorr_293kA));
  Comps.at(kPRISMPred) += Comps.at(kNDDataCorr_280kA);
  
  Comps.emplace(kNDLinearComb, Comps.at(kPRISMPred));

  // Numu -> Nue x-section correction as a function of true energy.
  if (FDSigFlavor == Flavors::kNuMuToNuE) {
    Spectrum FD_nueapp_spectrum = FDNueSwapAppOscPrediction->PredictComponentSyst(
        calc, (fVaryNDFDMCData ? kNoShift : shift), Flavors::kNuMuToNuE,
        Current::kCC, NDSigSign);
    //TH1D *FD_nueapp_h = FD_nueapp_spectrum.ToTH1(NDPOT);

    Spectrum FD_numusurv_apposc_spectrum =
        FDNonSwapAppOscPrediction
            ->PredictComponentSyst(calc, (fVaryNDFDMCData ? kNoShift : shift),
                                   Flavors::kNuMuToNuE, Current::kCC, NDSigSign);

    Comps.emplace(kFD_NumuNueCorr_Nue, FD_nueapp_spectrum);
    Comps.emplace(kFD_NumuNueCorr_Numu, FD_numusurv_apposc_spectrum);

    Ratio FD_NumuNueCorr_r(FD_nueapp_spectrum, FD_numusurv_apposc_spectrum, NDPOT);

    //TH1D *FD_numusurv_apposc_h = FD_numusurv_apposc_spectrum.ToTH1(NDPOT);

    //FD_nueapp_h->Divide(FD_numusurv_apposc_h);

    //Spectrum FD_NumuNueCorr = Comps.at(kPRISMPred);
    //FD_NumuNueCorr.Clear();
    //FD_NumuNueCorr.FillFromHistogram(FD_nueapp_h);
    //FD_NumuNueCorr.OverridePOT(NDPOT);
    //FD_NumuNueCorr.OverrideLivetime(0);
    //Comps.emplace(kFD_NumuNueCorr, FD_NumuNueCorr_r);

    // Give extrapolation method access to the nue/numu ratio
    // so this can be applied to the unfolded ND data.
    fNDFD_Matrix->SetNumuNueCorr(Comps.at(kFD_NumuNueCorr).ToTH1(NDPOT));

    //HistCache::Delete(FD_nueapp_h);
    //HistCache::Delete(FD_numusurv_apposc_h);
  }

  //-------------------------------------------------------------
  // Procedure for near to far extrapolation of PRISM prediction:
  // ------------------------------------------------------------
  std::cout << "HERE1" << std::endl; 
  // 1. Calculate efficiency of selection.
  fMCEffCorrection->CalcEfficiency(calc, fAnalysisAxisFD, (fVaryNDFDMCData ? kNoShift : shift), 
                                   NDSigFlavor, FDSigFlavor, 
                                   Current::kCC, NDSigSign, FDSigSign);
  // Do ND to FD detector extrapolation here
  // Extrapolate just the LC ND, not the MC
  // 2. Extrapolate 293kA sample.
  std::cout << "HERE2" << std::endl;
  fNDFD_Matrix->ExtrapolateNDtoFD(NDComps.at(kNDDataCorr2D_293kA), NDPOT, 293,
                                  UnRunPlannedLinearCombination_293kA.get(),
                                  calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                  NDSigFlavor, FDSigFlavor, Current::kCC,
                                  NDSigSign, FDSigSign, 
                                  fMCEffCorrection->GetNDefficiency(293),
                                  fMCEffCorrection->GetFDefficiency());
  // 3. Extrapolate 280kA sample.
  fNDFD_Matrix->ExtrapolateNDtoFD(NDComps.at(kNDDataCorr2D_280kA), NDPOT, 280,
                                  UnRunPlannedLinearCombination_280kA.get(),
                                  calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                  NDSigFlavor, FDSigFlavor, Current::kCC,
                                  NDSigSign, FDSigSign, 
                                  fMCEffCorrection->GetNDefficiency(280),
                                  fMCEffCorrection->GetFDefficiency());
  std::cout << "HERE3" << std::endl;
  /*ReweightableSpectrum sNDExtrap_293kA =  
    ReweightableSpectrum(ana::Constant(1), fNDFD_Matrix->GetNDExtrap_293kA(),
                         fAnalysisAxisFD.GetLabels(), fAnalysisAxisFD.GetBinnings(),
                         NDPOT, 0);*/
  LabelsAndBins ExtrapAnaAxis(fAnalysisAxisFD.GetLabels().at(0), 
                              fAnalysisAxisFD.GetBinnings().at(0));
  LabelsAndBins ExtrapWeightAxis(fAnalysisAxisFD.GetLabels().at(1), 
                                 fAnalysisAxisFD.GetBinnings().at(1));
  LabelsAndBins CovAnaAxis(fCovarianceAxis.GetLabels().at(0), 
                           fCovarianceAxis.GetBinnings().at(0));
  LabelsAndBins CovWeightAxis(fCovarianceAxis.GetLabels().at(1),
                              fCovarianceAxis.GetBinnings().at(1));
  std::cout << "HERE4" << std::endl;
  // 4. Get extrapolated 293kA sample.
  ReweightableSpectrum sNDExtrap_293kA(std::move(fNDFD_Matrix->GetNDExtrap_293kA()),
                                       ExtrapAnaAxis, ExtrapWeightAxis, NDPOT, 0);
  NDComps.emplace(kNDDataExtrap2D_293kA, sNDExtrap_293kA);

  // 5. Weight extrapolated 293kA ND data by linear combination coeffiecients.
  Comps.emplace(
      kNDDataExtrap_293kA,
      sNDExtrap_293kA.WeightedBy(UnRunPlannedLinearCombination_293kA_r));   
  // 6. Get covariance matrix propagated through 293kA linear combination.
  /*ReweightableSpectrum sCovMat(ana::Constant(1), fNDFD_Matrix->GetCovMat_293kA(),
                               fCovarianceAxis.GetLabels(), fCovarianceAxis.GetBinnings(),
                               NDPOT, 0); */
  ReweightableSpectrum sCovMat(std::move(fNDFD_Matrix->GetCovMat_293kA()),
                               CovAnaAxis, CovWeightAxis, NDPOT, 0);

  // 7. Get extrapolated 280kA sample.
  ReweightableSpectrum sNDExtrap_280kA(std::move(fNDFD_Matrix->GetNDExtrap_280kA()),
                                       ExtrapAnaAxis, ExtrapWeightAxis, NDPOT, 0);

  /*ReweightableSpectrum sNDExtrap_280kA = 
    ReweightableSpectrum(ana::Constant(1), fNDFD_Matrix->GetNDExtrap_280kA(),
                         fAnalysisAxisFD.GetLabels(), fAnalysisAxisFD.GetBinnings(),
                         NDPOT, 0);*/
  NDComps.emplace(kNDDataExtrap2D_280kA, sNDExtrap_280kA);
  // 8. Weight extrapolated 280kA ND data by linear combination coeffiecient.
  Comps.emplace(
      kNDDataExtrap_280kA,
      sNDExtrap_280kA.WeightedBy(UnRunPlannedLinearCombination_280kA_r));
  // 9. Get covariance matrix propagated through 280 linear combination.
  //    Only interested in the total covariance matrix, so no need to save a seperate
  //    280kA covariance matrix.
  /*ReweightableSpectrum sCovMat_280kA(ana::Constant(1), fNDFD_Matrix->GetCovMat_280kA(),
                                     fCovarianceAxis.GetLabels(), fCovarianceAxis.GetBinnings(),
                                     NDPOT, 0);*/
  ReweightableSpectrum sCovMat_280kA(std::move(fNDFD_Matrix->GetCovMat_280kA()),
                                     CovAnaAxis, CovWeightAxis, NDPOT, 0);
  sCovMat += sCovMat_280kA;

  // 10. Add weighted and extrapolated 293kA and 280kA samples to get
  //     extrapolated PRISM prediction.
  Comps.emplace(kNDData_FDExtrap, Comps.at(kNDDataExtrap_293kA));
  Comps.at(kNDData_FDExtrap) += Comps.at(kNDDataExtrap_280kA);
  // 11. For adding in MC corrections:
  Comps.emplace(kNDDataCorr_FDExtrap, Comps.at(kNDData_FDExtrap));
  //------------------------------------------------------------
  //------------------------------------------------------------
  // Repeat extrapolation for MC for debugging & 'fake data' studies
  if (NDComps.count(kNDSig_293kA) && fVaryNDFDMCData) {
    
   /* fNDFD_Matrix->ExtrapolateNDtoFD(NDComps.at(kNDSig2D_293kA), NDPOT, 293,
                                    UnRunPlannedLinearCombination_293kA.get(),
                                    calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                    NDSigFlavor, FDSigFlavor, Current::kCC,
                                    NDSigSign, FDSigSign,
                                    fMCEffCorrection->GetNDefficiency(293),
                                    fMCEffCorrection->GetFDefficiency());

    fNDFD_Matrix->ExtrapolateNDtoFD(NDComps.at(kNDSig2D_280kA), NDPOT, 280,
                                    UnRunPlannedLinearCombination_280kA.get(),
                                    calc, (fVaryNDFDMCData ? kNoShift : shift), 
                                    NDSigFlavor, FDSigFlavor, Current::kCC,
                                    NDSigSign, FDSigSign,
                                    fMCEffCorrection->GetNDefficiency(280),
                                    fMCEffCorrection->GetFDefficiency());

    ReweightableSpectrum sNDMCExtrap_293kA =
      ReweightableSpectrum(ana::Constant(1), fNDFD_Matrix->GetNDExtrap_293kA(),
                           fAnalysisAxisFD.GetLabels(), fAnalysisAxisFD.GetBinnings(),
                           NDPOT, 0);
    NDComps.emplace(kNDMCExtrap2D_293kA, sNDMCExtrap_293kA);
    
    Comps.emplace(
      kNDMCExtrap_293kA,
      sNDMCExtrap_293kA.WeightedBy(UnRunPlannedLinearCombination_293kA_r));
    
    ReweightableSpectrum sNDMCExtrap_280kA =
      ReweightableSpectrum(ana::Constant(1), fNDFD_Matrix->GetNDExtrap_280kA(),
                           fAnalysisAxisFD.GetLabels(), fAnalysisAxisFD.GetBinnings(),
                           NDPOT, 0);
    NDComps.emplace(kNDMCExtrap2D_280kA, sNDMCExtrap_280kA);

    Comps.emplace(
      kNDMCExtrap_280kA,
      sNDMCExtrap_280kA.WeightedBy(UnRunPlannedLinearCombination_280kA_r));

    Comps.emplace(kNDMC_FDExtrap, Comps.at(kNDMCExtrap_293kA));
    Comps.at(kNDMC_FDExtrap) += Comps.at(kNDMCExtrap_280kA);
    */
  }
  //----------------------------------------------------------------
  
  // If we have the FD background predictions add them back in.
  // Add variances of background to diagonal of covariance matrix.
  if (fNCCorrection) { // Add in NC background.
    Comps.emplace(kFDNCBkg,
                  FDPrediction->PredictComponentSyst(
                      calc, (fVaryNDFDMCData ? kNoShift : shift), Flavors::kAll, 
                      Current::kNC, Sign::kBoth));
    if (fAxisAgreement) {
      Comps.at(kPRISMPred) += Comps.at(kFDNCBkg);
      Comps.at(kPRISMMC) += Comps.at(kFDNCBkg);
    }
    Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDNCBkg);
    sCovMat += GetDiagonalCovariance(Comps.at(kFDNCBkg), NDPOT, fCovarianceAxis);
    if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDNCBkg);
  }

  if (fWLBCorrection) { // Add in wrong lepton background.
    Comps.emplace(kFDWrongLepBkg,
                  FDPrediction->PredictComponentSyst(
                      calc, (fVaryNDFDMCData ? kNoShift : shift), FDWrongFlavor, 
                      Current::kCC, Sign::kBoth));
    if (fAxisAgreement) {
      Comps.at(kPRISMPred) += Comps.at(kFDWrongLepBkg);
      Comps.at(kPRISMMC) += Comps.at(kFDWrongLepBkg);
    }
    Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDWrongLepBkg);
    sCovMat += GetDiagonalCovariance(Comps.at(kFDWrongLepBkg), NDPOT, fCovarianceAxis);
    if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDWrongLepBkg);
  }

  if (fNuTauCCCorrection) { // Miss-identified CC nu-taus always a background.
    Comps.emplace(kFDNuTauCCBkg,
                  FDPrediction->PredictComponentSyst(
                      calc, (fVaryNDFDMCData ? kNoShift : shift), Flavors::kAllNuTau,
                      Current::kCC, Sign::kBoth));
    if (fAxisAgreement) {
      Comps.at(kPRISMPred) += Comps.at(kFDNuTauCCBkg);
      Comps.at(kPRISMMC) += Comps.at(kFDNuTauCCBkg);
    }
    Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDNuTauCCBkg);
    sCovMat += GetDiagonalCovariance(Comps.at(kFDNuTauCCBkg), NDPOT, fCovarianceAxis);
    if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDNuTauCCBkg);
  }

  if (fWSBCorrection) { // Add in wrong sign background. Nue: Numu->Nue only.
    Comps.emplace(kFDWSBkg,
                  FDPrediction->PredictComponentSyst(
                      calc, (fVaryNDFDMCData ? kNoShift : shift), FDSigFlavor, 
                      Current::kCC, FDWrongSign));
    if (fAxisAgreement) {
      Comps.at(kPRISMPred) += Comps.at(kFDWSBkg);
      Comps.at(kPRISMMC) += Comps.at(kFDWSBkg);
    }
    Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDWSBkg);
    sCovMat += GetDiagonalCovariance(Comps.at(kFDWSBkg), NDPOT, fCovarianceAxis);
    if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDWSBkg);
  }

  if (fIntrinsicCorrection) { // Add in intrinsic correction. Both signs needed!
    Comps.emplace(kFDIntrinsicBkg,
                  FDPrediction->PredictComponentSyst(
                      calc, (fVaryNDFDMCData ? kNoShift : shift), FDIntrinsicFlavor, 
                      Current::kCC, Sign::kBoth));	
    if (fAxisAgreement) {
      Comps.at(kPRISMPred) += Comps.at(kFDIntrinsicBkg);
      Comps.at(kPRISMMC) += Comps.at(kFDIntrinsicBkg);
    }
    Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDIntrinsicBkg);
    sCovMat += GetDiagonalCovariance(Comps.at(kFDIntrinsicBkg), NDPOT, fCovarianceAxis);
    if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDIntrinsicBkg);
  }

  // Always shift FDOsc pred, as this acts as our 'shifted fd data' when 
  // doing fake data shifts
  Comps.emplace(kFDOscPred, 
                FDPrediction->PredictComponentSyst(calc, shift, Flavors::kAll,
                                                   Current::kBoth, Sign::kBoth));
  // Sometimes may want to look just at Numu CC FD prediction, if so, un-comment
  // below and comment-out above.
  //Comps.emplace(kFDOscPred,
  //              FDPrediction->PredictComponentSyst(calc, shift, FDSigFlavor,
  //                                                 Current::kCC, FDSigSign));

  // Get the residual from the event rate/flux matcher.
  std::unique_ptr<TH1> resid(static_cast<TH1 *>(
      fFluxMatcher->GetLastResidual()->Clone("weighted_residual")));
  resid->SetDirectory(nullptr);
  Eigen::ArrayXd resid_arr = GetEigenFlatArray(resid);
  Ratio resid_r(std::move(resid_arr), fAnalysisAxisFD.GetLabels(), 
                fAnalysisAxisFD.GetBinnings());
  // Calculate FD flux miss-matching correction.
  // TODO CHANGE TO RATIO!!
  Comps.emplace(kFDFluxCorr, FDUnOscWeightedSig.WeightedBy(resid_r));
  
  if (fAxisAgreement) Comps.at(kPRISMPred) += Comps.at(kFDFluxCorr);
  
  // At Flux correction to extrapolated PRISM.
  Comps.at(kNDDataCorr_FDExtrap) += Comps.at(kFDFluxCorr);
  //if (Comps.count(kNDMC_FDExtrap)) Comps.at(kNDMC_FDExtrap) += Comps.at(kFDFluxCorr);
  // Final covariance matrix.
  sCovMat += GetDiagonalCovariance(Comps.at(kFDFluxCorr), NDPOT, fCovarianceAxis);
  // Convert final covariance matrix into 2D spectrum
  //Comps.emplace(kExtrapCovarMatrix, sCovMat.ToSpectrum());

  if (NDComps.count(kPRISMMC) && fAxisAgreement) {
    Comps.at(kPRISMMC) += Comps.at(kFDFluxCorr);
  }
 
  for (auto const &NDC :
       NDComps) { // If you haven't been added, project to a 2D spectrum
    if (!Comps.count(NDC.first)) {
      if (NDC.second.GetBinnings().size() < 2) {
        //Comps.emplace(NDC.first, NDC.second.ToSpectrum());
      } else {
        // 2D analysis axis means 3D ND data spectrum
        // Quite annoying to do, and not particularly helpful
      }
    }
  }
  // Try clearing cache to stop overloading memory usage
  //HistCache::ClearCache();

  return Comps;
}

std::map<PredictionPRISM::PRISMComponent, Spectrum>
PredictionPRISM::PredictGaussianFlux(double mean, double width,
                                     ana::SystShifts shift,
                                     BeamChan NDChannel) const {

  // bool WeHaveNDData = HaveNDData(NDChannel);
  // bool WeHaveNDPrediction = HaveNDPrediction(NDChannel);

  // if (!WeHaveNDData || !WeHaveNDPrediction) {
  //   std::cout
  //       << "[ERROR]: Cannot make a PRISM prediction has we have: HaveNDData:
  //       "
  //       << (WeHaveNDData ? "have" : "do not have") << ", WeHaveNDPrediction:
  //       "
  //       << (WeHaveNDPrediction ? "have" : "do not have") << std::endl;
  //   abort();
  // }

  // DontAddDirectory guard;

  // auto &NDData = GetNDData(NDChannel);
  // auto &NDPrediction = GetNDPrediction(NDChannel);

  // // Sort out the flavors and signs
  // auto NDSigFlavor = (NDChannel.chan & NuChan::kNumuNumuBar) ?
  // Flavors::kAllNuMu
  //                                                            :
  //                                                            Flavors::kAllNuE;
  // auto NDSigSign =
  //     ((NDChannel.chan & NuChan::kNumu) || (NDChannel.chan & NuChan::kNue))
  //         ? Sign::kNu
  //         : Sign::kAntiNu;
  // auto NDWrongSign = (NDSigSign == Sign::kNu) ? Sign::kAntiNu : Sign::kNu;
  // auto NDWrongFlavor =
  //     (NDSigFlavor == Flavors::kAllNuMu) ? Flavors::kAllNuE :
  //     Flavors::kAllNuMu;

  // // Using maps for non-default constructible classes is awful...
  // std::map<PredictionPRISM::PRISMComponent, ReweightableSpectrum> NDComps;
  std::map<PredictionPRISM::PRISMComponent, Spectrum> Comps;
  // NDComps.emplace(kNDData, *NDData);
  // NDComps.emplace(kNDDataCorr2D, NDComps.at(kNDData));

  // double NDPOT = NDData->POT();

  // static osc::NoOscillations no;

  // // Start building MC components
  // Spectrum NDSig_spec = NDPrediction->PredictComponentSyst(
  //     &no, shift, NDSigFlavor, Current::kCC, NDSigSign);

  // std::unique_ptr<TH2> NDSig_h(NDSig_spec.ToTH2(NDPOT));

  // ReweightableSpectrum NDSig(ana::Constant(1), NDSig_h.get(),
  //                            fAnalysisAxis.GetLabels(),
  //                            fAnalysisAxis.GetBinnings(), NDPOT, 0);

  // NDComps.emplace(kNDSig2D, NDSig);

  // // ND Background subtraction
  // if (fNCCorrection) {
  //   std::unique_ptr<TH2> NC_h(
  //       NDPrediction
  //           ->PredictComponentSyst(&no, shift, Flavors::kAll, Current::kNC,
  //                                  Sign::kBoth)
  //           .ToTH2(NDPOT));
  //   ReweightableSpectrum NC(ana::Constant(1), NC_h.get(),
  //                           fAnalysisAxis.GetLabels(),
  //                           fAnalysisAxis.GetBinnings(), NDPOT, 0);

  //   NDComps.emplace(kNDNCBkg, NC);
  //   NDComps.at(kNDDataCorr2D) -= NDComps.at(kNDNCBkg);
  // }

  // if (fWLBCorrection) {
  //   std::unique_ptr<TH2> WLB_h(
  //       NDPrediction
  //           ->PredictComponentSyst(&no, shift, NDWrongFlavor, Current::kCC,
  //                                  Sign::kBoth)
  //           .ToTH2(NDPOT));
  //   ReweightableSpectrum Nue(ana::Constant(1), WLB_h.get(),
  //                            fAnalysisAxis.GetLabels(),
  //                            fAnalysisAxis.GetBinnings(), NDPOT, 0);

  //   NDComps.emplace(kNDWrongLepBkg, Nue);
  //   NDComps.at(kNDDataCorr2D) -= NDComps.at(kNDWrongLepBkg);
  // }

  // if (fWSBCorrection) {
  //   std::unique_ptr<TH2> WSB_h(
  //       NDPrediction
  //           ->PredictComponentSyst(&no, shift, NDSigFlavor, Current::kCC,
  //                                  NDWrongSign)
  //           .ToTH2(NDPOT));
  //   ReweightableSpectrum WSB(ana::Constant(1), WSB_h.get(),
  //                            fAnalysisAxis.GetLabels(),
  //                            fAnalysisAxis.GetBinnings(), NDPOT, 0);

  //   NDComps.emplace(kNDWSBkg, WSB);
  //   NDComps.at(kNDDataCorr2D) -= NDComps.at(kNDWSBkg);
  // }

  // // Linear Combination
  // TH1 const *LinearCombination = fFluxMatcher->GetGaussianCoefficients(
  //     mean, width, NDChannel, shift);

  // Comps.emplace(kNDDataCorr,
  //               NDComps.at(kNDDataCorr2D).WeightedByErrors(LinearCombination));
  // Comps.emplace(kPRISMPred, Comps.at(kNDDataCorr));

  // Comps.emplace(kNDSig,
  //               NDComps.at(kNDSig2D).WeightedByErrors(LinearCombination));

  // for (auto const &NDC :
  //      NDComps) { // If you haven't been added, project to a 2D spectrum
  //   if (!Comps.count(NDC.first)) {
  //     Comps.emplace(NDC.first, NDC.second.ToSpectrum());
  //   }
  // }
  return Comps;
}

//----------------------------------------------------------------------

ReweightableSpectrum PredictionPRISM::GetDiagonalCovariance(Spectrum const &spec, 
                                                            double POT,
                                                            HistAxis const &axis) const {
  LabelsAndBins ana_axis(axis.GetLabels().at(0), axis.GetBinnings().at(0));
  LabelsAndBins true_axis(axis.GetLabels().at(1), axis.GetBinnings().at(1));
  //TH1D *spec_h = dynamic_cast<TH1D*>(spec.ToTH1(POT));
  Eigen::ArrayXd spec_arr = spec.GetEigen();
  // Create empty 2D hist - this will be the new covariance matrix.
  //TH2D *Cov_h = HistCache::NewTH2D("CovMat", spec_h->GetXaxis(), spec_h->GetXaxis());
  Eigen::MatrixXd Cov_mat = Eigen::MatrixXd::Zero(spec_arr.size(), spec_arr.size());
  // Set CovMat diagonal to be bin content of spec_h -> Poisson statistics.
  for (int ebin = 0; ebin < spec_arr.size(); ebin++) {
    //Cov_h->SetBinContent(ebin, ebin, std::pow(spec_h->GetBinError(ebin), 2));
    Cov_mat(ebin, ebin) = spec_arr(ebin);
  }

  //ReweightableSpectrum rw(ana::Constant(1), Cov_h, axis.GetLabels(),
  //                        axis.GetBinnings(), POT, 0);
  ReweightableSpectrum rw(std::move(Cov_mat), ana_axis, true_axis, POT, 0);
  //HistCache::Delete(spec_h);
  //HistCache::Delete(Cov_h);

  return rw;
}

//----------------------------------------------------------------------
Spectrum PredictionPRISM::PredictComponent(osc::IOscCalc *calc,
                                           Flavors::Flavors_t flav,
                                           Current::Current_t curr,
                                           Sign::Sign_t sign) const {

  throw;
}

// I know... but makes the SaveTo/LoadFrom a lot neater
using NamedReweightableSpectrumRefVect = std::vector<
    std::pair<std::string,
              std::reference_wrapper<std::unique_ptr<ReweightableSpectrum>>>>;
using NamedPredInterpRefVect = std::vector<std::pair<
    std::string, std::reference_wrapper<std::unique_ptr<PredictionInterp>>>>;
using NamedReweightableSpectrumCRefVect = std::vector<std::pair<
    std::string,
    std::reference_wrapper<std::unique_ptr<ReweightableSpectrum> const>>>;
using NamedPredInterpCRefVect = std::vector<
    std::pair<std::string,
              std::reference_wrapper<std::unique_ptr<PredictionInterp> const>>>;

//----------------------------------------------------------------------
void PredictionPRISM::SaveTo(TDirectory *dir, const std::string& name) const {
  TDirectory *tmp = gDirectory;

  dir = dir->mkdir(name.c_str()); // switch to subdir
  dir->cd();

  TObjString("PredictionPRISM").Write("type");

  for (auto &meas : NamedReweightableSpectrumCRefVect{
           {"M_ND_numu_ccinc_sel_numode",
            Measurements.ND_293kA.numu_ccinc_sel_numode},
           {"M_ND_numubar_ccinc_sel_numode",
            Measurements.ND_293kA.numubar_ccinc_sel_numode},
           {"M_ND_numu_ccinc_sel_nubmode",
            Measurements.ND_293kA.numu_ccinc_sel_nubmode},
           {"M_ND_numubar_ccinc_sel_nubmode",
            Measurements.ND_293kA.numubar_ccinc_sel_nubmode},
           {"M_ND_nue_ccinc_sel_numode",
            Measurements.ND_293kA.nue_ccinc_sel_numode},
           {"M_ND_nuebar_ccinc_sel_nubmode",
            Measurements.ND_293kA.nuebar_ccinc_sel_nubmode},
           {"M_ND_280kA_numu_ccinc_sel_numode",
            Measurements.ND_280kA.numu_ccinc_sel_numode},
           {"M_ND_280kA_numubar_ccinc_sel_numode",
            Measurements.ND_280kA.numubar_ccinc_sel_numode},
           {"M_ND_280kA_numu_ccinc_sel_nubmode",
            Measurements.ND_280kA.numu_ccinc_sel_nubmode},
           {"M_ND_280kA_numubar_ccinc_sel_nubmode",
            Measurements.ND_280kA.numubar_ccinc_sel_nubmode},
           {"M_ND_280kA_nue_ccinc_sel_numode",
            Measurements.ND_280kA.nue_ccinc_sel_numode},
           {"M_ND_280kA_nuebar_ccinc_sel_nubmode",
            Measurements.ND_280kA.nuebar_ccinc_sel_nubmode}}) {
    if (meas.second.get()) {
      //meas.second.get()->OverridePOT(1); // RWSpec does not have this function :(
      meas.second.get()->SaveTo(dir, meas.first.c_str());
    }
  }

  for (auto &meas : NamedPredInterpCRefVect{
           {"P_ND_numu_ccinc_sel_numode",
            Predictions.ND_293kA.numu_ccinc_sel_numode},
           {"P_ND_numubar_ccinc_sel_numode",
            Predictions.ND_293kA.numubar_ccinc_sel_numode},
           {"P_ND_numu_ccinc_sel_nubmode",
            Predictions.ND_293kA.numu_ccinc_sel_nubmode},
           {"P_ND_numubar_ccinc_sel_nubmode",
            Predictions.ND_293kA.numubar_ccinc_sel_nubmode},
           {"P_ND_nue_ccinc_sel_numode",
            Predictions.ND_293kA.nue_ccinc_sel_numode},
           {"P_ND_nuebar_ccinc_sel_nubmode",
            Predictions.ND_293kA.nuebar_ccinc_sel_nubmode},
           {"P_ND_280kA_numu_ccinc_sel_numode",
            Predictions.ND_280kA.numu_ccinc_sel_numode},
           {"P_ND_280kA_numubar_ccinc_sel_numode",
            Predictions.ND_280kA.numubar_ccinc_sel_numode},
           {"P_ND_280kA_numu_ccinc_sel_nubmode",
            Predictions.ND_280kA.numu_ccinc_sel_nubmode},
           {"P_ND_280kA_numubar_ccinc_sel_nubmode",
            Predictions.ND_280kA.numubar_ccinc_sel_nubmode},
           {"P_ND_280kA_nue_ccinc_sel_numode",
            Predictions.ND_280kA.nue_ccinc_sel_numode},
           {"P_ND_280kA_nuebar_ccinc_sel_nubmode",
            Predictions.ND_280kA.nuebar_ccinc_sel_nubmode}}) {
    if (meas.second.get()) {
      //meas.second.get()->GetPredNomAs<PredictionNoOsc>()->OverridePOT(1);
      meas.second.get()->SaveTo(dir, meas.first.c_str());
    }
  }

  for (auto &meas : NamedPredInterpCRefVect{
           {"P_FD_numu_ccinc_sel_numode", Predictions.FD.numu_ccinc_sel_numode},
           {"P_FD_nue_ccinc_sel_numode", Predictions.FD.nue_ccinc_sel_numode},
           {"P_FD_numubar_ccinc_sel_nubmode",
            Predictions.FD.numubar_ccinc_sel_nubmode},
           {"P_FD_nuebar_ccinc_sel_nubmode",
            Predictions.FD.nuebar_ccinc_sel_nubmode}}) {
    if (meas.second.get()) {
      meas.second.get()->SaveTo(dir, meas.first.c_str());
    }
  }

  for (auto &meas : NamedPredInterpCRefVect{
           {"P_FD_numu_ccinc_sel_sig_numode",
            Predictions.FD.numu_ccinc_sel_sig_numode},
           {"P_FD_nue_ccinc_sel_sig_numode",
            Predictions.FD.nue_ccinc_sel_sig_numode},
           {"P_FD_numubar_ccinc_sel_sig_nubmode",
            Predictions.FD.numubar_ccinc_sel_sig_nubmode},
           {"P_FD_nuebar_ccinc_sel_sig_nubmode",
            Predictions.FD.nuebar_ccinc_sel_sig_nubmode},
           {"P_FD_numu_ccinc_sel_sig_apposc_numode",
            Predictions.FD.numu_ccinc_sel_sig_apposc_numode},
           {"P_FD_numubar_ccinc_sel_sig_apposc_nubmode",
            Predictions.FD.numubar_ccinc_sel_sig_apposc_nubmode},
           {"P_FD_nue_ccinc_sel_sig_apposc_numode",
            Predictions.FD.nue_ccinc_sel_sig_apposc_numode},
           {"P_FD_nuebar_ccinc_sel_sig_apposc_nubmode",
            Predictions.FD.nuebar_ccinc_sel_sig_apposc_nubmode}
       }) {
    if (meas.second.get()) {
      meas.second.get()->SaveTo(dir, meas.first.c_str());
    }
  }

  for (unsigned int i = 0; i < fAnalysisAxisND.GetBinnings().size(); ++i) {
    TObjString(fAnalysisAxisND.GetLabels()[i].c_str())  
        .Write(TString::Format("predND_label%d", i).Data());
    std::string str("predND_bins");
    str += std::to_string(i);
    fAnalysisAxisND.GetBinnings()[i].SaveTo( 
        dir, str); 
  }

  for (unsigned int i = 0; i < fAnalysisAxisFD.GetBinnings().size(); ++i) {
    TObjString(fAnalysisAxisFD.GetLabels()[i].c_str())
        .Write(TString::Format("predFD_label%d", i).Data());
    std::string str("predFD_bins");
    str += std::to_string(i);
    fAnalysisAxisFD.GetBinnings()[i].SaveTo(
        dir, str);
  }

  for (unsigned int i = 0; i < fNDOffAxis.GetBinnings().size(); ++i) {
    TObjString(fNDOffAxis.GetLabels()[i].c_str())
        .Write(TString::Format("offaxis_label%d", i).Data());
    std::string str("offaxis_bins"); 
    str += std::to_string(i);
    fNDOffAxis.GetBinnings()[i].SaveTo(
        dir, str);
  }

  for (unsigned int i = 0; i < fND280kAAxis.GetBinnings().size(); ++i) {
    TObjString(fND280kAAxis.GetLabels()[i].c_str())
        .Write(TString::Format("280kAaxis_label%d", i).Data());
    std::string str("280kAaxis_bins");
    str += std::to_string(i);
    fND280kAAxis.GetBinnings()[i].SaveTo(
        dir, str);
  }

  for (unsigned int i = 0; i < fNDFDEnergyMatchAxis.GetBinnings().size(); ++i) {
    TObjString(fNDFDEnergyMatchAxis.GetLabels()[i].c_str())
        .Write(TString::Format("ematch_label%d", i).Data());
    std::string str("ematch_bins");
    str += std::to_string(i);
    fNDFDEnergyMatchAxis.GetBinnings()[i].SaveTo(
        dir, str);
  }

  for (unsigned int i = 0; i < fTrueAnalysisAxis.GetBinnings().size(); ++i) { 
    TObjString(fTrueAnalysisAxis.GetLabels()[i].c_str())
        .Write(TString::Format("predTrue_label%d", i).Data());
    std::string str("predTrue_bins");
    str += std::to_string(i);
    fTrueAnalysisAxis.GetBinnings()[i].SaveTo(
        dir, str);
  }

  tmp->cd();
}

//----------------------------------------------------------------------
std::unique_ptr<PredictionPRISM> PredictionPRISM::LoadFrom(TDirectory *dir, 
                                                           const std::string &name) {

  std::cout << "Loading PredictionPRISM" << std::endl;
  dir = dir->GetDirectory(name.c_str());
  assert(dir);

  std::vector<std::string> predND_labels;
  std::vector<Binning> predND_bins;
  std::vector<Var> predND_dummy_vars;

  for (int i = 0;; ++i) {
    TDirectory *subdir = dir->GetDirectory(TString::Format("predND_bins%d", i));
    if (!subdir) {
      break;
    }
    std::string str("predND_bins");
    str += std::to_string(i);
    predND_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label =
        (TObjString *)dir->Get(TString::Format("predND_label%d", i));
    predND_labels.push_back(label ? label->GetString().Data() : "");
    predND_dummy_vars.push_back(Constant(1));
  }

  std::vector<std::string> predFD_labels;
  std::vector<Binning> predFD_bins; 
  std::vector<Var> predFD_dummy_vars;

  for (int i = 0;; ++i) {  
    TDirectory *subdir = dir->GetDirectory(TString::Format("predFD_bins%d", i));
    if (!subdir) {  
      break; 
    }
    std::string str("predFD_bins");
    str += std::to_string(i);
    predFD_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label = 
        (TObjString *)dir->Get(TString::Format("predFD_label%d", i));
    predFD_labels.push_back(label ? label->GetString().Data() : "");
    predFD_dummy_vars.push_back(Constant(1)); 
  }

  std::vector<std::string> offaxis_labels;
  std::vector<Binning> offaxis_bins;
  std::vector<Var> offaxis_dummy_vars;

  for (int i = 0;; ++i) {
    TDirectory *subdir =
        dir->GetDirectory(TString::Format("offaxis_bins%d", i));
    if (!subdir) {
      break;
    }
    std::string str("offaxis_bins");
    str += std::to_string(i);
    offaxis_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label =
        (TObjString *)dir->Get(TString::Format("offaxis_label%d", i));
    offaxis_labels.push_back(label ? label->GetString().Data() : "");
    offaxis_dummy_vars.push_back(Constant(1));
  }

  std::vector<std::string> _280kAaxis_labels;
  std::vector<Binning> _280kAaxis_bins;
  std::vector<Var> _280kAaxis_dummy_vars;

  for (int i = 0;; ++i) {
    TDirectory *subdir =
        dir->GetDirectory(TString::Format("280kAaxis_bins%d", i));
    if (!subdir) {
      break;
    }
    std::string str("280kAaxis_bins");
    str += std::to_string(i);
    _280kAaxis_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label =
        (TObjString *)dir->Get(TString::Format("280kAaxis_label%d", i));
    _280kAaxis_labels.push_back(label ? label->GetString().Data() : "");
    _280kAaxis_dummy_vars.push_back(Constant(1));
  }

  std::vector<std::string> ematch_labels;
  std::vector<Binning> ematch_bins;
  std::vector<Var> ematch_dummy_vars;

  for (int i = 0;; ++i) {
    TDirectory *subdir = dir->GetDirectory(TString::Format("ematch_bins%d", i));
    if (!subdir) {
      break;
    }
    std::string str("280kAaxis_bins");
    str += std::to_string(i);
    ematch_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label =
        (TObjString *)dir->Get(TString::Format("ematch_label%d", i));
    ematch_labels.push_back(label ? label->GetString().Data() : "");
    ematch_dummy_vars.push_back(Constant(1));
  }

  std::vector<std::string> predTrue_labels;
  std::vector<Binning> predTrue_bins; 
  std::vector<Var> predTrue_dummy_vars;

  for (int i = 0;; ++i) {
    TDirectory *subdir = dir->GetDirectory(TString::Format("predTrue_bins%d", i));
    if (!subdir) {
      break;
    }
    std::string str("predTrue_bins");
    str += std::to_string(i);
    predTrue_bins.push_back(*Binning::LoadFrom(dir, str));
    TObjString *label = 
        (TObjString *)dir->Get(TString::Format("predTrue_label%d", i));
    predTrue_labels.push_back(label ? label->GetString().Data() : "");
    predTrue_dummy_vars.push_back(Constant(1));  
  }

  HistAxis const predictionAxisND(predND_labels, predND_bins, predND_dummy_vars);
  HistAxis const predictionAxisFD(predFD_labels, predFD_bins, predFD_dummy_vars);
  HistAxis const offAxis(offaxis_labels, offaxis_bins, offaxis_dummy_vars);
  HistAxis const _280kAAxis(_280kAaxis_labels, _280kAaxis_bins,
                            _280kAaxis_dummy_vars);
  HistAxis const energyMatchAxis(ematch_labels, ematch_bins, ematch_dummy_vars);
  HistAxis const predictionAxisTrue(predTrue_labels, predTrue_bins, predTrue_dummy_vars);

  std::vector<HistAxis> AxisVec = { predictionAxisFD };
  HistAxis const covarianceAxis = GetMatrixAxis(AxisVec);

  HistAxis const offPredictionAxis = GetTwoDAxis(predictionAxisND, offAxis);
  HistAxis const off280kAPredictionAxis = GetTwoDAxis(predictionAxisND, _280kAAxis);
  HistAxis const fluxMatcherCorrAxis = GetTwoDAxis(predictionAxisFD, energyMatchAxis);

  std::unique_ptr<PredictionPRISM> pred = std::make_unique<PredictionPRISM>(
      predictionAxisND, predictionAxisFD, offAxis, 
      _280kAAxis, predictionAxisTrue, energyMatchAxis, covarianceAxis,
      offPredictionAxis, off280kAPredictionAxis, fluxMatcherCorrAxis);
  // Important new function
  pred->Initialize();

  for (auto &meas : NamedReweightableSpectrumRefVect{
           {"M_ND_numu_ccinc_sel_numode",
            pred->Measurements.ND_293kA.numu_ccinc_sel_numode},
           {"M_ND_numubar_ccinc_sel_numode",
            pred->Measurements.ND_293kA.numubar_ccinc_sel_numode},
           {"M_ND_numu_ccinc_sel_nubmode",
            pred->Measurements.ND_293kA.numu_ccinc_sel_nubmode},
           {"M_ND_numubar_ccinc_sel_nubmode",
            pred->Measurements.ND_293kA.numubar_ccinc_sel_nubmode},
           {"M_ND_nue_ccinc_sel_numode",
            pred->Measurements.ND_293kA.nue_ccinc_sel_numode},
           {"M_ND_nuebar_ccinc_sel_nubmode",
            pred->Measurements.ND_293kA.nuebar_ccinc_sel_nubmode},
           {"M_ND_280kA_numu_ccinc_sel_numode",
            pred->Measurements.ND_280kA.numu_ccinc_sel_numode},
           {"M_ND_280kA_numubar_ccinc_sel_numode",
            pred->Measurements.ND_280kA.numubar_ccinc_sel_numode},
           {"M_ND_280kA_numu_ccinc_sel_nubmode",
            pred->Measurements.ND_280kA.numu_ccinc_sel_nubmode},
           {"M_ND_280kA_numubar_ccinc_sel_nubmode",
            pred->Measurements.ND_280kA.numubar_ccinc_sel_nubmode},
           {"M_ND_280kA_nue_ccinc_sel_numode",
            pred->Measurements.ND_280kA.nue_ccinc_sel_numode},
           {"M_ND_280kA_nuebar_ccinc_sel_nubmode",
            pred->Measurements.ND_280kA.nuebar_ccinc_sel_nubmode}}) {
    if (dir->GetDirectory(meas.first.c_str())) {
      meas.second.get() =
          ReweightableSpectrum::LoadFrom(dir, meas.first.c_str());
          //ReweightableSpectrum::LoadFrom(dir->GetDirectory(meas.first.c_str()));
      //meas.second.get()->OverridePOT(1);
    }
  }

  for (auto &meas : NamedPredInterpRefVect{
           {"P_ND_numu_ccinc_sel_numode",
            pred->Predictions.ND_293kA.numu_ccinc_sel_numode},
           {"P_ND_numubar_ccinc_sel_numode",
            pred->Predictions.ND_293kA.numubar_ccinc_sel_numode},
           {"P_ND_numu_ccinc_sel_nubmode",
            pred->Predictions.ND_293kA.numu_ccinc_sel_nubmode},
           {"P_ND_numubar_ccinc_sel_nubmode",
            pred->Predictions.ND_293kA.numubar_ccinc_sel_nubmode},
           {"P_ND_nue_ccinc_sel_numode",
            pred->Predictions.ND_293kA.nue_ccinc_sel_numode},
           {"P_ND_nuebar_ccinc_sel_nubmode",
            pred->Predictions.ND_293kA.nuebar_ccinc_sel_nubmode},
           {"P_ND_280kA_numu_ccinc_sel_numode",
            pred->Predictions.ND_280kA.numu_ccinc_sel_numode},
           {"P_ND_280kA_numubar_ccinc_sel_numode",
            pred->Predictions.ND_280kA.numubar_ccinc_sel_numode},
           {"P_ND_280kA_numu_ccinc_sel_nubmode",
            pred->Predictions.ND_280kA.numu_ccinc_sel_nubmode},
           {"P_ND_280kA_numubar_ccinc_sel_nubmode",
            pred->Predictions.ND_280kA.numubar_ccinc_sel_nubmode},
           {"P_ND_280kA_nue_ccinc_sel_numode",
            pred->Predictions.ND_280kA.nue_ccinc_sel_numode},
           {"P_ND_280kA_nuebar_ccinc_sel_nubmode",
            pred->Predictions.ND_280kA.nuebar_ccinc_sel_nubmode}}) {
    if (dir->GetDirectory(meas.first.c_str())) {
      meas.second.get() =
          PredictionInterp::LoadFrom(dir, meas.first.c_str());  
          //PredictionInterp::LoadFrom(dir->GetDirectory(meas.first.c_str()));
      //meas.second.get()->GetPredNomAs<PredictionNoOsc>()->OverridePOT(1);
    }
  }

  for (auto &meas : NamedPredInterpRefVect{
           {"P_FD_numu_ccinc_sel_numode",
            pred->Predictions.FD.numu_ccinc_sel_numode},
           {"P_FD_nue_ccinc_sel_numode",
            pred->Predictions.FD.nue_ccinc_sel_numode},
           {"P_FD_numubar_ccinc_sel_nubmode",
            pred->Predictions.FD.numubar_ccinc_sel_nubmode},
           {"P_FD_nuebar_ccinc_sel_nubmode",
            pred->Predictions.FD.nuebar_ccinc_sel_nubmode}}) {
    if (dir->GetDirectory(meas.first.c_str())) {
      meas.second.get() =
          PredictionInterp::LoadFrom(dir, meas.first.c_str());
          //PredictionInterp::LoadFrom(dir->GetDirectory(meas.first.c_str()));
    }
  }

  for (auto &meas : NamedPredInterpRefVect{
           {"P_FD_numu_ccinc_sel_sig_numode",
            pred->Predictions.FD.numu_ccinc_sel_sig_numode},
           {"P_FD_nue_ccinc_sel_sig_numode",
            pred->Predictions.FD.nue_ccinc_sel_sig_numode},
           {"P_FD_numubar_ccinc_sel_sig_nubmode",
            pred->Predictions.FD.numubar_ccinc_sel_sig_nubmode},
           {"P_FD_nuebar_ccinc_sel_sig_nubmode",
            pred->Predictions.FD.nuebar_ccinc_sel_sig_nubmode},
           {"P_FD_numu_ccinc_sel_sig_apposc_numode",
            pred->Predictions.FD.numu_ccinc_sel_sig_apposc_numode},
           {"P_FD_numubar_ccinc_sel_sig_apposc_nubmode",
            pred->Predictions.FD.numubar_ccinc_sel_sig_apposc_nubmode},
           {"P_FD_nue_ccinc_sel_sig_apposc_numode",
            pred->Predictions.FD.nue_ccinc_sel_sig_apposc_numode},
           {"P_FD_nuebar_ccinc_sel_sig_apposc_nubmode",
            pred->Predictions.FD.nuebar_ccinc_sel_sig_apposc_nubmode}
       }) {
    if (dir->GetDirectory(meas.first.c_str())) {
      meas.second.get() =
          PredictionInterp::LoadFrom(dir, meas.first.c_str());
          //PredictionInterp::LoadFrom(dir->GetDirectory(meas.first.c_str()));
    }
  }

  delete dir;

  return pred;
}

} // namespace ana

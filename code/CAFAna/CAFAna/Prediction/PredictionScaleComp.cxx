#include "CAFAna/Prediction/PredictionScaleComp.h"

#include "CAFAna/Core/Cut.h"
#include "CAFAna/Core/HistCache.h"
#include "CAFAna/Core/LoadFromFile.h"
#include "CAFAna/Prediction/PredictionNoOsc.h"
#include "CAFAna/Prediction/PredictionNoExtrap.h"

#include "TDirectory.h"
#include "TObjString.h"
#include "TH1.h"
#include "TVectorD.h"

#include <cassert>

namespace ana
{
  //----------------------------------------------------------------------
  PredictionScaleComp::
  PredictionScaleComp(SpectrumLoaderBase& loader,
                      const HistAxis& axis,
                      Cut cut,
                      const std::vector<const SystComponentScale*>& systs,
                      const SystShifts& shift,
                      const Var& wei)
    : fSysts(systs)
  {
    Cut complementCut = kNoCut;

    assert(!systs.empty() && "Please give at least one systematic.");
    for(const SystComponentScale* syst: systs){
      fPreds.push_back(new PredictionNoOsc(loader, axis,
                                           cut && syst->GetCut(), shift, wei));
      complementCut = complementCut && !syst->GetCut();
    }

    // This is the set of events that didn't wind up in any of the scaleable
    // categories.
    fComplement = new PredictionNoOsc(loader, axis,
                                      cut && complementCut, shift, wei);
  }

  //----------------------------------------------------------------------
  PredictionScaleComp::
  PredictionScaleComp(DUNERunPOTSpectrumLoader& loaderBeam,
                      DUNERunPOTSpectrumLoader& loaderNue,
                      DUNERunPOTSpectrumLoader& loaderNuTau,
                      DUNERunPOTSpectrumLoader& loaderNC,
                      const HistAxis&     axis,
                      Cut                 cut,
                      const std::vector<const SystComponentScale*>& systs,
                      const SystShifts&   shift,
                      const Var&          wei)
    : fSysts(systs)
  {
    Cut complementCut = kNoCut;

    assert(!systs.empty() && "Please give at least one systematic.");
    for(const SystComponentScale* syst: systs){
      fPreds.push_back(new PredictionNoExtrap(loaderBeam, loaderNue, loaderNuTau, loaderNC,
                                              axis, cut && syst->GetCut(), shift, wei));
      complementCut = complementCut && !syst->GetCut();
    }

    // This is the set of events that didn't wind up in any of the scaleable
    // categories.
    fComplement = new PredictionNoExtrap(loaderBeam, loaderNue, loaderNuTau, loaderNC,
                                         axis, cut && complementCut, shift, wei);
  }

  //----------------------------------------------------------------------
  PredictionScaleComp::
  PredictionScaleComp(SpectrumLoaderBase& loader,
                      const HistAxis& axis1,
                      const HistAxis& axis2,
                      Cut cut,
                      const std::vector<const SystComponentScale*>& systs,
                      const SystShifts& shift,
                      const Var& wei)
  {
    assert(0 && "unimplemented");

    // TODO TODO TODO
    /*
    assert(truthcuts.size()>0 && "Please give at least one truth selection.");
    for(unsigned int i = 0; i < truthcuts.size(); ++i){
      fPreds.push_back(new PredictionNoOsc(loader, axis1, axis2,
                                           cut && truthcuts[i], shift, wei));
      fSysts.push_back(new DummyScaleCompSyst(i));
      fComplementCut = fComplementCut && !truthcuts[i];
    }

    // The idea is that if truthcuts are exhaustive, this Spectrum should wind
    // up empty
    fComplement = new Spectrum(loader, axis1, axis2,
                               cut && fComplementCut, shift, wei);
    */
  }

  //----------------------------------------------------------------------
  PredictionScaleComp::
  PredictionScaleComp(const IPrediction* complement,
                      const std::vector<const IPrediction*>& preds,
                      const std::vector<const SystComponentScale*>& systs)
    : fSysts(systs),
      fPreds(preds),
      fComplement(complement)
  {
  }

  //----------------------------------------------------------------------
  PredictionScaleComp::~PredictionScaleComp()
  {
    for(const IPrediction* p: fPreds) delete p;
    delete fComplement;
  }

  //----------------------------------------------------------------------
  Spectrum PredictionScaleComp::Predict(osc::IOscCalculator* calc) const
  {
    Spectrum ret = fComplement->Predict(calc);

    for(const IPrediction* p: fPreds) ret += p->Predict(calc);

    return ret;
  }

  //----------------------------------------------------------------------
  Spectrum PredictionScaleComp::PredictSyst(osc::IOscCalculator* calc,
                                            const SystShifts&    shift) const
  {
    SystShifts shiftClean = shift;
    for(const ISyst* s: fSysts) shiftClean.SetShift(s, 0);

    Spectrum ret = fComplement->PredictSyst(calc, shiftClean);

    for(unsigned int i = 0; i < fPreds.size(); ++i){
      Spectrum si = fPreds[i]->PredictSyst(calc, shiftClean);
      si.Scale(1 + shift.GetShift(fSysts[i]));

      si.Scale(pow(1+fSysts[i]->OneSigmaScale(), shift.GetShift(fSysts[i])));

      ret += si;
    }

    return ret;
  }

  //----------------------------------------------------------------------
  void PredictionScaleComp::SaveTo(TDirectory* dir) const
  {
    TDirectory* tmp = gDirectory;
    dir->cd();

    TObjString("PredictionScaleComp").Write("type");

    fComplement->SaveTo(dir->mkdir("complement"));

    for(unsigned int i = 0; i < fPreds.size(); ++i){
      fPreds[i]->SaveTo(dir->mkdir(("pred"+std::to_string(i)).c_str()));
    }

    for(unsigned int i = 0; i < fSysts.size(); ++i){
      fSysts[i]->SaveTo(dir->mkdir(("syst"+std::to_string(i)).c_str()));
    }

    tmp->cd();
  }

  //----------------------------------------------------------------------
  std::unique_ptr<PredictionScaleComp> PredictionScaleComp::LoadFrom(TDirectory* dir)
  {
    IPrediction* complement = ana::LoadFrom<IPrediction>(dir->GetDirectory("complement")).release();

    std::vector<const IPrediction*> preds;
    for(unsigned int i = 0; ; ++i){
      TDirectory* di = dir->GetDirectory(("pred"+std::to_string(i)).c_str());
      if(!di) break; // We got all the predictions

      preds.push_back(ana::LoadFrom<IPrediction>(di).release());
    }

    std::vector<const SystComponentScale*> systs;
    for(unsigned int i = 0; ; ++i){
      TDirectory* si = dir->GetDirectory(("syst"+std::to_string(i)).c_str());
      if(!si) break; // We got all the predictions

      systs.push_back(ana::LoadFrom<SystComponentScale>(si).release());
    }

    return std::unique_ptr<PredictionScaleComp>(new PredictionScaleComp(complement, preds, systs));
  }

}

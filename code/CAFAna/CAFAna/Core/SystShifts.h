#pragma once

#include "CAFAna/Core/ISyst.h"

#include <map>
#include <string>
#include <vector>

namespace ana
{
  /// Simple record of shifts applied to systematic parameters
  class SystShifts
  {
  public:
    SystShifts();
    SystShifts(const ISyst* syst, double shift);
    SystShifts(const std::map<const ISyst*, double>& shifts);

    static SystShifts Nominal(){return SystShifts();}

    bool IsNominal() const {return fSysts.empty();}

    /// Shifts are 0=nominal, -1,+1 = 1 sigma shifts
    void SetShift(const ISyst* syst, double shift);
    double GetShift(const ISyst* syst) const;
    void ResetToNominal();

    /// Penalty term for chi-squared fits
    double Penalty() const;

    void Shift(Restorer& restore,
               caf::StandardRecord* sr,
               double& weight) const;

    /// Brief description of component shifts, for printing to screen
    std::string ShortName() const;
    /// Long description of component shifts, for plot labels
    std::string LatexName() const;

    /// SystShifts with the same set of systs should have the same ID
    int ID() const {return fID;}

    std::vector<const ISyst*> ActiveSysts() const;

    /// \brief Look up an ISyst in this object from its name
    ///
    /// \return 0 if not found
    const ISyst* SystFromShortName(const std::string& name) const;
  protected:
    std::vector<std::pair<const ISyst*, double>> fSysts;

    int fID;
    /// The next unused ID
    static int fgNextID;
  };

  const SystShifts kNoShift = SystShifts::Nominal();
}

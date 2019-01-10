#pragma once

#include <cassert>
#include "CAFAna/Core/Cut.h"
#include "StandardRecord/StandardRecord.h"

namespace ana
{

  /// \brief Is this a Neutral %Current event?
  ///
  /// We use uniform-initializer syntax to concisely pass the list of necessary
  /// branches. In this case the selection function is simple enough that we
  /// can include it inline as a lambda function.
  const Cut kIsNC({"dune.ccnc"},
                  [](const caf::StandardRecord* sr)
                  {
                    return !sr->dune.isCC;
                  });

  //----------------------------------------------------------------------
  /// Helper for defining true CC event cuts
  class CCFlavSel
  {
  public:
    CCFlavSel(int pdg, int pdgorig) : fPdg(pdg), fPdgOrig(pdgorig)
    {
    }

    bool operator()(const caf::StandardRecord* sr) const
    {
      return sr->dune.isCC && abs(sr->dune.nuPDGunosc) == fPdgOrig && abs(sr->dune.nuPDG) == fPdg;
    }
  protected:
    int fPdg, fPdgOrig;
  };

  // Finally, the function argument to the Cut constructor can be a "functor"
  // object (one with operator()). This allows similar logic but with different
  // constants to be easily duplicated.

  /// Select CC \f$ \nu_\mu\to\nu_e \f$
  const Cut kIsSig    ({}, CCFlavSel(12, 14));
  /// Select CC \f$ \nu_\mu\to\nu_\mu \f$
  const Cut kIsNumuCC ({}, CCFlavSel(14, 14));
  /// Select CC \f$ \nu_e\to\nu_e \f$
  const Cut kIsBeamNue({}, CCFlavSel(12, 12));
  /// Select CC \f$ \nu_e\to\nu_\mu \f$
  const Cut kIsNumuApp({}, CCFlavSel(14, 12));
  /// Select CC \f$ \nu_\mu\to\nu_\tau \f$
  const Cut kIsTauFromMu({}, CCFlavSel(16, 14));
  /// Select CC \f$ \nu_e\to\nu_\tau \f$
  const Cut kIsTauFromE({}, CCFlavSel(16, 12));

  /// Is this truly an antineutrino?
  const Cut kIsAntiNu({},
                      [](const caf::StandardRecord* sr)
                      {
                        return sr->dune.nuPDG < 0;
                      });

  const Cut kIsTrueFV({},
                      [](const caf::StandardRecord* sr)
                      {
                        if (sr->dune.isFD){
                          return ( 
                                  abs(sr->dune.vtx_x) < 310 &&
                                  abs(sr->dune.vtx_y) < 550 &&
                                    sr->dune.vtx_z >  50      &&
                                    sr->dune.vtx_z < 1244 );
                        }
                        else{
                          bool inDeadRegion = false;
                          for( int i = -3; i <= 3; ++i ) {
                            // 0.5cm cathode in the middle of each module, plus 0.5cm buffer
                            double cathode_center = i*102.1;
                            if( sr->dune.vtx_x > cathode_center - 0.75 && sr->dune.vtx_x < cathode_center + 0.75 ) inDeadRegion = true;

                            // 1.6cm dead region between modules (0.5cm module wall and 0.3cm pixel plane, x2)
                            // don't worry about outer boundary because events are only generated in active Ar + insides
                            double module_boundary = i*102.1 + 51.05;
                            if( i <= 2 && sr->dune.vtx_x > module_boundary - 1.3 && sr->dune.vtx_x < module_boundary + 1.3 ) inDeadRegion = true;
                          }
                          for( int i = 1; i <= 4; ++i ) {
                            // module boundaries in z are 1.8cm (0.4cm ArCLight plane + 0.5cm module wall, x2)
                            // module is 102.1cm wide, but only 101.8cm long due to cathode (0.5cm) being absent in length
                            // but ArCLight is 0.1cm thicker than pixel plane so it's 0.3cm difference
                            // positions are off-set by 0.6 because I defined 0 to be the upstream edge based on the active volume
                            // by inspecting a plot, and aparently missed by 3 mm, but whatever
                            // add 8mm = 2 pad buffer due to worse position resolution in spatial dimension z compared to timing direction x
                            // so total FV gap will be 1.8 + 2*0.8 = 3.4cm
                            double module_boundary = i*101.8 - 0.6;
                            if( sr->dune.vtx_z > module_boundary - 1.7 && sr->dune.vtx_z < module_boundary + 1.7 ) inDeadRegion = true;
                          }

                          return (
                                  abs(sr->dune.vtx_x) < 300 &&
                                  abs(sr->dune.vtx_y) < 100 &&
                                  sr->dune.vtx_z > 50 &&
                                  sr->dune.vtx_z < 350 &&
                                  !inDeadRegion
                                  );
                        }
                      });

  //ETW 11/5/2018 Fiducial cut using MVA variable
  //Should use the previous one (kIsTrueFV) for nominal analysis
  const Cut kPassFid_MVA({},
                        [](const caf::StandardRecord* sr)
                        {
                          return ( sr->dune.mvanumu > -1 );
                        });

}

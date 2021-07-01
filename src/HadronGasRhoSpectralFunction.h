#ifndef SRC_HADRONGASRHOSPECTRALFUNCTION
#define SRC_HADRONGASRHOSPECTRALFUNCTION

#include <vector>
#include <memory>

#include "ThermalPhoton.h"
#include "ParameterReader.h"

class HadronGasRhoSpectralFunction : public ThermalPhoton {
 public:
    HadronGasRhoSpectralFunction(std::shared_ptr<ParameterReader> paraRdr_in);
    ~HadronGasRhoSpectralFunction() {}
    void analyticRates(double T, double muB, std::vector<double> &Eq,
                       std::vector<double> &eqrate_ptr);
};

#endif

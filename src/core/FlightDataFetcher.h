#pragma once

#include <Arduino.h>
#include <vector>
#include "BaseStateVectorFetcher.h"
#include "BaseFlightFetcher.h"
#include "StateVector.h"
#include "FlightInfo.h"

class FlightDataFetcher
{
public:
    FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                      BaseFlightFetcher *flightFetcher);

    size_t fetchFlights(std::vector<StateVector> &outStates,
                        std::vector<FlightInfo> &outFlights);

private:
    BaseStateVectorFetcher *_stateFetcher;
    BaseFlightFetcher *_flightFetcher;
};

#pragma once

#include <Arduino.h>
#include <vector>
#include "BaseStateVectorFetcher.h"
#include "BaseFlightFetcher.h"
#include "StateVector.h"
#include "FlightInfo.h"

// Callback invoked between sub-calls to report progress and pump the WebServer.
// phase: short label ("OpenSky", "AeroAPI 3/8", ""); empty string = idle/done.
using FetchProgressCb = void(*)(void *ctx, const char *phase);

class FlightDataFetcher
{
public:
    FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                      BaseFlightFetcher *flightFetcher);

    void setProgressCallback(FetchProgressCb cb, void *ctx) { _cb = cb; _ctx = ctx; }

    size_t fetchFlights(std::vector<StateVector> &outStates,
                        std::vector<FlightInfo> &outFlights);

private:
    BaseStateVectorFetcher *_stateFetcher;
    BaseFlightFetcher      *_flightFetcher;
    FetchProgressCb         _cb  = nullptr;
    void                   *_ctx = nullptr;

    inline void progress(const char *phase) { if (_cb) _cb(_ctx, phase); }
};

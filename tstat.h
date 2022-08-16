// Thermostat controller
//
// D. Robins, 20190127

#pragma once

#include <string>


extern bool g_fDebug;

#define DEBUG(format) if (g_fDebug) std::cout << format << std::endl;


namespace tstat
{

typedef unsigned Pin;
typedef int TempC1000;  // 1000x the temperature in degrees centigrade

enum class Mode
{
        Off,
        Cool,
        Heat,
        Heat2,
        Fan,
};


TempC1000 const tempNil = std::numeric_limits<TempC1000>::min();
TempC1000 const dtempThreshold = 200;
TempC1000 const dtempSmall = 500;


void Thread();
void Stop();
std::string SParseCommand(std::string const &s);

}  // namespace

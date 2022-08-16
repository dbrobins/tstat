// Thermostat controller
//
// D. Robins, 20190127

#include "tstat.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <glob.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment" //TODO: this doesn't actually work
#include "pigpio.h"
#pragma GCC diagnostic pop


namespace tstat
{
class TempController
{
public:
        TempController() {}
        ~TempController();
        bool FInit();
        Mode ModeGet() const { return m_mode; }
        bool FSetRange(TempC1000 temp, TempC1000 tempMin, TempC1000 tempMax);
        unsigned CSameMode() const { return m_cSameMode; }

private:
        // the temp sensor is handled by the w1* modules
        // the relays used are NC, i.e., high opens, low closes
        // experientially:
        // - when emergency/stage 2 heat is on, so is the compressor/stage 1
        // - when heat/cooling is on, the fan is on (even if it may be automatic)
        Pin const pinChange = 17;  // "O/B", open = heat
        Pin const pinComp   = 27;  // "Y", closed = turn on compressor
        Pin const pinEmerg  = 22;  // "W2"/"E", closed = stage 2 heat
        Pin const pinFan    = 10;  // "G", closed = turn on fan

        Mode m_mode = Mode::Off;
        unsigned m_cSameMode = 0;
        TempC1000 m_tempPrev = tempNil;

        bool FSetMode(Mode mode);
        Mode ModeSetTemp(TempC1000 temp, TempC1000 tempMin, bool fHeat);
};


static constexpr TempC1000 TempFromDegf(int degf);

static bool g_fTerm = false;
auto g_tempMin = TempFromDegf(70);
auto g_tempMax = TempFromDegf(75);
auto g_tempPrev = tempNil;
static TempController *g_ptctl;


static bool FHeating(Mode mode)
{
        return mode == Mode::Heat || mode == Mode::Heat2;
}

static char const *SFromMode(Mode mode)
{
        switch (mode)
        {
        case Mode::Off: return "off";
        case Mode::Cool: return "cool";
        case Mode::Heat: return "heat";
        case Mode::Heat2: return "heat2";
        case Mode::Fan: return "fan";
        default: return "<invalid>";
        }
}

static std::ostream &operator<<(std::ostream &os, const Mode &mode)
{
    return os << SFromMode(mode);
}

static std::string SFromTemp(TempC1000 temp)
{
        std::ostringstream oss;
        temp = temp * 9 / 5 + 32000 + 50/*round*/;
        oss << (temp / 1000) << '.' << std::setfill('0') << std::setw(1) <<
                ((temp % 1000) / 100);
        return oss.str();
}

static constexpr TempC1000 TempFromDegf(int degf)
{
        return ((degf - 32) * 5000) / 9;
}


TempController::~TempController()
{
        FSetMode(Mode::Off);
        gpioTerminate();
}

bool TempController::FInit()
{
        gpioCfgSetInternals(gpioCfgGetInternals() | PI_CFG_NOSIGHANDLER);
        if (gpioInitialise() < 0)
                return false;
        for (auto pin : { pinChange, pinComp, pinEmerg, pinFan })
        {
                if (gpioSetMode(pin, PI_OUTPUT) != 0 || gpioWrite(pin, 1) != 0)
                        return false;
        }
        DEBUG("temperature controller initialized");
        return true;
}

bool TempController::FSetMode(Mode mode)
{
        if (mode == m_mode)
        {
                ++m_cSameMode;
                return true;
        }
        DEBUG("setting mode to " << mode << " (was " << m_mode << " " << m_cSameMode << "x)");
        m_cSameMode = 0;

        bool fHeat = true;
        bool fComp = false;
        bool fEmerg = false;
        bool fFan = true;

        switch (mode)
        {
        case Mode::Off:
                fFan = false;
                break;
        case Mode::Cool:
                fHeat = false;
                fComp = true;
                break;
        case Mode::Heat:
                fComp = true;
                break;
        case Mode::Heat2:
                fComp = true;
                fEmerg = true;
        case Mode::Fan:
                break;
        }

        if (gpioWrite(pinChange, fHeat) != 0 ||
                        gpioWrite(pinComp, !fComp) != 0 ||
                        gpioWrite(pinEmerg, !fEmerg) != 0 ||
                        gpioWrite(pinFan, !fFan) != 0)
        {
                return false;
        }
        m_mode = mode;
        return true;
}

Mode TempController::ModeSetTemp(TempC1000 temp, TempC1000 tempWanted, bool fHeat)
{
        auto dtemp = tempWanted - temp;
        auto mode = m_mode;

        // if we're heating, don't run the A/C, and vice versa
        if (fHeat ? (mode == Mode::Cool) : FHeating(mode))
                mode = Mode::Off;

        // do we no longer need to run the HVAC?
        if (mode != Mode::Off)
        {
                if (fHeat ? dtemp <= -dtempThreshold : dtemp >= dtempThreshold)
                        mode = Mode::Off;
                // crank the heat if it falls out of small-adjustment range or tried low too long
                if (fHeat && mode == Mode::Heat && (dtemp > dtempSmall || m_cSameMode > 20))
                        mode = Mode::Heat2;
        }
        // do we need to start running it?
        else {
                if (fHeat)
                {
                        if (dtemp > dtempSmall)
                                mode = Mode::Heat2;
                        else if (dtemp > dtempThreshold)
                                mode = Mode::Heat;
                }
                else if (dtemp < -dtempThreshold)
                {
                        mode = Mode::Cool;
                }
        }
        // otherwise we are good to maintain
        return mode;
}

bool TempController::FSetRange(TempC1000 temp, TempC1000 tempMin, TempC1000 tempMax)
{
        auto mode = ModeSetTemp(temp, tempMin, true/*fHeat*/);
        if (mode == Mode::Off)
                mode = ModeSetTemp(temp, tempMax, false/*fHeat*/);
        if (!FSetMode(mode))
                return false;

        if (m_cSameMode == 0 || m_tempPrev == tempNil || m_tempPrev != temp)
                DEBUG("current temperature " << SFromTemp(temp) << " -> [" <<
                                SFromTemp(tempMin) << ", " << SFromTemp(tempMax) << "]");

        m_tempPrev = temp;
        return true;
}

static TempC1000 TempGet()
{
        static std::string s_pathDev;
        if (s_pathDev.empty())
        {
                glob_t glb;
                if (glob("/sys/bus/w1/devices/28*/w1_slave", GLOB_NOSORT, nullptr, &glb) != 0)
                {
                        std::cerr << "FATAL: can't find temperature sensor\n";
                        abort();
                }
                s_pathDev = glb.gl_pathv[0];
                globfree(&glb);
        }

        std::ifstream ifs(s_pathDev);
        std::string s;
        std::getline(ifs, s);
        if (s.find("YES") == std::string::npos)
        {
                std::cerr << "ERROR: bad result from temperature sensor: " << s << "\n";
                return tempNil;
        }
        std::getline(ifs, s);
        auto ich = s.find("t=");
        if (ich == std::string::npos)
        {
                std::cerr << "ERROR: temperature not found in: " << s << "\n";
                return tempNil;
        }
        try
        {
                return g_tempPrev = std::stoi(s.substr(ich + 2));
        }
        catch (...)
        {
                std::cerr << "ERROR: can't read temperature value: " << s << "\n";
                return tempNil;
        }
}


void Thread()
{
        TempController tctl;
        if (!tctl.FInit())
        {
                std::cerr << "FATAL: GPIO initialization failed\n";
                return;
        }
        g_ptctl = &tctl;

        while (!g_fTerm)
        {
                if (!tctl.FSetRange(TempGet(), g_tempMin, g_tempMax))
                        std::cerr << "ERROR: set temperature failed\n";

                // wait between checks
                gpioSleep(PI_TIME_RELATIVE, 5, 0);
        }

        g_ptctl = nullptr;
}

void Stop()
{
        g_fTerm = true;
}

std::string SParseCommand(std::string const &s)
{
        std::istringstream iss(s);
        iss.unsetf(std::ios_base::basefield);

        std::string sCmd;
        if (!(iss >> sCmd) || sCmd.empty())
                return "bad command";

        std::ostringstream oss;
        if (sCmd == "get")
        {
                oss << "temperature " << SFromTemp(g_tempPrev) << " to [" <<
                                SFromTemp(g_tempMin) << ", " << SFromTemp(g_tempMax) << "]" <<
                                " mode " << (g_ptctl ? g_ptctl->ModeGet() : Mode::Off) <<
                                " " << (g_ptctl ? g_ptctl->CSameMode() : 0) << "x";
        }
        else if (sCmd == "set")
        {
                TempC1000 tempMin;
                if (!(iss >> tempMin))
                        return "set: invalid minimum temperature value";

                TempC1000 tempMax;
                if (!(iss >> tempMax))
                        return "set: invalid maximum temperature value";

                oss << "set target range to [" <<
                                SFromTemp(tempMin) << ", " << SFromTemp(tempMax) << "]";
                g_tempMin = tempMin;
                g_tempMax = tempMax;
        }
        else
        {
                oss << "unknown command: " << sCmd;
        }

        return oss.str();
}

} // namespace

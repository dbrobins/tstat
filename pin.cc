// Pin test
//
// D. Robins, 20190528

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include "pigpio.h"

typedef unsigned Pin;

// tstat.cc is canonical source for pin details
Pin const pinChange = 17;  // "O/B", open = heat
Pin const pinComp   = 27;  // "Y", closed = turn on compressor
Pin const pinEmerg  = 22;  // "W2"/"E", closed = stage 2 heat
Pin const pinFan    = 10;  // "G", closed = turn on fan
// to set a relay to open (no power, V differential), use gpioWrite(pin, true)

bool FFromFlag(char ch)
{
        return ch != '.' && ch != '-';
}

int main(int argc, char const **argv)
{
        if (argc != 2 || strlen(argv[1]) != 4)
        {
                std::cerr << "syntax: " << argv[0] << ".... (./- off else on)" <<
                                " (change on=cool, compressor, fan, emergency)\n";
                exit(1);
        }

        gpioCfgSetInternals(gpioCfgGetInternals() | PI_CFG_NOSIGHANDLER);
        if (gpioInitialise() < 0)
                return 1;
        size_t i = 0;
        for (auto pin : { pinChange, pinComp, pinEmerg, pinFan })
        {
                // relay is negated, i.e., it is closed = gets power if low
                if (gpioSetMode(pin, PI_OUTPUT) != 0 ||
                                gpioWrite(pin, !FFromFlag(argv[1][i++])) != 0)
                        return 1;
        }

#if 0
        gpioWrite(pinChange, false);  // cool
        gpioWrite(pinComp, false);    // compressor on [does not seem to work]
        gpioWrite(pinFan, false);     // fan on

        auto f = false;
        for ( ; ; sleep(1), f = !f)
        {
                std::cout << "switch to " << f << std::endl;
                gpioWrite(pinChange, f);
        }
#endif
        // don't terminate, it clears the pins
        //gpioTerminate();
}

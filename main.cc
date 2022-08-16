// Thermostat controller
//
// D. Robins, 20190126

#include <cassert>
#include <iostream>
#include <set>
#include "Poco/Event.h"
#include "Poco/String.h"
#include "Poco/Thread.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/TCPServer.h"
#include "tstat.h"


bool g_fDebug = true;


class Connection : public Poco::Net::TCPServerConnection
{
public:
        Connection(Poco::Net::StreamSocket const &sock) :
                Poco::Net::TCPServerConnection(sock) {}
        void run() override;
        static void SendAll(std::string const &s);

private:
        static Poco::Mutex s_mx;
        static std::set<Poco::Net::SocketStream *> s_stsocks;
};

class Application : public Poco::Util::ServerApplication
{
public:
        bool FRunning() const { return m_fRunning; }

private:
        void initialize(Poco::Util::Application &app) override;
        void uninitialize() override;
        int main(const std::vector<std::string> &rgsArg) override;

        void Server();

        volatile bool m_fRunning = false;
        Poco::Event m_evtDone;

        Poco::Thread m_thrTstat;
        Poco::Thread m_thrSrv;
};


// Connection

/*static*/ Poco::Mutex Connection::s_mx;
/*static*/ std::set<Poco::Net::SocketStream *> Connection::s_stsocks;

void Connection::run()
{
        Poco::Net::SocketStream socks(socket());
        {
                Poco::ScopedLock _(s_mx);
                s_stsocks.insert(&socks);
        }

        auto const &app = dynamic_cast<Application &>(Poco::Util::Application::instance());

        while (app.FRunning())
        {
                std::string s;
                std::getline(socks, s);
                Poco::trimRightInPlace(s);
                if (s.empty())
                        break;
                socks << tstat::SParseCommand(s) << std::endl;
        }

        {
                Poco::ScopedLock _(s_mx);
                s_stsocks.erase(&socks);
        }
}

/*static*/ void Connection::SendAll(std::string const &s)
{
        Poco::ScopedLock _(s_mx);
        for (auto psocks : s_stsocks)
                *psocks << s;
}

// Application

void Application::initialize(Poco::Util::Application &app)
{
        m_thrTstat.startFunc([]() { tstat::Thread(); });
        m_thrSrv.startFunc([&]() { Server(); });
}

void Application::uninitialize()
{
        tstat::Stop();
        m_thrTstat.join();
        m_fRunning = false;
        m_evtDone.set();
        m_thrSrv.join();
}

void Application::Server()
{
        m_fRunning = true;
        Poco::Net::TCPServer srv(new Poco::Net::TCPServerConnectionFactoryImpl<Connection>(),
                        10000/*port*/);

        srv.start();
        m_evtDone.wait();
        assert(!m_fRunning);
        srv.stop();
}

int Application::main(const std::vector<std::string> &rgsArg)
{
        waitForTerminationRequest();
        return 0;
}


int main(int argc, char **argv)
{
        Application app;
        return app.run(argc, argv);
}

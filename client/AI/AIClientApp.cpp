#include "AIClientApp.h"

#include "../../SDL_net2/fastevents.h"
#include "../../SDL_net2/net2.h"
#include "../../network/Message.h"

#include <boost/lexical_cast.hpp>
#include <log4cpp/Appender.hh>
#include <log4cpp/Category.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/FileAppender.hh>


// static member(s)
AIClientApp*  AIClientApp::s_app = 0;

AIClientApp::AIClientApp(int argc, char* argv[]) : 
   m_log_category(log4cpp::Category::getRoot())
{
   const int CMD_LINE_ARGS = 2; // required number of command-line args (including the client executable file as the first arg)
   
   if (s_app)
      throw std::runtime_error("Attempted to construct a second instance of singleton class AIClientApp");
   
   if (argc != CMD_LINE_ARGS)
      throw std::invalid_argument("Attempted to construct an AIClientApp with the wrong number of command-line arguments");
   
   s_app = this;
   
   // read command line args
   m_player_name = argv[1];
   
   const std::string AICLIENT_LOG_FILENAME(m_player_name + ".log");
   
   // a platform-independent way to erase the old log
   std::ofstream temp(AICLIENT_LOG_FILENAME.c_str());
   temp.close();
   
   // establish debug logging
   log4cpp::Appender* appender = new log4cpp::FileAppender("FileAppender", AICLIENT_LOG_FILENAME);
   log4cpp::PatternLayout* layout = new log4cpp::PatternLayout();
   layout->setConversionPattern("%d %p : %m%n");
   appender->setLayout(layout);
   m_log_category.setAdditivity(false);  // make appender the only appender used...
   m_log_category.setAppender(appender);
   m_log_category.setAdditivity(true);   // ...but allow the addition of others later
   m_log_category.setPriority(log4cpp::Priority::DEBUG);
   m_log_category.debug(m_player_name + " logger initialized.");
}

AIClientApp::~AIClientApp()
{
   m_log_category.debug("Shutting down " + m_player_name + " logger...");
	log4cpp::Category::shutdown();
}

void AIClientApp::operator()()
{
   Run();
}

void AIClientApp::Exit(int code)
{
   Logger().fatalStream() << "Initiating Exit (code " << code << " - " << (code ? "error" : "normal") << " termination)";
   SDLQuit();
   exit(code);
}

log4cpp::Category& AIClientApp::Logger()
{
   return m_log_category;
}

AIClientApp* AIClientApp::GetApp()
{
   return s_app;
}

void AIClientApp::Run()
{
   try {
      SDLInit();
      Initialize();
      while (1)
         Poll();
   } catch (const std::invalid_argument& exception) {
      m_log_category.fatal("std::invalid_argument Exception caught in AIClientApp::Run(): " + std::string(exception.what()));
      Exit(1);
   } catch (const std::runtime_error& exception) {
      m_log_category.fatal("std::runtime_error Exception caught in AIClientApp::Run(): " + std::string(exception.what()));
      Exit(1);
   }
}

void AIClientApp::SDLInit()
{
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
      Logger().errorStream() << "SDL initialization failed: " << SDL_GetError();
      Exit(1);
   }

   if (SDLNet_Init() < 0) {
      Logger().errorStream() << "SDL Net initialization failed: " << SDLNet_GetError();
      Exit(1);
   }
  
   if (FE_Init() < 0) {
      Logger().errorStream() << "FastEvents initialization failed: " << FE_GetError();
      Exit(1);
   }

   if (NET2_Init() < 0) {
      Logger().errorStream() << "SDL Net2 initialization failed: " << NET2_GetError();
      Exit(1);
   }
  
   Logger().debugStream() << "SDLInit() complete.";
}

void AIClientApp::Initialize()
{
   // join game at server
   const int MAX_TRIES = 5;
   int tries = 0;
   bool connected = false;
   while (tries < MAX_TRIES && !connected) {
      connected = m_network_core.ConnectToLocalhostServer();
      ++tries;
   }
   if (!connected) {
      Logger().fatalStream() << "AIClientApp::Initialize : Failed to connect to localhost server after " << MAX_TRIES << " tries.  Exiting.";
      Exit(1);
   }
   m_network_core.SendMessage(JoinGameMessage(m_player_name));
}

void AIClientApp::Poll()
{
   // handle events
   SDL_Event event;
   while (FE_PollEvent(&event)) {
      int net2_type = NET2_GetEventType(&event);
      if (event.type == SDL_USEREVENT && 
          (net2_type == NET2_ERROREVENT || 
           net2_type == NET2_TCPACCEPTEVENT || 
           net2_type == NET2_TCPRECEIVEEVENT || 
           net2_type == NET2_TCPCLOSEEVENT || 
           net2_type == NET2_UDPRECEIVEEVENT)) { // an SDL_net2 event
         m_network_core.HandleNetEvent(event);
      }
   }
}

void AIClientApp::FinalCleanup()
{
}

void AIClientApp::SDLQuit()
{
   FinalCleanup();
   NET2_Quit();
   FE_Quit();
   SDLNet_Quit();
   SDL_Quit();
   Logger().debugStream() << "SDLQuit() complete.";
}

void AIClientApp::HandleMessageImpl(const Message& msg)
{
   switch (msg.Type()) {
   case Message::SERVER_STATUS:
      Logger().debugStream() << "AIClientApp::HandleMessageImpl : Received SERVER_STATUS";
      break;
   case Message::JOIN_GAME:
      if (msg.Sender() == -1 && msg.Receiver() == -1) {
         if (m_player_id == -1) {
            m_player_id = boost::lexical_cast<int>(msg.GetText());
            Logger().debugStream() << "AIClientApp::HandleMessageImpl : Received JOIN_GAME acknowledgement";
         } else {
            Logger().debugStream() << "AIClientApp::HandleMessageImpl : Received erroneous JOIN_GAME acknowledgement when already in a game";
         }
      }
      break;
   }
}


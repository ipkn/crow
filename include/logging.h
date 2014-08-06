#pragma once

#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

#include "settings.h"

using namespace std;

namespace crow
{
	enum class LogLevel
	{
		CRITICAL,
		ERROR,
		WARNING,
		INFO,
		DEBUG
	};

	class ILogHandler {
		public:
			virtual void log(string message, LogLevel level) = 0;
	};

	class CerrLogHandler : public ILogHandler {
		public:
			void log(string message, LogLevel level) override {
				cerr << message;
			}
	};

	class logger {

		private:
			//
			static string timeStamp()
			{
				char date[32];
			  	time_t t = time(0);
			  	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", gmtime(&t));
			  	return string(date);
			}

		public:

			//
			static LogLevel currentLevel;
			static std::shared_ptr<ILogHandler> currentHandler;

			logger(string prefix, LogLevel level) : m_prefix(prefix), m_level(level) {

			}
			~logger() {
	#ifdef CROW_ENABLE_LOGGING
				if(m_level <= currentLevel) {
					ostringstream str;
					str << "(" << timeStamp() << ") [" << m_prefix << "] " << m_stringStream.str() << endl;
					currentHandler->log(str.str(), m_level);
				}
	#endif
			}

			//
			template <typename T>
			logger& operator<<(T const &value) {

	#ifdef CROW_ENABLE_LOGGING
				if(m_level <= currentLevel) {
	    			m_stringStream << value;
	    		}
	#endif
	    		return *this;
			}

			//
			static void setLogLevel(LogLevel level) {
				currentLevel = level;
			}

			static void setHandler(std::shared_ptr<ILogHandler> handler) {
				currentHandler = handler;
			}

		private:

			//
			ostringstream m_stringStream;
			string m_prefix;
			LogLevel m_level;
	};

	//
	LogLevel logger::currentLevel = (LogLevel)CROW_LOG_LEVEL;
	std::shared_ptr<ILogHandler> logger::currentHandler = std::make_shared<CerrLogHandler>();

}

#define CROW_LOG_CRITICAL	crow::logger("CRITICAL", crow::LogLevel::CRITICAL)
#define CROW_LOG_ERROR		crow::logger("ERROR   ", crow::LogLevel::ERROR)
#define CROW_LOG_WARNING	crow::logger("WARNING ", crow::LogLevel::WARNING)
#define CROW_LOG_INFO		crow::logger("INFO    ", crow::LogLevel::INFO)
#define CROW_LOG_DEBUG		crow::logger("DEBUG   ", crow::LogLevel::DEBUG)




#pragma once

#include <string>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

string timeStamp()
{
	char date[32];
  	time_t t = time(0);
  	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", gmtime(&t));
  	return string(date);
}

class logger {

	public:

		//
		enum class Level
		{
			CRITICAL,
			ERROR,
			WARNING,
			INFO,
			DEBUG
		};

		//
		static Level currentLevel;

		logger(string prefix, logger::Level level) : m_prefix(prefix), m_level(level) {

		}
		~logger() {
#ifdef CROW_ENABLE_LOGGING
			if(m_level <= currentLevel) {
				cerr << "(" << timeStamp() << ") [" << m_prefix << "] " << m_stringStream.str() << endl;
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
		static void setLogLevel(logger::Level level) {
			currentLevel = level;
		}

	private:

		//
		ostringstream m_stringStream;
		string m_prefix;
		Level m_level;
};

//
logger::Level logger::currentLevel = (Level)CROW_LOG_LEVEL;

#define CROW_LOG_CRITICAL	logger("CRITICAL", logger::Level::CRITICAL)
#define CROW_LOG_ERROR		logger("ERROR   ", logger::Level::ERROR)
#define CROW_LOG_WARNING	logger("WARNING ", logger::Level::WARNING)
#define CROW_LOG_INFO		logger("INFO    ", logger::Level::INFO)
#define CROW_LOG_DEBUG		logger("DEBUG   ", logger::Level::DEBUG)




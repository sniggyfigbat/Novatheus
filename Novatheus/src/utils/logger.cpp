#include "pch.h"
#include "utils/logger.h"

namespace Utils {
	Logger * Logger::sp_logger;
	
	Logger::Logger() {
		m_logger = spdlog::stdout_color_mt("output");

		spdlog::set_pattern("[%H:%M:%S %z] [%^---%L---%$] [thread %t] %v");
		//spdlog::set_level(spdlog::level::trace);

		m_logger->info("Logger Started.");
	}

	Logger::~Logger() {
		m_logger->info("Logger Shutting Down.");
	}

	Logger * Logger::getSingleton()
	{
		if (sp_logger == nullptr) {
			sp_logger = new Logger();
		}
		
		return sp_logger;
	}
}
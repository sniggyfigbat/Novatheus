#pragma once

#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Utils {
	class Logger {
	private:
		static Logger * sp_logger;
		
		std::shared_ptr<spdlog::logger> m_logger;

		Logger();
		~Logger();
	public:
		static Logger * getSingleton();
		static inline std::shared_ptr<spdlog::logger> & get() { return getSingleton()->m_logger; }
	};
}

#define LOG(...) Utils::Logger::get()->log(__VA_ARGS__)
#define TRACE(...) Utils::Logger::get()->log(spdlog::level::trace, __VA_ARGS__)
#define DEBUGM(...) Utils::Logger::get()->log(spdlog::level::debug, __VA_ARGS__)
#define INFO(...) Utils::Logger::get()->log(spdlog::level::info, __VA_ARGS__)
#define WARN(...) Utils::Logger::get()->log(spdlog::level::warn, __VA_ARGS__)
#define CRITICAL(...) Utils::Logger::get()->log(spdlog::level::critical, __VA_ARGS__)
#define ERRORM(...) Utils::Logger::get()->log(spdlog::level::err, __VA_ARGS__)
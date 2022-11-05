#ifndef _SOURCE_LOGGER_H
#define _SOURCE_LOGGER_H

#include <iostream>
#include <thread>
#include <mutex>

class Logger {
private:
	std::mutex m_mtx;
	std::condition_variable m_cv;
	bool m_locked; //control for spurious wake ups :vomit:

	Logger();
public:
	~Logger();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(Logger&&) = delete;

	static std::unique_ptr<Logger>& GetInstance();

	//attempts to log, returns whether the attempt was successful
	bool [[nodiscard]] TryLog(const std::string& s);

	//yields until able to log and then logs the passed string
	void Log(const std::string& s);

	//calls Logger::Log on a new thread, copies the string when passing to the thread
	void LogAsync(const std::string& s);
};

#endif
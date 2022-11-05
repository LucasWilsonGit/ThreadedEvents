#include "Logger.h"
#include <memory>
#include <sstream>

Logger::Logger() : m_locked(false) {
}

Logger::~Logger() {}

std::unique_ptr<Logger>& Logger::GetInstance() {
	static std::unique_ptr<Logger> ptr(new Logger());
	return ptr;
}

bool Logger::TryLog(const std::string& s) {
	std::unique_lock l(m_mtx, std::defer_lock);
	if (l.try_lock()) {
		std::cout << s << std::endl;
		return true;
	}
	return false;
}

void Logger::Log(const std::string& s) {
	std::unique_lock l(m_mtx);
	std::cout << s << std::endl;
}

void Logger::LogAsync(const std::string& s) {
	std::thread t(&Logger::Log, this, s);
	t.detach();
}
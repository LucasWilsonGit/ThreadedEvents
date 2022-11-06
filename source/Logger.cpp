/*
* <h1>Logger</h1>
* <p>A Thread-Safe minimalistic logging singleton wrapping the STL cout ostream</p>
* Author: Lucas Wilson (LucasWilsonGit)
* Date: 04/11/2022 (DD/MM/YYYY)
* Last Edited: 06/11/2022 (DD/MM/YYYY)
* 
* License: MIT 
* (see LICENSE in rootdir)
* 
* Use note:
* Do not modify any log method to call another log method, the implementation does not use a recursive mutex
*/

#include "Logger.h"
#include <memory>
#include <sstream>

//Use Logger::GetInstance(), don't make a friend class and create new Loggers, they will have a different mutex and output will not be thread-safe.
Logger::Logger() : m_locked(false) {
}

Logger::~Logger() {}

std::unique_ptr<Logger>& Logger::get_instance() {
	static std::unique_ptr<Logger> ptr(new Logger());
	//static initialisation is thread safe since C++11
	//unique pointers do not store state about the heap they point to so it's "fine" to use a unique_ptr constructor (T*) from new T rather than making unique_ptr a friend of 
	//some class with a private ctor
	//(like our Logger) 
	return ptr;
}

//Attempts to log without blocking until the mutex is available, returns whether the string was logged or not 
bool Logger::try_log(const std::string& s) {
	std::unique_lock l(m_mtx, std::defer_lock);
	if (l.try_lock()) {
		std::cout << s << std::endl;
		return true;
	}
	return false;
}

//blocking log (non-busy)
void Logger::log(const std::string& s) {
	std::unique_lock l(m_mtx);
	std::cout << s << std::endl;
}

/*blocking log (busy loop - does not cede to scheduler)
* This can waste CPU cycles and energy. Only use this if scheduler performance is a concern. 
*/
void Logger::busy_log(const std::string& s) {
	while (!try_log(s)) {}
}

//spawns a blocking log in a new thread
void Logger::log_async(const std::string& s) {
	std::thread t(&Logger::log, this, s);
	t.detach();
}
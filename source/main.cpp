#include <iostream>
#include <string>
#include "Event.h"
#include "Logger.h"

using SomeEvent = Event<int>;

int main() {
	std::atomic<uint8_t> e1_run_count(0);

	std::unique_ptr<SomeEvent> e(SomeEvent::create());
	auto& l = e->listen([&](int a) { 
		auto& logger = Logger::get_instance();
		std::stringstream s("");
		s << "Main thread: event happened arg " << a << std::endl;
		logger->log_async(s.str());
		e1_run_count++;
	});

	std::unique_ptr<Event<int>> e2(Event<int>::create());
	auto& l2 = e2->listen([&](int) {
		Logger::get_instance()->log(std::to_string(e1_run_count.load()));
	});

	std::thread t2([&]() {
		e->fire(1);
		Logger::get_instance()->log("after fire 1");
		e->fire(2);
		Logger::get_instance()->log("after fire 2");
		e->fire(3);
		Logger::get_instance()->log("after fire 3");

		e->wait();
		Logger::get_instance()->log("after wait");

		e->fire(4);
		Logger::get_instance()->log("after 4");

		Logger::get_instance()->log("before 5");
		e->fire(5);
		Logger::get_instance()->log("after 5");
	});

	bool run = true;

	std::thread t3([&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		Logger::get_instance()->log("fire 100");
		e->fire(100);

		std::this_thread::sleep_for(std::chrono::seconds(2));
		e2->fire(0);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		run = false;
	});

	while (run) {
		l->poll();
		l2->poll();
	}

	t2.join();
	t3.join();

	if (!l->drop()) {
		Logger::get_instance()->log("Failed to drop listener l");
	}
	l2->drop();

	

	return 0;
}
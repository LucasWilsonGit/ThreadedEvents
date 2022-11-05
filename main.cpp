#include <iostream>
#include <string>
#include "Event.h"
#include "Logger.h"

using SomeEvent = Event<int>;

int main() {
	std::atomic<uint8_t> e1_run_count(0);

	std::unique_ptr<SomeEvent> e(SomeEvent::create());
	auto& l = e->listen([&](int a) { 
		auto& logger = Logger::GetInstance();
		std::stringstream s("");
		s << "Main thread: event happened arg " << a << std::endl;
		logger->LogAsync(s.str());
		e1_run_count++;
	});

	std::unique_ptr<Event<int>> e2(Event<int>::create());
	auto& l2 = e2->listen([&](int) {
		Logger::GetInstance()->Log(std::to_string(e1_run_count.load()));
	});

	std::thread t2([&]() {
		e->fire(1);
		Logger::GetInstance()->Log("after fire 1");
		e->fire(2);
		Logger::GetInstance()->Log("after fire 2");
		e->fire(3);
		Logger::GetInstance()->Log("after fire 3");

		e->wait();
		Logger::GetInstance()->Log("after wait");

		e->fire(4);
		Logger::GetInstance()->Log("after 4");

		Logger::GetInstance()->Log("before 5");
		e->fire(5);
		Logger::GetInstance()->Log("after 5");
	});

	bool run = true;

	std::thread t3([&] {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		Logger::GetInstance()->Log("fire 100");
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
		Logger::GetInstance()->Log("Failed to drop listener l");
	}
	l2->drop();

	e.reset();
	e2.reset();

	while (true) {}

	//std::cout << "listener& l = " << l.get() << " in ptr @" << &l << std::endl;
	/*if (!e->drop(l->get_ID())) {
		Logger::GetInstance()->Log("drop failed");
	}*/
	/*if (!l->drop()) {
		Logger::GetInstance()->Log("drop failed");
	}*/

	

	return 0;
}
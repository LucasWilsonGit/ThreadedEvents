#ifndef _ThreadedEvents_Source_Event_h
#define _ThreadedEvents_Source_Event_h

#include <memory>
#include <functional>
#include <list>
#include <utility>

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>

#include <random>
#include <chrono>
#include <stdexcept>

template <class... T>
class Event;

template <class... ArgTypes>
class Listener {
	using _args_tuple_t = typename std::tuple<ArgTypes...>;
	using _func = typename std::function<void(ArgTypes...)>;

	template <class... args>
	friend class Event;

private:
	static inline std::atomic<uint64_t> s_aID{ 0 };

	const uint64_t m_ID;
	std::vector<_args_tuple_t> m_unprocessed_invocations;
	std::mutex m_mtx;
	Event<ArgTypes...>& m_event;
	_func m_func;
protected:
	Listener(_func&& f, Event<ArgTypes...>& e) : m_ID(s_aID++), m_func(std::move(f)), m_event(e) {

	};
public:
	~Listener() { };

	bool operator==(const Listener& l) const {
		return m_ID = l.m_ID;
	}

	//blocking locks access mutex, invokes callback foreach args tuple in m_unprocessed_invocations, clears vector when finished
	void poll() {
		std::unique_lock lk(m_mtx);
		for (auto& args_tup : m_unprocessed_invocations) {
			std::apply(m_func, std::forward<std::tuple<ArgTypes...>>(args_tup));
		}
		m_unprocessed_invocations.clear();
	};

	bool drop() {
		std::unique_lock lk(m_mtx);
		return m_event.drop(m_ID);
	}

	void enqueue(_args_tuple_t&& t) {
		std::unique_lock lk(m_mtx);
		m_unprocessed_invocations.push_back(std::move(t));
	}

	uint64_t get_ID() const { return m_ID; }
};


template <class... EventArgTypes>
class Event
{
	using _listener = typename Listener<EventArgTypes...>;
	using _listener_ptr = typename std::unique_ptr<_listener>;
private:
	std::list<_listener_ptr> m_listeners;
	std::mutex m_cv_mtx;
	std::mutex m_listeners_mtx;
	std::atomic_uint32_t m_waiting_count;
	std::condition_variable m_wait_cv;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_last_cv_wakeup; //used to calc a bool for spurious checks 
	std::thread::id m_owner_thread_id;
	bool m_wakeup;

protected:
	Event() : m_last_cv_wakeup(std::chrono::high_resolution_clock::now()), m_owner_thread_id(std::this_thread::get_id()), m_waiting_count(0), m_wakeup(false) {
		//m_listeners.reserve(5);
	};

public:
	~Event() {};

	Event(const Event&) = delete; //no copy constructor (implicitly no copy assignment)
	Event& operator=(const Event&) = delete;
	Event(Event&&) = default;
	Event& operator=(Event&&) = default;

	static std::unique_ptr<Event<EventArgTypes...>> create() {

		return std::unique_ptr<Event<EventArgTypes...>>(new Event<EventArgTypes...>());
	};

	void fire(EventArgTypes... args) {
		std::unique_lock<std::mutex> lk(m_listeners_mtx);
		
		for (auto& l : m_listeners) {
			l->enqueue(
				std::forward_as_tuple(args...)
			);//respect type of args to construct the tuple, tuple object itself is built with (new) because I don't want it to be lifetimed, I will clean these up anyway
		}

		{
			std::unique_lock lk2(m_cv_mtx);
			m_wakeup = true;
		}
		

		m_last_cv_wakeup = std::chrono::high_resolution_clock::now();
		m_wait_cv.notify_all();

		while (m_waiting_count > 0) {} //maybe I could implement some mutex so this wasn't a busy wait for the semaphore but I think that would be slow - Lucas

		std::unique_lock lk2(m_cv_mtx);
		m_wakeup = false;
	};

	_listener_ptr& listen(std::function<void(EventArgTypes...)>&& callback) {
		std::unique_lock lk(m_listeners_mtx);

		m_listeners.push_back(
			_listener_ptr(
				new _listener(
					std::move(callback), 
					*this
				) 
			)
		);

		return m_listeners.back(); //for some reason this is trying to make a copy when compiled
	};

	//returns whether the listener was succesfully dropped from the event
	[[nodiscard]] bool drop(uint64_t listener_ID) {
		if ((*m_listeners.begin())->get_ID() == listener_ID) {
			m_listeners.erase(m_listeners.begin());

			return true;
		}

		for (auto it = m_listeners.begin(); it != m_listeners.end(); it++) {
			if ((*it)->get_ID() == listener_ID) {
				m_listeners.erase(it--);

				return true;
			}
		}

		return false;
	};

	//yields the thread until it is woken up by the event being fired
	void wait() {
		std::unique_lock lk(m_cv_mtx);
		m_waiting_count++; //increment the waiting count of the event (number of threads waiting for the event to fire)
		m_wait_cv.wait(lk, [&, this] { return m_wakeup; });
		m_waiting_count--;  //decrement the waiting count

		//waiting count is used in the fire method for a semaphore to control the predicate boolean m_wakeup
		//see Event<T...>::fire(T...) 
		
	}
};

#endif
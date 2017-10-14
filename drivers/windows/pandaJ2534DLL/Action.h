#pragma once
#include <memory>

#include "J2534Frame.h"

class J2534Connection;

class Action
{
public:
	Action(
		std::weak_ptr<J2534Connection> connection,
		std::chrono::microseconds delay
	) : connection(connection), delay(delay) { };

	Action(
		std::weak_ptr<J2534Connection> connection
	) : connection(connection), delay(std::chrono::microseconds(0)) { };

	virtual void execute() = 0;

	void scheduleImmediate() {
		expire = std::chrono::steady_clock::now();
	}

	void scheduleImmediateDelay() {
		expire = std::chrono::steady_clock::now() + this->delay;
	}

	void schedule(std::chrono::time_point<std::chrono::steady_clock> starttine, BOOL adddelayed) {
		this->expire = starttine;
		if (adddelayed)
			expire += this->delay;
	}

	std::weak_ptr<J2534Connection> connection;
	std::chrono::microseconds delay;
	std::chrono::time_point<std::chrono::steady_clock> expire;
};

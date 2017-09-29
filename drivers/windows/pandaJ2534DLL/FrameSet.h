#pragma once
#include <memory>
#include "synchronize.h"
#include "J2534MessageFilter.h"

class J2534MessageFilter;

class FrameSet
{
public:
	//Critical section will be required if accessed outside of processMessage
	FrameSet();

	static std::shared_ptr<FrameSet> init_tx(std::string& payload, std::weak_ptr<J2534MessageFilter> filter);

	static std::shared_ptr<FrameSet> init_rx_first_frame(uint16_t final_size, const std::string& piece, unsigned long rxFlags);

	bool rx_add_frame(uint8_t pci_byte, unsigned int max_packet_size, const std::string piece);

	void reset();

	unsigned int bytes_remaining();

	bool FrameSet::flush_result(std::string& final_msg);

	void lock() {
		access_lock.lock();
	}

	void unlock() {
		access_lock.unlock();
	}

	std::weak_ptr<J2534MessageFilter> filter;

private:
	Mutex access_lock;
	bool istx;
	std::string msg;
	unsigned char next_part;

	unsigned long expected_size;
	unsigned long flags;

	unsigned long consumed_count;
};

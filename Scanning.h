#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
//declare class
class Scanning
{
	//everything in class is public
public:
	//struct named ports containing an int, a bool, and a string
	struct ports {
		int port;
		bool open;
		std::string service;
	};
	//condition variable created for use in Scanning.cpp
	std::condition_variable cv;
	//mutex for changing variables to prevent race conditions
	std::mutex wait_mutex;
	std::mutex insert_vector_mutex;
	std::mutex string_insert;
	//atomic integer for counting number of open ports
	std::atomic<int> open_ports_num = 0;
	//constructor definition
	Scanning();
	//function definitions
	bool one_port_open(std::string address, int port);
	std::vector<ports> port_open_sequential(std::string address, int start, int end); 
	std::vector<ports> port_open_parallel_1000(std::string address);
	std::vector<ports> service_detection_parallel(std::vector<ports> results);
	std::string service_detection(int port);
	std::vector<ports> sort(std::vector<ports> results);
};


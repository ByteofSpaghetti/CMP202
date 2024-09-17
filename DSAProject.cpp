#include "Scanning.h"
//include libraries
#include <iostream>
#include <chrono>
#include <regex>

int main() {
    //regex taken form https://www.geeksforgeeks.org/how-to-validate-an-ip-address-using-regex/ in order to validate IP address
    std::regex IP("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
    //scanner object for accessing scanning classs
    Scanning scanner;
    //initalise variables
    std::string address = "AHHHH";
    int port = 0;
    int input = 0;
    //asks for user input to input a IP address for scanning
    std::cout << "-----------------------------------------------------------" << std::endl;
    std::cout << "Please enter address for scan: ";
    std::cin >> address;
    //while the address does not fit the regex, ask for new address.
    while (!regex_match(address, IP)) {
        std::cout << "Not valid address, please enter address for scan: ";
        std::cin >> address;
    }
    std::cout << "-----------------------------------------------------------" << std::endl;
    //gives option on which port scan to peform
    std::cout << "Enter 1 to scan 1 port, and 2 to scan the first 1000 ports: ";
    std::cin >> input;
    std::cout << "-----------------------------------------------------------" << std::endl;
    //if 1 port is scanned, user is asked to enter a valid port number
    if (input == 1) {
        std::cout << "input port: " << std::endl;
        std::cin >> port;
        std::cout << "-----------------------------------------------------------" << std::endl;
        //bool variable to hold result
        bool bool_result = scanner.one_port_open(address, port);
        //if bool is true port is open
        if (bool_result) {
            std::cout << "port " << port << " open" << std::endl;
        }
        //else port is closed
        else {
            std::cout << "port " << port << " Closed or inaccessible" << std::endl;
        }
    }

    //else if input is 2 first thousand ports are scanned
    else if (input == 2) {
        //message written to console
        std::cout << "Parellel scanning the first thousand ports happening now!... " << std::endl;
        //clock starts
        auto start = std::chrono::high_resolution_clock::now();
        //parallel scanning called with address as parameter 
        std::vector<Scanning::ports> result_parallel = scanner.port_open_parallel_1000(address);
        //end timer
        auto end = std::chrono::high_resolution_clock::now();
        //works out time
        auto par_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        //outputs time taken
        std::cout << "Parallel time taken: " << par_time << std::endl;
        //writes to console
        std::cout << "Parallel scanning results: " << std::endl;
        //sort function called to sort through the results
        std::vector<Scanning::ports> sorted_parallel = scanner.sort(result_parallel);
        //for loop to go through and output the open and closed ports
        for (int i = 0; i < sorted_parallel.size(); i++) {
            //if port is open, port number and service is written out
            if (sorted_parallel[i].open) {
                std::cout << "Port " << sorted_parallel[i].port << " " << "Open " << sorted_parallel[i].service << std::endl; //this doesnt? cant have bools= true just bool or !
            }
            //else port is closed and written out with port number and status
            else {
                std::cout << "Port " << sorted_parallel[i].port << " " << "Closed" << std::endl;
            }
        }
        //writes out atomic variable of number of ports
        std::cout << "Number of open ports: " << scanner.open_ports_num << std::endl;
        //outputs message (as this part will take roughly a half hour-40 minutes)
        std::cout << "Scanning first thousand ports... (dear marker, this part will take forever, to demonstrate that the parallel version is better!)" << std::endl;
        //resets atomic integer open ports variable
        scanner.open_ports_num = 0;
        //starts timer
        start = std::chrono::high_resolution_clock::now();
        //calls sequential port scanning function and stores it in result_sequential variable
        std::vector<Scanning::ports> result_sequential = scanner.port_open_sequential(address, 1, 1000); //FIRST 1000
        //ends timer
        end = std::chrono::high_resolution_clock::now();
        //works out sequential time taken in nanoseconds
        auto seq_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        //writes to terminal
        std::cout << "Sequnetial time taken: " << seq_time << std::endl;
        //sequential scanning results
        std::cout << "Sequential scanning results: " << std::endl;
        //for loop to output scan results
        for (int i = 0; i < result_sequential.size(); i++) {
            //if port is open outputs port number and state
            if (result_sequential[i].open) {
                std::cout << "Port " << result_sequential[i].port << " " << "Open" << std::endl;
            }
            //else outputs that port is closed
            else {
                std::cout << "Port " << result_sequential[i].port << " " << "Closed" << std::endl;
            }
        }
        //outputs number of open ports
        std::cout << "Number of open ports: " << scanner.open_ports_num << std::endl;
        std::cout << "-----------------------------------------------------------" << std::endl;


    }
    //else outputs error message if wrong input is entered and program ends
    else {
        std::cout << "Error, wrong number entered" << std::endl;
    }
    return 0;
}




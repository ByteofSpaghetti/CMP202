#include "Scanning.h"
#include <thread>
#include <condition_variable>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
/*new simpler idea without pools, just ask client if they want to scan first 10, 100, or 1000 and maybe all after done. */
#pragma comment(lib, "ws2_32.lib")
//use different objects for each thread??!! to scan different hosts together and shit? 
/* so for service detection, a function to check which ports are open, takes vector, then just threading to check those ports maybe one by one.*/
//default constructor
Scanning::Scanning() {
}

//bool function to return state of 1 port
bool Scanning::one_port_open(std::string address, int port) {
    //type struct containing infomration about windows sockets (initialises winsock library)
	WSADATA data; 
    //Create and configure socket 
    struct sockaddr_in server_address;
    //Using IPv4
    server_address.sin_family = AF_INET;

    //checks if WSA started correctly, if not outputs error message and returns false
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cout << "WSAStartup failed" << std::endl;
        return false;
    }
    //Socket that function creates socket with IPv4, 2 way connection using TCP, last paramter just means it uses TCP
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //tests if variable sockfd is valid or not
    if (sockfd == INVALID_SOCKET) {
        std::cout << "error in opening socket" << std::endl;
       WSACleanup(); //uncomment for threaded as ends for all threads
        return false;
    }

    //function to convert IPv4 address to numeric binary form , taking family(AF_INET), 
    //next paramater converts it into a c style string needed for future use, next paramter is a pointer to where the IP address in binary formaat will be stored    
    inet_pton(AF_INET, address.c_str(), &server_address.sin_addr); 
    //converts from int to TCP network byte order
    server_address.sin_port = htons(port);
        //int result variable holding value returned from connect function
    int result = connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address));
    //if result has socket error returns false as port is closed
    if (result == SOCKET_ERROR) {
        closesocket(sockfd);
        WSACleanup();
        return false;
    }

    //closes socket
    closesocket(sockfd);
    //returns true as socket is open
    return true;

}

//function for sequentially scanning ports, taking address, start and end integers as variables
std::vector<Scanning::ports> Scanning::port_open_sequential(std::string address, int start, int end) {
    //vector of ports struct as defined in header
    std::vector<ports> sequential_results;
    //type struct containing infomration about windows sockets (initialises winsock library)
    WSADATA data;  
    //Create and configure socket 
    struct sockaddr_in server_address;
    //Using IPv4
    server_address.sin_family = AF_INET;
    //checks that winsock was innitialised without errors
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cout << "WSAStartup failed" << std::endl;
    }
    //for loop using u_short variable as i variable is used for port
    for (u_short i = start; i <= end; i++) {
        //Socket that function creates socket with IPv4, 2 way connection using TCP, last paramter just means it uses TCP
        SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        //if error in socket fucnition exits from program and displays error message
         if (sockfd == INVALID_SOCKET) {
          std::cout << "error in opening socket" << std::endl;
          WSACleanup(); //uncomment for threaded as ends for all threads
          }

         //function to convert IPv4 address to numeric binary form , taking family(AF_INET), 
         //next paramater converts it into a c style string needed for future use, next paramter is a pointer to where the IP address in binary formaat will be stored 
        inet_pton(AF_INET, address.c_str(), &server_address.sin_addr);

        //converts from int to TCP network byte order
        server_address.sin_port = htons(i);
        //result variable to hold connection status
        int result = connect(sockfd, (struct sockaddr*)&server_address, sizeof(server_address));
        //if port is closed, pts the port number and status of port number into sequential_results vector
        if (result == SOCKET_ERROR)
        {
          sequential_results.push_back({ i,false });
        }
        //else puts port number and status in sequential_results and increments open_ports_num atomic variable
        else {
            sequential_results.push_back({ i,true });
            open_ports_num++;
        }
        //closes socket and loops again
         closesocket(sockfd);
         //for loop to repeat!
    }
    //returns sequential results vector
    return sequential_results; 
}

std::vector<Scanning::ports> Scanning::port_open_parallel_1000(std::string address) {
    open_ports_num = 0; //resets open port atomic counter
    std::vector<ports> open_ports; //vector to contain results
    std::vector<std::thread> threads;//vector of threads 

    int start_thread = 1; // varaibles for multithreading
    int end_thread = 10;
     for (int i = 0; i <= 100; i++) { //changed to equals sign 
         if (end_thread > 1000) { //checks if port is exceeded 
             break;//breaks if scanning more ports than specified
         }
         //lambda function to capture specific variables and do the actual parellisation of the program
         threads.emplace_back([this, address, start_thread, end_thread, &open_ports]() { //bit in th [] captures all varaibles captures everything

             //initialises and adds results to vector called thread_results
             std::vector<ports> thread_results = port_open_sequential(address, start_thread, end_thread); 

    {//mutex lock to prevent multiple inserts to the same vector
        insert_vector_mutex.lock();
        //inserts results from thread_results into open_ports vector
        open_ports.insert(open_ports.end(), thread_results.begin(), thread_results.end()); //inserts into vector called open_ports
        //mutex unlock
        insert_vector_mutex.unlock();
    }
            });
            //incrememnts variables
            start_thread = end_thread;
            end_thread = end_thread + 10;

     }
     //joins all the threads
          for (auto& thread : threads) {
             thread.join();
          }
          //if open ports num variable is above 0, begins service detection
        if (open_ports_num > 0) {
            //notifys service detection function
            cv.notify_one();
            //calls service detection function
            std::vector<ports> new_open_ports = service_detection_parallel(open_ports);
            //returns new vector
            return new_open_ports;

        }
        //else returns old vector
        else {
            return open_ports;
        }

}

 //sort function
std::vector<Scanning::ports> Scanning::sort(std::vector<ports> open_ports) {
    //initalise variables for swapping
    int pos = 0;
    int temp = 0;
    bool bool_temp = false;
    std::string service_temp;
    //selection sort iterates sorting the vector from lowest to highest, swapping each variable in the ports structure
    for (int i = 0; i < open_ports.size(); i++) {
        pos = i;

        for (int k = i + 1; k < open_ports.size(); k++) {
            if (open_ports[k].port < open_ports[pos].port) {
                pos = k;
            }
        }
        temp = open_ports[pos].port;
        bool_temp = open_ports[pos].open;
        service_temp = open_ports[pos].service;
        open_ports[pos].port = open_ports[i].port;
        open_ports[pos].open = open_ports[i].open;
        open_ports[pos].service = open_ports[i].service;
        open_ports[i].port = temp;
        open_ports[i].open = bool_temp;
        open_ports[i].service = service_temp;
    }
    return open_ports;
}

//service detection fucntion taking 1 port as an argument
 std::string Scanning::service_detection(int port) {
     //type struct containing infomration about windows sockets (initialises winsock library)
     WSADATA data;
     //if winsock initalisation failes outputs error message
     if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
         std::cout << "WSAStartup failed" << std::endl;
     }
     
     // Perform service lookup taking port and protocl used as arguments
     struct servent* service_entry = getservbyport(htons(port), "tcp");
     //if something was returned by previous function return name of service
     if (service_entry != nullptr) {
         return service_entry->s_name;
     }
     //calls cleanup funciton
     WSACleanup();
     //else returns unknown service
     return "Unknown Service";
 }


 std::vector<Scanning::ports> Scanning::service_detection_parallel(std::vector<ports> results) {
     //creates thread funciton
     std::vector<std::thread> threads;
     // Lock the mutex before checking the condition
     std::unique_lock<std::mutex> lock(wait_mutex);
     cv.wait(lock, [this] { return open_ports_num > 0; }); // Wait until open_ports_num is greater than 0
     lock.unlock(); // Unlock the mutex after condition is met

     //for loop that iterates through the size of the vector
     for (int i = 0; i < results.size(); i++) {
         //if the port is open
         if (results[i].open) {
             //lambda function is used to call service detection function with corresponding port
             threads.emplace_back([this, &results, i]() {
                 std::string service = service_detection(results[i].port);
                 //mutex to lock to insert service to vector
                 string_insert.lock();
                 results[i].service = service;
                 //mutex unlock
                 string_insert.unlock();
                 });
         }
     }
     //joins all threads
     for (auto& thread : threads) {
         thread.join();
     }
     //returns results
     return results;
 }

#include <iostream>
#include <vector>
#include <map>
#include <cmath>
#include <span>
#include <fstream>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <iomanip>
#include <locale>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <regex>
#include <fstream> // For logging
#include "./DNS/DNSMessage.h"
#include "./DNS/DNSDomainName.h"
#include "./DNS/DNSHeader.h"
#include "./DNS/DNSQuestion.h"
#include "./DNS/DNSResourceRecord.h"
#include <fstream>

#define TESTING false
#define BACKLOG 10
#define MAXDATASIZE 1000000
#define MAX_CLIENTS 30 // Maximum number of clients

std::string log_file_name;
std::map<std::string, std::vector<int>> cache;
std::ofstream myfile("miProxy.txt");
std::ofstream logfile;

void sigchld_handler(int s)
{
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

bool validateIpAddress(const std::string &ipAddress)
{
    for (int i = 0; i < ipAddress.size(); i++)
    {
        if (!isdigit(ipAddress[i]) && ipAddress[i] != '.')
        {
            return false;
        }
    }
    return true;
}

template <class T>
std::string FormatWithCommas(T value)
{
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_content_length(const std::string &header)
{
    std::regex content_length_regex(R"(Content-Length:\s*(\d+))");
    std::smatch match;
    if (std::regex_search(header, match, content_length_regex))
    {
        return std::stoi(match[1]);
    }
    return -1; // If not found
}

int get_end_header(std::string header)
{
    std::regex content_length_regex("\r\n\r\n");
    std::smatch match;
    if (std::regex_search(header, match, content_length_regex))
    {
        return std::stoi(match[1]);
    }
    return -1; // If not found
}

std::string query_nameserver(std::string domain, std::string proxy_host, std::string proxy_port, std::string nameserver_ip, std::string nameserver_port)
{
    // todo
    int sockfd, newfd;
    fd_set master_fds, read_fds;
    int fdmax;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int numbytes;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(proxy_host.c_str(), proxy_port.c_str(), &hints, &servinfo) != 0)
    {
        perror("getaddrinfo error");
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    DNSMessage msg = DNSMessage();

    msg.header.ID = rand() % 65535;
    msg.header.QR = 0;
    msg.header.OPCODE = DNSOpcode::QUERY;
    msg.header.AA = 0;
    msg.header.TC = 0;
    msg.header.RD = 0;
    msg.header.RA = 0;
    msg.header.Z = 0;
    msg.header.RCODE = DNSRcode::NO_ERROR;
    msg.header.QDCOUNT = 1;
    msg.header.ANCOUNT = 0;
    msg.header.NSCOUNT = 0;
    msg.header.ARCOUNT = 0;
    msg.question.QCLASS = DNSQClass::IN;
    msg.question.QNAME = msg.question.QNAME.fromString(domain);
    msg.question.QTYPE = DNSQType::A;
    std::vector<std::byte> serialized_msg = msg.serialize();

    struct sockaddr_in DNS_addr;
    memset(&DNS_addr, 0, sizeof(DNS_addr));
    DNS_addr.sin_family = AF_INET;
    DNS_addr.sin_port = htons(std::stoi(nameserver_port)); // Port number to send to
    inet_pton(AF_INET, nameserver_ip.c_str(), &DNS_addr.sin_addr);

    numbytes = sendto(sockfd, serialized_msg.data(), serialized_msg.size(), 0, (sockaddr *)&DNS_addr, sizeof(DNS_addr));
    if (numbytes == -1)
    {
        perror("sending to nameserver");
        exit(1);
    }
    myfile << "sent: " << numbytes << std::endl;

    char buffer[2000];
    socklen_t DNS_addr_len = sizeof DNS_addr;

    myfile << "receiving..." << std::endl;

    numbytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&DNS_addr, &DNS_addr_len);
    if (numbytes == -1)
    {
        perror("receiving from nameserver");
        exit(1);
    }
    myfile << "received: " << numbytes << std::endl;

    buffer[numbytes] = '\0';

    std::vector<std::byte> v(
        reinterpret_cast<std::byte *>(std::begin(buffer)),
        reinterpret_cast<std::byte *>(&buffer[numbytes]));

    DNSMessage response = DNSMessage::deserialize(std::span<const std::byte>(v));

    myfile << "1" << std::endl;
    if (response.header.RCODE != DNSRcode::NO_ERROR)
    {
        perror("RCODE from nameserver");
        exit(1);
    }
    myfile << "2" << std::endl;

    std::string result;
    myfile << sizeof response.answers[0].RDATA << std::endl;
    try
    {
        auto address_obj = std::get<0>(response.answers[0].RDATA);
        result = address_obj.toString();
    }
    catch (...)
    {
        perror("std::get of variant");
        myfile << "triggered" << std::endl;
        exit(1);
    }
    myfile << result << std::endl;
    close(sockfd);

    return result;
}

void log_chunk(const std::string &browser_ip, const std::string &chunkname, const std::string &server_ip,
               double duration, double tput, double avg_tput, int bitrate)
{
    logfile << browser_ip << " " << chunkname << " " << server_ip << " "
            << duration << " " << tput << " " << avg_tput << " " << bitrate << std::endl;
}

void serverprocess(int clientfd, std::string ip, std::string port, const std::string &browser_ip, std::string adaptation_gain, std::string adaptation_bitrate_multiplier, std::string log_file_name)
{
    char buf[MAXDATASIZE];
    fd_set read_fds;
    FD_ZERO(&read_fds);

    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(std::stoi(port));
    server_address.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(serverfd, (sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        std::cerr << "Connection to the server failed" << std::endl;
        exit(1);
    }
    int chunk = 1;
    long T_cur = 0;
    std::vector<int> available_bitrates;
    std::vector<int> qualities;
    std::chrono::steady_clock::time_point start_time;
    bool isChunk = false;
    int manifests = 0;
    std::string clientrequest = "";
    int selected_bitrate = 0;

    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(clientfd, &read_fds);
        FD_SET(serverfd, &read_fds);
        int max_fd = std::max(clientfd, serverfd);
        std::string selected_quality = "";

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR)
        {
            perror("select error");
            break;
        }

        if (FD_ISSET(clientfd, &read_fds))
        {
            memset(buf, 0, MAXDATASIZE);
            int bytesReceived = recv(clientfd, buf, sizeof(buf), 0);

            if (bytesReceived <= 0)
            {
                perror("client");
                break;
            }
            clientrequest = buf;

            std::string request(buf, static_cast<std::string::size_type>(bytesReceived));

            // checks for chunks
            // coutfile << "T_cur " << T_cur << std::endl;
            if (request.find("p_") != std::string::npos && T_cur != 0)
            {
                isChunk = true;
                if (available_bitrates.empty())
                {
                    perror("bitrates empty");
                    return;
                }
                selected_quality = "";
                selected_bitrate = 0;

                for (int i = available_bitrates.size() - 1; i >= 0; i--)
                {
                    if (T_cur >= available_bitrates[i] * std::stod(adaptation_bitrate_multiplier))
                    {
                        selected_quality = std::to_string(qualities[i]); // Choose highest bitrate that fits throughput
                        selected_bitrate = available_bitrates[i];
                        break;
                    }
                }
                if (selected_quality == "")
                {
                    selected_quality = std::to_string(qualities[0]);
                    selected_bitrate = available_bitrates[0];
                }

                int index = request.find("p_");
                int start = index - 1;
                std::string name = "";
                while (true)
                {
                    if (!isdigit(request[start]))
                    {
                        break;
                    }
                    name = request[start] + name;
                    start--;
                }
                request.replace(start + 1, name.length(), selected_quality);
                clientrequest = request;
                send(serverfd, request.c_str(), request.length(), 0);
            }
            else if (request.find("favicon") != std::string::npos)
            {
                std::string errormessage = "HTTP/1.1 404 File not found\nServer: SimpleHTTP/0.6 Python/3.12.3\nDate: Fri, 18 Oct 2024 10:51:41 GMT\nContent-Type: text/html;charset=utf-8\nContent-Length: 335\nCache-Control: no-store, no-cache, must-revalidate\nAccess-Control-Allow-Origin: *\nAccess-Control-Allow-Methods: *\nAccess-Control-Allow-Headers: *\n\n<!DOCTYPE HTML>\n<html lang=\"en\">\n    <head>\n        <meta charset=\"utf-8\">\n        <title>Error response</title>\n    </head>\n    <body>\n        <h1>Error response</h1>\n        <p>Error code: 404</p>\n        <p>Message: File not found.</p>\n        <p>Error code explanation: 404 - Nothing matches the given URI.</p>\n    </body>\n</html>\n";
                send(clientfd, errormessage.c_str(), errormessage.length(), 0);
            }
            else
            {
                if (request.find("hls.min.js.map") != std::string::npos)
                {
                    std::cout << "found it" << std::endl;
                }
                send(serverfd, buf, bytesReceived, 0);
            }
        }

        if (FD_ISSET(serverfd, &read_fds))
        {

            memset(buf, 0, MAXDATASIZE);
            int bytesReceived = 0;
            auto start = std::chrono::high_resolution_clock::now();
            bytesReceived = recv(serverfd, buf, sizeof buf, 0);
            if (bytesReceived <= 0)
            {
                perror("server 1");
                break;
            }
            if (isChunk)
            {
                if (bytesReceived <= 0)
                {
                    perror("chunks");
                    return;
                }
                // send(clientfd, buf, bytesReceived, 0); // send header

                int contentLength = get_content_length(buf);
                std::string contentMessage(buf);
                std::string bufstr(buf, buf + bytesReceived);
                char pattern[] = "\r\n\r\n";
                std::string headerMessage = contentMessage.substr(0, contentMessage.find(pattern) + 4);

                long totalReceived = bytesReceived - headerMessage.length();

                while (true)
                {
                    if (totalReceived == contentLength)
                    {
                        break;
                    }
                    memset(buf, 0, MAXDATASIZE);
                    bytesReceived = recv(serverfd, buf, sizeof buf, 0);
                    std::string tempstr(buf, buf + bytesReceived);
                    bufstr += tempstr;

                    if (bytesReceived <= 0)
                    {
                        perror("chunks");
                        return;
                    }

                    totalReceived += bytesReceived;
                }
                auto elapsed = std::chrono::high_resolution_clock::now() - start;

                send(clientfd, bufstr.c_str(), bufstr.length(), 0);

                long nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       elapsed)
                                       .count();

                long throughput = 8000000000 * (double)totalReceived / nanoseconds;

                double alpha = std::stod(adaptation_gain);
                T_cur = (alpha * throughput) + ((1 - alpha) * T_cur);
                std::string chunkname = clientrequest.substr(4, clientrequest.find("HTTP") - 5);
                double seconds = (double)nanoseconds / 1000000000;
                log_chunk(browser_ip, chunkname, ip, seconds, throughput / 1000, T_cur / 1000, selected_bitrate / 1000);
                isChunk = false;

                continue;
            }

            std::string response(buf);

            if ((response.find("BANDWIDTH=") != std::string::npos))
            {
                // std::string url = clientrequest.substr(4, clientrequest.find("HTTP")-1);
                int index;
                available_bitrates.clear();
                qualities.clear();
                while (response.find("BANDWIDTH") != std::string::npos)
                {
                    index = response.find("BANDWIDTH");
                    std::string bandwidth_string;
                    std::string name_string;
                    int end = index + 10;
                    while (true)
                    {

                        if (!isdigit(response[end]))
                        {
                            break;
                        }
                        bandwidth_string += response[end];
                        end++;
                    }
                    response = response.substr(end);

                    end = response.find("NAME") + 6;
                    while (true)
                    {
                        if (!isdigit(response[end]))
                        {
                            break;
                        }
                        name_string += response[end];
                        end++;
                    }
                    available_bitrates.push_back(stoi(bandwidth_string));
                    qualities.push_back(stoi(name_string));
                }
                sort(available_bitrates.begin(), available_bitrates.end());
                sort(qualities.begin(), qualities.end());
                T_cur = available_bitrates[0];
            }
            send(clientfd, buf, bytesReceived, 0);
        }
    }
    return;
}

int main(int argc, char *argv[])
{
    std::string proxy_host, proxy_port, upstream_server_host, upstream_server_port, alpha, adaptation_bitrate_multiplier, nameserver_ip, nameserver_port;
    if (argc < 3 || argc % 2 == 0)
    {
        std::cerr << "Invalid arguments. Usage: ./miProxy --proxy-host <host> --proxy-port <port> ..." << std::endl;
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--proxy-host") == 0)
        {
            proxy_host = argv[i + 1];
        }
        else if (strcmp(argv[i], "--proxy-port") == 0)
        {
            proxy_port = argv[i + 1];
        }
        else if (strcmp(argv[i], "--upstream-server-host") == 0)
        {
            upstream_server_host = argv[i + 1];
        }
        else if (strcmp(argv[i], "--upstream-server-port") == 0)
        {
            upstream_server_port = argv[i + 1];
        }
        else if (strcmp(argv[i], "--log-file-name") == 0)
        {
            log_file_name = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--adaptation-gain") == 0)
        {
            alpha = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--nameserver-ip") == 0)
        {
            nameserver_ip = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--nameserver-port") == 0)
        {
            nameserver_port = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--adaptation-bitrate-multiplier"))
        {
            adaptation_bitrate_multiplier = argv[i + 1];
        }
        i++;
    }
    if (adaptation_bitrate_multiplier.empty())
    {
        adaptation_bitrate_multiplier = "1.5";
    }

    int sockfd, newfd;
    fd_set master_fds, read_fds;
    int fdmax;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    logfile.open(log_file_name);

    if (!validateIpAddress(upstream_server_host))
    {
        myfile << "shouldn't be here" << std::endl;
        upstream_server_host = query_nameserver(upstream_server_host, proxy_host, proxy_port, nameserver_ip, nameserver_port);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(proxy_host.c_str(), proxy_port.c_str(), &hints, &servinfo) != 0)
    {
        perror("getaddrinfo error");
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        std::cerr << "server: failed to bind" << std::endl;
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    while (true)
    {

        sin_size = sizeof their_addr;
        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        std::cout << "new connection! " << std::endl;
        if (newfd == -1)
        {
            perror("accept");
            return 0;
        }
        if (!fork())
        {
            char client_ip[INET6_ADDRSTRLEN];
            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), client_ip, sizeof client_ip);
            serverprocess(newfd, upstream_server_host, upstream_server_port, client_ip, alpha, adaptation_bitrate_multiplier, log_file_name);
        }
    }

    return 0;
}

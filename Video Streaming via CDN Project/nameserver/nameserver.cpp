#include <iostream>
#include <vector>
#include <fstream>
#include <span>
#include <sstream>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <iterator>

#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <regex>
#include <fstream> // For logging
#include "DNS/DNSDomainName.h"
#include "DNS/DNSHeader.h"
#include "DNS/DNSMessage.h"
#include "DNS/DNSQuestion.h"
#include "DNS/DNSResourceRecord.h"

#define TESTING false
#define BACKLOG 10
#define MAXDATASIZE 2000
std::ofstream myfile("nameserver.txt", std::ios_base::app);
std::ofstream ip_logger("nameserver_log.txt");

std::vector<std::string> ips;
std::vector<std::string> clients;
std::vector<std::string> servers;
std::vector<std::vector<int>> matrix;

int ip_index = 0;

void logger(std::string log_file_name, std::string ip, std::string query, std::string response)
{
    ip_logger << ip << " " << query << " " << response << std::endl;
}

int minDistance(std::vector<int> dist, bool sptSet[])
{

    // Initialize min value
    int min = INT32_MAX, min_index;

    for (int v = 0; v < matrix.size(); v++)
        if (sptSet[v] == false && dist[v] <= min)
            min = dist[v], min_index = v;

    return min_index;
}
std::string dijkstra(int src)
{

    std::vector<int> dist(matrix.size());

    bool sptSet[matrix.size()];

    for (int i = 0; i < matrix.size(); i++)
        dist[i] = INT32_MAX, sptSet[i] = false;

    dist[src] = 0;

    for (int count = 0; count < matrix.size() - 1; count++)
    {
        int u = minDistance(dist, sptSet);
        sptSet[u] = true;

        for (int v = 0; v < matrix.size(); v++)

            if (!sptSet[v] && matrix[u][v] && dist[u] != INT32_MAX && dist[u] + matrix[u][v] < dist[v])
                dist[v] = dist[u] + matrix[u][v];
    }

    int minimum = INT32_MAX;
    std::string min_ip;

    for (int i = dist.size() - servers.size(); i < dist.size(); i++)
    {
        if (dist[i] < minimum)
        {
            minimum = dist[i];
            min_ip = servers[i - dist.size() + servers.size()];
        }
    }
    if (minimum == INT32_MAX)
    {
        return "-1";
    }
    return min_ip;
}
bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it))
        ++it;
    return !s.empty() && it == s.end();
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void server_action(socklen_t serverfd, std::string domain, std::string log_file_name, bool round_robin)
{

    char buffer[MAXDATASIZE];
    struct sockaddr_storage client_addr;
    int numbytes;
    char s[INET6_ADDRSTRLEN];

    socklen_t client_addr_len = sizeof client_addr;

    memset(buffer, 0, sizeof buffer);
    numbytes = recvfrom(serverfd, buffer, MAXDATASIZE - 1, 0, (struct sockaddr *)&client_addr, &client_addr_len);
    if (numbytes == -1)
    {
        perror("recvfrom");
        exit(1);
    }

    buffer[numbytes] = '\0';
    struct sockaddr_in *sin = (struct sockaddr_in *)&client_addr;
    struct in_addr ipAddr = sin->sin_addr;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, client_ip, INET_ADDRSTRLEN);
    std::string client_ip_str(client_ip);

    std::vector<std::byte> v(
        reinterpret_cast<std::byte *>(std::begin(buffer)),
        reinterpret_cast<std::byte *>(&buffer[numbytes]));

    auto queryMessage = DNSMessage::deserialize(std::span<const std::byte>(v));

    uint16_t id = queryMessage.header.ID;
    auto domain_name = queryMessage.question.QNAME.toString();

    DNSMessage response = DNSMessage();

    response.header.ID = id;
    response.header.QR = 1;
    response.header.OPCODE = DNSOpcode::QUERY;
    response.header.AA = 0;
    response.header.TC = 0;
    response.header.RD = 0;
    response.header.RA = 0;
    response.header.Z = 0;
    response.header.RCODE = DNSRcode::NO_ERROR;
    response.header.QDCOUNT = 1;
    response.header.ANCOUNT = 0;
    response.header.NSCOUNT = 0;
    response.header.ARCOUNT = 0;
    response.question = queryMessage.question;

    if (domain[domain.size() - 1] != '.')
    {
        domain.push_back('.');
    }

    if ((!round_robin && find(clients.begin(), clients.end(), client_ip_str) == clients.end()) || domain_name != domain || queryMessage.header.OPCODE != DNSOpcode::QUERY || queryMessage.question.QTYPE != DNSQType::A)
    {
        response.header.RCODE = DNSRcode::NAME_ERROR;
        std::vector<std::byte> serializedMessage = response.serialize();
        numbytes = sendto(serverfd, serializedMessage.data(), serializedMessage.size(), 0, (struct sockaddr *)&client_addr, client_addr_len);
        if (numbytes == -1)
        {
            perror("send");
            exit(1);
        }
        return;
    }

    DNSResourceRecord rr;
    rr.TYPE = DNSRRType::A;
    rr.CLASS = DNSRRClass::IN;

    rr.NAME = DNSDomainName::fromString(domain_name);

    rr.RDLENGTH = sizeof(DNSResourceRecord::RecordDataTypes::A);
    myfile << static_cast<uint16_t>(rr.RDLENGTH) << std::endl;
    rr.TTL = 0;
    myfile << static_cast<uint16_t>(rr.TTL) << std::endl;

    std::string responseip;

    if (round_robin)
    {
        myfile << "round-robin" << std::endl;
        responseip = ips[ip_index];
        rr.RDATA = DNSResourceRecord::RecordDataTypes::A(responseip);

        myfile << "check" << std::get<0>(rr.RDATA).toString() << std::endl;
        if (ip_index == ips.size() - 1)
        {
            ip_index = 0;
        }
        else
        {
            ip_index++;
        }
    }
    else
    {
        myfile << "ge" << std::endl;
        bool client_found = false;
        myfile << "servers" << std::endl;
        for (int i = 0; i < servers.size(); i++)
        {
            myfile << servers[i] << std::endl;
        }
        myfile << "clients " << std::endl;
        for (int i = 0; i < clients.size(); i++)
        {
            myfile << clients[i] << std::endl;
        }
        myfile << client_ip << std::endl;

        for (int i = 0; i < clients.size(); i++)
        {
            if (clients[i] == client_ip)
            {
                responseip = dijkstra(i);
                client_found = true;
            }
        }

        myfile << "dijkstras: " << responseip << std::endl;
        if (!client_found)
        {
            perror("ip not found");
            exit(1);
        }

        if (responseip == "-1")
        {
            std::vector<std::byte> serializedMessage = response.serialize();
            numbytes = sendto(serverfd, serializedMessage.data(), serializedMessage.size(), 0, (struct sockaddr *)&client_addr, client_addr_len);
            if (numbytes == -1)
            {
                perror("send");
                exit(1);
            }
            myfile << "sent" << numbytes << std::endl;
            return;
        }

        rr.RDATA = DNSResourceRecord::RecordDataTypes::A(responseip);
    }
    response.header.ANCOUNT = 1;
    response.answers.push_back(rr);

    std::vector<std::byte> serializedMessage = response.serialize();

    auto des = DNSMessage::deserialize(serializedMessage);

    logger(log_file_name, client_ip_str, domain, responseip);
    numbytes = sendto(serverfd, serializedMessage.data(), serializedMessage.size(), 0, (struct sockaddr *)&client_addr, client_addr_len);
    myfile << "sent" << numbytes << std::endl;
    if (numbytes == -1)
    {
        perror("send");
        exit(1);
    }
    return;
    close(serverfd);
}

int main(int argc, char *argv[])
{
    std::string ip, port, domain, log_file_name, round_robin_ip_list_file_path = "", network_topology_file_path = "";
    if (argc < 3 || argc % 2 == 0)
    {
        std::cerr << "Invalid arguments. Usage: ./miProxy --proxy-host <host> --proxy-port <port> ..." << std::endl;
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--ip") == 0)
        {
            ip = argv[i + 1];
        }
        else if (strcmp(argv[i], "--port") == 0)
        {
            port = argv[i + 1];
        }
        else if (strcmp(argv[i], "--domain") == 0)
        {
            domain = argv[i + 1];
        }
        else if (strcmp(argv[i], "--log-file-name") == 0)
        {
            log_file_name = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--round-robin-ip-list-file-path") == 0)
        {
            round_robin_ip_list_file_path = argv[i + 1]; // Capture log file name
        }
        else if (strcmp(argv[i], "--network-topology-file-path") == 0)
        {
            network_topology_file_path = argv[i + 1];
        }
        i++;
    }

    int sockfd;
    fd_set master_fds, read_fds;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    bool round_robin;

    // round_robin_ip_list_file_path = "";
    // network_topology_file_path = "./resources/sample_network_topology.txt";

    if (round_robin_ip_list_file_path != "")
    {
        round_robin = true;
        std::ifstream file(round_robin_ip_list_file_path);
        std::string str;
        while (std::getline(file, str))
        {
            ips.push_back(str);
        }
    }
    else
    {
        if (network_topology_file_path == "")
        {
            perror("no files!");
            exit(1);
        }
        round_robin = false;
        std::ifstream file(network_topology_file_path);
        std::string str;
        std::string tempstr;
        std::string node1, node2, weight;
        int num_nodes;
        int num_links;
        std::getline(file, str);
        std::istringstream my_stream(str);

        my_stream >> tempstr;
        my_stream >> num_nodes;
        while (std::getline(file, str))
        {
            std::istringstream my_stream(str);
            my_stream >> tempstr;
            if (!is_number(tempstr))
            {
                my_stream >> num_links;
                break;
            }
            my_stream >> tempstr;
            if (tempstr == "CLIENT")
            {
                my_stream >> tempstr;
                clients.push_back(tempstr);
            }
            else if (tempstr == "SERVER")
            {
                my_stream >> tempstr;
                servers.push_back(tempstr);
            }
        }

        for (int i = 0; i < num_nodes; i++)
        {
            std::vector temp(num_nodes, 0);
            matrix.push_back(temp);
        }
        while (std::getline(file, str))
        {
            std::istringstream my_stream(str);
            my_stream >> node1;
            my_stream >> node2;
            my_stream >> weight;
            matrix[std::stoi(node1)][std::stoi(node2)] = std::stoi(weight);
            matrix[std::stoi(node2)][std::stoi(node1)] = std::stoi(weight);
        }
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(ip.c_str(), port.c_str(), &hints, &servinfo) != 0)
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

    while (true)
    {
        server_action(sockfd, domain, log_file_name, round_robin);
    }

    return 0;
}

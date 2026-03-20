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
#include <fstream>
#include "../starter_files/crc32.h"
#include "../starter_files/PacketHeader.h"

#define PORT "9000"

std::string port_num, window_size, output_dir, log_file;
std::ofstream output;

struct message_package
{
    int seq_num;
    std::vector<char> message;
};

void handle(socklen_t sockfd, int client_count)
{
    printf("listener: waiting to recvfrom...\n");
    struct sockaddr_storage their_addr;
    PacketHeader *header;
    socklen_t their_addr_len = sizeof their_addr;
    char buff[1472];
    while (true)
    {
        memset(buff, 0, sizeof(buff));
        int bytes_recv = recvfrom(sockfd, buff, sizeof(buff), 0,
                                  (struct sockaddr *)&their_addr, &their_addr_len);

        if (bytes_recv <= 0)
        {
            perror("recvfrom");
            exit(1);
        }
        char header_msg[16];
        strncpy(header_msg, buff, 16);

        header = reinterpret_cast<PacketHeader *>(header_msg);
        output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;

        if (header->type != 0)
        {
            perror("received not start");
            continue;
        }
        break;
    }

    std::ofstream file(output_dir + "FILE-" + std::to_string(client_count) + ".out");
    header->type = 3;
    header->checksum = 0;
    header->length = 0;

    memset(buff, 0, sizeof(buff));
    memcpy(buff, header, 16);

    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
    ;
    their_addr_len = sizeof(their_addr);

    int bytes_sent = sendto(sockfd, buff, sizeof(buff), 0,
                            (struct sockaddr *)&their_addr, their_addr_len);
    int seq_num = 0;

    std::vector<message_package> message_buff(std::stoi(window_size));
    int dupack = 0;
    while (true)
    {
        char buff[1472];
        memset(buff, 0, sizeof(buff));

        their_addr_len = sizeof(their_addr);

        int bytes_recv = recvfrom(sockfd, buff, sizeof(buff), 0,
                                  (struct sockaddr *)&their_addr, &their_addr_len);
        if (bytes_recv <= 0)
        {
            perror("recvfrom");
            exit(1);
        }

        char header_msg[16];
        memcpy(header_msg, buff, 16);

        PacketHeader *header;
        header = reinterpret_cast<PacketHeader *>(header_msg);

        output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
        ;

        if (header->type == 1)
        {
            break;
        }
        if (header->type == 0)
        {
            continue;
        }
        if (header->type != 2)
        {
            perror("header type");
            exit(1);
        }
        if (header->seqNum >= seq_num + std::stoi(window_size))
        {
            continue;
        }

        char message[header->length];

        memcpy(message, buff + 16, header->length);
        if (header->checksum != crc32(message, header->length))
        {
            // checksum failed
            continue;
        }

        if (header->seqNum == seq_num)
        {
            int found = 0;
            message_package pack = message_package();
            pack.seq_num = header->seqNum;
            pack.message.assign(message,message + sizeof(message));

            file.write(pack.message.data(), pack.message.size());
            for (int i = 0; i < message_buff.size(); i++)
            {
                if (message_buff[i].seq_num == seq_num + found + 1)
                {
                    file.write(message_buff[i].message.data(), message_buff[i].message.size());
                    message_buff.erase(message_buff.begin() + i);
                    found++;
                    i--;
                }
            }
            header->checksum = 0;
            header->length = 0;
            header->seqNum = header->seqNum + found + 1;
            header->type = 3;
            char reply[16];
            memcpy(reply, header, 16);
            output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
            ;
            their_addr_len = sizeof(their_addr);

            bytes_sent = sendto(sockfd, reply, sizeof(reply), 0,
                                (struct sockaddr *)&their_addr, their_addr_len);
            if (bytes_sent <= 0)
            {
                perror("send");
                exit(1);
            }
            seq_num += found + 1;
        }
        else
        {
            message_package pack = message_package();
            pack.seq_num = header->seqNum;
            pack.message.assign(message,message + sizeof(message));
            message_buff.push_back(pack);

            header->checksum = 0;
            header->length = 0;
            header->seqNum = seq_num;
            header->type = 3;
            char reply[16];
            memcpy(reply, header, 16);
            output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
            ;
            their_addr_len = sizeof(their_addr);

            bytes_sent = sendto(sockfd, reply, sizeof(reply), 0,
                                (struct sockaddr *)&their_addr, their_addr_len);
            if (bytes_sent <= 0)
            {
                perror("send");
                exit(1);
            }
        }
    }
    header->type = 3;
    header->length = 0;
    header->checksum = 0;
    header->seqNum = 0;
    char reply[16];
    memcpy(reply, header, 16);
    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
    ;
    their_addr_len = sizeof(their_addr);

    bytes_sent = sendto(sockfd, reply, sizeof(reply), 0, (sockaddr *)&their_addr, their_addr_len);
    if (bytes_sent <= 0)
    {
        perror("end message");
        exit(1);
    }
    return;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        perror("arguments");
        exit(1);
    }
    port_num = argv[1];
    window_size = argv[2];
    output_dir = argv[3];
    log_file = argv[4];

    output.open(log_file);

    struct addrinfo hints, *p;
    struct sockaddr_in server_addr;
    int sockfd;
    int rv;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    // // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(std::stoi(port_num)); // Port number
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // // Bind the socket to the specified IP and port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Error binding socket" << std::endl;
        return 1;
    }

    // if ((rv = getaddrinfo(NULL, port_num.c_str(), &hints, &servinfo)) != 0)
    // {
    //     fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    //     return 1;
    // }

    // // loop through all the results and bind to the first we can
    // for (p = servinfo; p != NULL; p = p->ai_next)
    // {
    //     if ((sockfd = socket(p->ai_family, p->ai_socktype,
    //                          p->ai_protocol)) == -1)
    //     {
    //         perror("listener: socket");
    //         continue;
    //     }

    //     if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    //     {
    //         close(sockfd);
    //         perror("listener: bind");
    //         continue;
    //     }

    //     break;
    // }

    // if (p == NULL)
    // {
    //     fprintf(stderr, "listener: failed to bind socket\n");
    //     return 2;
    // }

    // struct sockaddr_in addr;
    // socklen_t addrlen = sizeof(addr);

    // if (getsockname(sockfd, (struct sockaddr *)&addr, &addrlen) == -1)
    // {
    //     std::cerr << "Error getting socket name." << std::endl;
    //     return 1;
    // }

    // int temp_port = ntohs(addr.sin_port);
    // char ipstr[INET_ADDRSTRLEN];
    // inet_ntop(AF_INET, &(addr.sin_addr), ipstr, INET_ADDRSTRLEN);

    // output << temp_port << std::endl;
    // output << "ip:" <<ipstr << std::endl;

    for (int i = 0; true; i++)
    {
        handle(sockfd, i);
    }
}

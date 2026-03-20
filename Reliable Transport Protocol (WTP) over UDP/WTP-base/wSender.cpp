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
#include <thread>
#include "../starter_files/crc32.h"
#include "../starter_files/PacketHeader.h"

int totalPackets = 0;
std::vector<char> bytes;
std::ofstream output;
std::ofstream test;
void send_start_end(socklen_t sockfd, bool start, struct sockaddr_in DNS_addr)
{
    PacketHeader *header = new PacketHeader();
    header->seqNum = 0;
    if (start)
    {
        header->type = 0;
    }
    else
    {
        header->type = 1;
    }
    header->length = 0;
    header->checksum = 0;
    char buff[16];
    memset(buff, 0, 16);

    memcpy(buff, header, 16);
    socklen_t DNS_addr_size = sizeof(DNS_addr);
    int bytes_sent = sendto(sockfd, buff, sizeof(buff), 0, (sockaddr *)&DNS_addr, DNS_addr_size);
    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;
    ;

    if (bytes_sent <= 0)
    {
        perror("end send");
        exit(1);
    }
}

void send(socklen_t sockfd, int seq_num, struct sockaddr_in DNS_addr)
{
    int length = 1456;
    PacketHeader *header = new PacketHeader();
    if (bytes.size() < seq_num * 1456 + 1456)
    {
        length = bytes.size() - (seq_num * 1456);
    }
    char buff[16 + length];
    memset(buff, 0, sizeof(buff));

    char message[length];
    memset(message, 0, sizeof(message));

    std::copy(bytes.begin() + seq_num * 1456, bytes.begin() + (seq_num * 1456) + length, message);

    header->type = 2;
    header->length = length;
    header->seqNum = seq_num;
    header->checksum = crc32(message, length);

    memcpy(buff, header, 16);
    memcpy(buff + 16, message, length);

    socklen_t DNS_addr_size = sizeof(DNS_addr);
    int bytes_sent = sendto(sockfd, buff, sizeof(buff), 0, (sockaddr *)&DNS_addr, DNS_addr_size);
    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;

    if (bytes_sent <= 0)
    {
        output << "exit" << std::endl;
        perror("send");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    std::string receiver_IP, receiver_port, window_size, input_file, log_file;
    if (argc != 6)
    {
        perror("arguments");
        exit(1);
    }
    receiver_IP = argv[1];
    receiver_port = argv[2];
    window_size = argv[3];
    input_file = argv[4];
    log_file = argv[5];
    output.open(log_file);

    // std::ofstream test("test.txt");

    struct addrinfo hints, *servinfo, *p;
    int rv, sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(NULL, receiver_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {

        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }

        break; // if we get here, we must have connected successfully
    }
    if (p == NULL)
    {
        // looped off the end of the list with no successful bind
        fprintf(stderr, "failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo); // all done with this structure
    // struct sockaddr_in addr;
    // socklen_t addrlen = sizeof(addr);

    // if (getsockname(sockfd, (struct sockaddr *)&addr, &addrlen) == -1)
    // {
    //     std::cerr << "Error getting socket name." << std::endl;
    //     return 1;
    // }

    // int temp_port = ntohs(addr.sin_port);
    // output << temp_port << std::endl;

    struct sockaddr_in DNS_addr;
    memset(&DNS_addr, 0, sizeof(DNS_addr));
    DNS_addr.sin_family = AF_INET;
    DNS_addr.sin_port = htons(std::stoi(receiver_port)); // Port number to send to
    int inet = inet_pton(AF_INET, receiver_IP.c_str(), &DNS_addr.sin_addr);
    if (inet < 0)
    {
        exit(1);
    }
    socklen_t DNS_addr_size = sizeof(DNS_addr);

    std::ifstream file(input_file, std::ios::binary);

    std::vector<char> temp_bytes(
        (std::istreambuf_iterator<char>(file)),
        (std::istreambuf_iterator<char>()));
    bytes = temp_bytes;

    totalPackets = bytes.size() / 1456 + 1;

    file.close();

    PacketHeader *header = new PacketHeader();
    header->type = 0;
    header->checksum = 0;
    header->length = 0;
    header->seqNum = 0;
    char header_data[16];
    memset(header_data, 0, 16);

    memcpy(header_data, header, 16);
    sockaddr_in holder;
    socklen_t holder_size = sizeof(holder);
    int numbytes = sendto(sockfd, header_data, sizeof(header_data), 0, (sockaddr *)&DNS_addr, DNS_addr_size);

    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;

    if (numbytes == -1)
    {
        perror("start message");
        exit(1);
    }
    numbytes = recvfrom(sockfd, header_data, sizeof(header_data), 0, (sockaddr *)&holder, &holder_size);

    if (numbytes <= 0)
    {
        perror("start message");
        exit(1);
    }

    memcpy(header, header_data, 16);

    output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;

    if (header->type != 3 || header->seqNum != 0)
    {
        perror("start message");
        exit(1);
    }
    // 1472
    // 1456
    int recv_seq_num = 0;
    bool last = false;

    for (int i = 0; i < std::stoi(window_size); i++)
    {
        if (i == totalPackets)
        {
            break;
        }
        send(sockfd, i, DNS_addr);
    }

    auto start = std::chrono::high_resolution_clock::now();
    long milliseconds;
    while (true)
    {
        sockaddr_in holder;
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                           elapsed)
                           .count();
        if (milliseconds >= 400)
        {
            for (int i = recv_seq_num; i < std::stoi(window_size) + recv_seq_num; i++)
            {
                if (i >= totalPackets)
                {
                    break;
                }
                send(sockfd, i, DNS_addr);
            }
            start = std::chrono::high_resolution_clock::now();
            continue;
        }
        char header_buff[16];
        memset(header_buff, 0, 16);
        holder_size = sizeof(holder);
        int bytes_recv = recvfrom(sockfd, header_buff, sizeof(header_buff) - 1, MSG_DONTWAIT, (sockaddr *)&holder, &holder_size);

        if (bytes_recv <= 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(500));
            continue;
        }


        PacketHeader header;
        memcpy(&header, header_buff, 16);

        output << header.type << " " << header.seqNum << " " << header.length << " " << header.checksum << std::endl;
        ;
        if (header.type != 3)
        {
            output << "out" << std::endl;
            perror("not an ack");
            exit(1);
        }
        if (header.seqNum == recv_seq_num)
        {
            continue;
        }
        else if (header.seqNum >= recv_seq_num + 1)
        {
            start = std::chrono::high_resolution_clock::now();

            if (header.seqNum == totalPackets)
            {
                break;
            }
            if (header.seqNum >= totalPackets - std::stoi(window_size) + 1)
            {
                recv_seq_num = header.seqNum;
                continue;
            }

            for (int i = recv_seq_num; i < header.seqNum; i++)
            {
                if (i == totalPackets)
                {
                    break;
                }
                send(sockfd, i + std::stoi(window_size), DNS_addr);
            
            }
            recv_seq_num = header.seqNum;
        }
    }

    send_start_end(sockfd, false, DNS_addr);

    while (true)
    {

        char buff[16];
        holder_size = sizeof(holder);
        int bytes_recv = recvfrom(sockfd, buff, sizeof(buff), 0, (sockaddr *)&holder, &holder_size);
        memcpy(header, buff, 16);
        output << header->type << " " << header->seqNum << " " << header->length << " " << header->checksum << std::endl;

        if (header->seqNum == 0 && header->type == 3)
        {
            break;
        }
    }
    output.close();
    return 1;
}

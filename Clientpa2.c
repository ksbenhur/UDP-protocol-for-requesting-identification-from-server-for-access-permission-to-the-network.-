#define _CRT_SECURE_NO_WARNINGS  // Disable security warnings for deprecated functions
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>  // Include for uint8_t, uint16_t

#pragma comment(lib, "Ws2_32.lib")

#define PORT 42000
#define MAXBUFFERSIZE 1024
#define TIMEOUT 3

#define START_PACKET 0XFFFF
#define END_PACKET 0XFFFF
#define CLIENT_ID_MAX 0XFF
#define MAX_LENGTH 0XFF
#define SOURCE_SUBSCRIBER_NO 0XFFFFFFFF

#define ACCESS 0XFFFB
#define NOT_PAID 0XFFF9
#define NOT_EXIST 0XFFFA

struct requestPacket {
    uint16_t startPacketID;
    uint8_t clientID;
    uint16_t accessPermission;
    uint8_t segmentNumber;
    uint8_t packLen;
    uint8_t technology;
    unsigned long srcSubNumber;
    uint16_t endPacketID;
};

struct responsePacket {
    uint16_t startPacketID;
    uint8_t clientID;
    uint16_t accessPermission;
    uint8_t segmentNumber;
    uint8_t packLen;
    uint8_t technology;
    unsigned long srcSubNumber;
    uint16_t endPacketID;
};

void showPacket(struct requestPacket request) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);  // Use non-secure version
    char timeStr[26];
    if (tm != NULL) {
        strncpy(timeStr, asctime(tm), sizeof(timeStr));  // Use non-secure version
        timeStr[24] = '\0';  // Remove newline added by asctime
    } else {
        strcpy(timeStr, "Unknown Time");
    }
    printf("\nDetails of Received Packet:");
    printf("\n****\n %s packetID: %x \n ClientID : %x \n Access Permission %x \n Segment number : %d \n Length %d \n Technology: %d \n Source Subscriber No: %lu \n End of packetID: %x\n****\n",
        timeStr, request.startPacketID, request.clientID, request.accessPermission, request.segmentNumber, request.packLen, request.technology, request.srcSubNumber, request.endPacketID);
}


struct requestPacket createRequestPacket() {
    struct requestPacket request;
    request.startPacketID = START_PACKET;
    request.clientID = CLIENT_ID_MAX;
    request.accessPermission = 0XFFF8;
    request.endPacketID = END_PACKET;
    return request;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct requestPacket request;
    struct responsePacket response;
    char Msg[30];
    FILE* file;
    SOCKET SOCKET_FD;
    struct sockaddr_in clientAddress;
    int addressSize = sizeof(clientAddress);
    int i = 1;

    if ((SOCKET_FD = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct timeval timer;
    timer.tv_sec = 3;
    timer.tv_usec = 0;
    if (setsockopt(SOCKET_FD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timer, sizeof(struct timeval)) == SOCKET_ERROR) {
        printf("setsockopt failed with error: %d\n", WSAGetLastError());
        closesocket(SOCKET_FD);
        WSACleanup();
        return 1;
    }

    memset(&clientAddress, 0, sizeof(clientAddress));
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); // Connect to localhost
    clientAddress.sin_port = htons(PORT);

    request = createRequestPacket();

    file = fopen("payload.txt", "rt");
    if (file == NULL) {
        printf("\n **** ERROR: File not found ****");
        closesocket(SOCKET_FD);
        WSACleanup();
        return 1;
    }

    printf("\n **** Establishing connection with the Server......\n");
    Sleep(5000);

    while (fgets(Msg, sizeof(Msg), file) != NULL) {
        int receive = 0;
        int retryCounter = 1;
        printf("\n");

        char* words = strtok(Msg, " ");
        request.srcSubNumber = strtoul(words, NULL, 10);
        words = strtok(NULL, " ");
        request.technology = atoi(words);
        request.segmentNumber = i;
        request.packLen = 6; // Technology (1) + SubscriberNo (5) = 6 bytes

        showPacket(request);

        while (receive <= 0 && retryCounter <= 3) {
            if (sendto(SOCKET_FD, (char*)&request, sizeof(request), 0, (struct sockaddr*)&clientAddress, addressSize) == SOCKET_ERROR) {
                printf("sendto failed with error: %d\n", WSAGetLastError());
                retryCounter++;
                continue;
            }

            receive = recvfrom(SOCKET_FD, (char*)&response, sizeof(response), 0, NULL, NULL);
            if (receive == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT) {
                    printf("\n **** ERROR: Server does not respond ****\n");
                    printf("\n Retrying... %d packet send operation.", retryCounter);
                    retryCounter++;
                } else {
                    printf("recvfrom failed with error: %d\n", error);
                    break;
                }
            } else if (receive > 0) {
                printf("\n\n Response Received from the Server ===>>> \n");
                if (response.accessPermission == NOT_PAID)
                    printf("\n ** INFO: Subscriber has not paid ** \n");
                else if (response.accessPermission == NOT_EXIST)
                    printf("\n ** INFO: Subscriber does not exist **\n");
                else if (response.accessPermission == ACCESS)
                    printf("\n ** INFO: Subscriber has access **\n");
            }
        }

        if (retryCounter > 3) {
            printf("\n **** ERROR : Server does not respond ****");
            break;
        }
        printf("\n----------------------------------------------------------------------------------------\n");
        i++;
    }

    fclose(file);
    closesocket(SOCKET_FD);
    WSACleanup();
    return 0;
}
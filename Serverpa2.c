#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 42000
#define MAXLINE 1024
#define ENTRY 10

#define START_PACKET 0XFFFF
#define END_PACKET 0XFFFF
#define CLIENT_ID_MAX 0XFF
#define ACCESS_PERMISSION 0XFFF8

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

struct subscriberIdentity {
    unsigned long subscriberNumber;
    uint8_t tech;
    int status;
};

void showPacket(struct requestPacket request) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);  // Use POSIX-compliant localtime
    char timeStr[26];
    if (tm != NULL) {
        strncpy(timeStr, asctime(tm), sizeof(timeStr));  // Use asctime
        timeStr[24] = '\0';  // Remove newline added by asctime
    } else {
        strcpy(timeStr, "Unknown Time");
    }
    printf("\nDetails of Received Packet:");
    printf("\n ****\n %s packetID: %x \n ClientID : %x \n Access Permission: %x \n Segment Number: %d \n Length: %d \n Technology: %d \n Source Subscriber No: %lu \n End of packetID: %x\n ****\n",
        timeStr, request.startPacketID, request.clientID, request.accessPermission, request.segmentNumber, request.packLen, request.technology, request.srcSubNumber, request.endPacketID);
}

struct responsePacket createResponsePacket(struct requestPacket request) {
    struct responsePacket response;
    response.startPacketID = request.startPacketID;
    response.clientID = request.clientID;
    response.segmentNumber = request.segmentNumber;
    response.packLen = request.packLen;
    response.technology = request.technology;
    response.srcSubNumber = request.srcSubNumber;
    response.endPacketID = request.endPacketID;
    return response;
}

void extractFileContents(struct subscriberIdentity identity[]) {
    char line[30];
    int i = 0;
    FILE* file = fopen("Verification_Database.txt", "rt");
    if (file == NULL) {
        printf("\n **** ERROR: File not found ****\n");
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char* token = strtok(line, " ");
        identity[i].subscriberNumber = strtoul(token, NULL, 10);

        token = strtok(NULL, " ");
        identity[i].tech = atoi(token);

        token = strtok(NULL, " ");
        identity[i].status = atoi(token);

        i++;
    }
    fclose(file);
}

int getSubscriberStatus(struct subscriberIdentity identity[], unsigned long subscriberNumber, uint8_t technology) {
    for (int e = 0; e < ENTRY; e++) {
        if (identity[e].subscriberNumber == subscriberNumber && identity[e].tech == technology) {
            return identity[e].status;
        }
    }
    return -1;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct requestPacket request;
    struct responsePacket response;
    struct subscriberIdentity identity[ENTRY];
    SOCKET SOCKET_FD;
    struct sockaddr_in serverAddress;
    struct sockaddr_storage serverStorage;
    int addressSize = sizeof(serverStorage);
    int receive;

    extractFileContents(identity);

    if ((SOCKET_FD = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    if (bind(SOCKET_FD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        closesocket(SOCKET_FD);
        WSACleanup();
        return 1;
    }

    printf("\n ***** Starting Server *****\n");
    Sleep(5000);
    printf("\n ***** Server started. Listening for Packets *****\n");

    while (1) {
        receive = recvfrom(SOCKET_FD, (char*)&request, sizeof(request), 0, (struct sockaddr*)&serverStorage, &addressSize);
        if (receive == SOCKET_ERROR) {
            printf("recvfrom failed with error: %d\n", WSAGetLastError());
            continue;
        }

        showPacket(request);

        if (request.accessPermission == ACCESS_PERMISSION) {
            response = createResponsePacket(request);
            int result = getSubscriberStatus(identity, request.srcSubNumber, request.technology);

            if (result == 0) {
                response.accessPermission = NOT_PAID;
                printf("\n ** Subscriber has not paid **\n");
            } else if (result == -1) {
                response.accessPermission = NOT_EXIST;
                printf("\n ** Subscriber does not exist **\n");
            } else if (result == 1) {
                response.accessPermission = ACCESS;
                printf("\n ** Subscriber has access ** \n");
            }

            if (sendto(SOCKET_FD, (char*)&response, sizeof(response), 0, (struct sockaddr*)&serverStorage, addressSize) == SOCKET_ERROR) {
                printf("sendto failed with error: %d\n", WSAGetLastError());
            }
        }

        printf("\n ---------------------------------------------------------------------- \n");
    }

    closesocket(SOCKET_FD);
    WSACleanup();
    return 0;
}
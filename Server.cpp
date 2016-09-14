#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <atomic>
#include <string>
#include <cmath>


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "7778"
#define MAX_CLIENTS 3

boost::mutex lock;

// struct that keeps info useful info about clients and threads

struct ClientInfo {
	SOCKET ClientSockets[MAX_CLIENTS];

	std::atomic<int> activeThreadID{ -1 };

	bool errors[MAX_CLIENTS];

	std::atomic<int> receiveSignal{ 0 };

	std::atomic<int> numBytes{ 0 };

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;
};

char findNthDigitAndConvertToChar(int n, int value) {
	int temp = (int)(abs(value) / pow(10, n)) % 10;

	char temp2 = '0' + temp;

	return temp2;
}

int numDigits(int number) {
	if (number == 0) {
		return 1;
	}
	int digits = 0;
	while (number) {
		number /= 10;
		digits++;
	}
	return digits;
}

void sendToClient(ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID, char* sendbuf) {
	while (true) {
		if (clientInfo.receiveSignal == 1 && clientInfo.activeThreadID != ID) {
			lock.lock();

			iResult = clientInfo.numBytes + numDigits(clientInfo.activeThreadID) + 1;

			sendbuf[clientInfo.numBytes] = ',';
			for (int i = 0; i < numDigits(clientInfo.activeThreadID); i++) {
				sendbuf[clientInfo.numBytes + i + 1] = findNthDigitAndConvertToChar(i, clientInfo.activeThreadID);
			}

			iSendResult = send(clientInfo.ClientSockets[ID], sendbuf, iResult, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(clientInfo.ClientSockets[ID]);
				WSACleanup();
				clientInfo.errors[ID] = 1;
			}
			std::cout << "Bytes sent to " << ID << " = " << iSendResult << std::endl;

			clientInfo.receiveSignal = 0;
			clientInfo.activeThreadID = -1;
			lock.unlock();
		}
	}
}

void communicate(SOCKET &ListenSocket, ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID) {

	clientInfo.ClientSockets[ID] = accept(ListenSocket, NULL, NULL);

	std::cout << ID << " connected!" << std::endl;
	if (clientInfo.ClientSockets[ID] == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}

	boost::thread sender(sendToClient, std::ref(clientInfo), iResult, iSendResult, ID, clientInfo.recvbuf);

	// Receive until the peer shuts down the connection
	while (true) {
		iResult = recv(clientInfo.ClientSockets[ID], clientInfo.recvbuf, clientInfo.recvbuflen, 0);
		lock.lock();
		clientInfo.activeThreadID = ID;

		clientInfo.receiveSignal = 1;

		clientInfo.numBytes = iResult;

		if (iResult > 0) {
			std::cout << ID << " says: ";
			for (int i = 0; i < iResult; i++) {
				std::cout << clientInfo.recvbuf[i];
			}
			std::cout << std::endl;
			
			std::cout << "Bytes received from " << ID << " = " << iResult << std::endl;

		}
		if (clientInfo.recvbuf[0] == '0') {
			printf("Connection closing...\n");
			break;
		}
		if (iResult == SOCKET_ERROR) {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(clientInfo.ClientSockets[ID]);
			WSACleanup();
			clientInfo.errors[ID] = 1;
		}
		lock.unlock();
	}
	sender.join();
}

int __cdecl main(void)
{
	
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult = 0;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	ClientInfo clientInfo;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		clientInfo.errors[i] = false;
		clientInfo.ClientSockets[i] = INVALID_SOCKET;
	}

	boost::thread clients[MAX_CLIENTS];

	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i] = boost::thread(communicate, ListenSocket, std::ref(clientInfo), iResult, iSendResult, i);
	}
	
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clientInfo.errors[i] == 1) {
			return 1;
		}
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].join();
	}

	system("pause");
	return 0;
}
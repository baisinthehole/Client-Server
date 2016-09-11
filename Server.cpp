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


// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "7778"
#define MAX_CLIENTS 2

boost::mutex lock;

// struct that keeps info useful info about clients and threads

struct ClientInfo {
	SOCKET ClientSockets[MAX_CLIENTS];

	int threadIDs[MAX_CLIENTS];

	std::atomic<int> activeThreadID{ 0 };

	std::atomic<int> activeThreadIndex{ -1 };

	bool errors[MAX_CLIENTS];

	std::atomic<int> receiveSignal{ 0 };

	std::atomic<int> numBytes{ 0 };
};

// This function is returning a thread ID as an unsigned long. Call it inside a thread to get the thread ID
unsigned long getThreadId() {
	std::string threadId = boost::lexical_cast<std::string>(boost::this_thread::get_id());

	unsigned long threadNumber = 0;

	sscanf_s(threadId.c_str(), "%lx", &threadNumber);

	return threadNumber;
}

// Converts a thread ID to index
int convertFromThreadIdToIndex(int id, int threadIds[], int numClients) {
	for (int i = 0; i < numClients; i++) {
		if (threadIds[i] == id) {
			return i;
		}
	}
	return -1;
}

void sendToClient(ClientInfo &clientInfo, int &iResult, int &iSendResult, int index, char* sendbuf) {
	while (true) {
		if (clientInfo.receiveSignal == 1 && clientInfo.activeThreadIndex != index) {
			lock.lock();

			iResult = clientInfo.numBytes;

			iSendResult = send(clientInfo.ClientSockets[index], sendbuf, iResult, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(clientInfo.ClientSockets[index]);
				WSACleanup();
				clientInfo.errors[index] = 1;
			}
			std::cout << "Bytes sent to " << clientInfo.threadIDs[index] << " = " << iSendResult << std::endl;

			clientInfo.receiveSignal = 0;
			clientInfo.activeThreadIndex = -1;
			lock.unlock();
		}
	}
}

void communicate(SOCKET &ListenSocket, ClientInfo &clientInfo, int &iResult, int &iSendResult) {

	int index = convertFromThreadIdToIndex(clientInfo.activeThreadID, clientInfo.threadIDs, MAX_CLIENTS);

	clientInfo.ClientSockets[index] = accept(ListenSocket, NULL, NULL);

	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	clientInfo.threadIDs[index] = getThreadId();
	std::cout << clientInfo.threadIDs[index] << " connected!" << std::endl;
	if (clientInfo.ClientSockets[index] == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		clientInfo.errors[index] = 1;
	}

	boost::thread sender(sendToClient, std::ref(clientInfo), iResult, iSendResult, index, recvbuf);

	// Receive until the peer shuts down the connection
	while (true) {
		iResult = recv(clientInfo.ClientSockets[index], recvbuf, recvbuflen, 0);
		lock.lock();
		clientInfo.activeThreadID = getThreadId();

		clientInfo.activeThreadIndex = convertFromThreadIdToIndex(clientInfo.activeThreadID, clientInfo.threadIDs, MAX_CLIENTS);

		clientInfo.receiveSignal = 1;

		clientInfo.numBytes = iResult;

		if (iResult > 0) {
			std::cout << clientInfo.threadIDs[index] << " says: ";
			for (int i = 0; i < iResult; i++) {
				std::cout << recvbuf[i];
			}
			std::cout << std::endl;
			
			std::cout << "Bytes received from " << clientInfo.threadIDs[index] << " = " << iResult << std::endl;

		}
		if (recvbuf[0] == '0') {
			printf("Connection closing...\n");
			break;
		}
		if (iResult == SOCKET_ERROR) {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(clientInfo.ClientSockets[index]);
			WSACleanup();
			clientInfo.errors[index] = 1;
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
		clientInfo.threadIDs[i] = 0;
	}

	boost::thread firstClient(communicate, ListenSocket, std::ref(clientInfo), iResult, iSendResult);
	boost::thread secondClient(communicate, ListenSocket, std::ref(clientInfo), iResult, iSendResult);

	while (true) {
		
	}

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clientInfo.errors[i] == 1) {
			return 1;
		}
	}

	firstClient.join();
	secondClient.join();

	system("pause");
	return 0;
}
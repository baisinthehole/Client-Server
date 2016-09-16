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

// Struct that keeps info useful info about clients and threads
// All variables that are atomic are shared between threads
struct ClientInfo {

	// Array of sockets. One socket per client
	SOCKET ClientSockets[MAX_CLIENTS];

	// Stores the ID of the client that last sent a message to the server
	std::atomic<int> activeClientID{ -1 };

	// Array of error variables. One for each client. Variable is set to 1 if error occurs
	std::atomic<bool> errors[MAX_CLIENTS];

	// Signal that alerts the server that it has received a message
	std::atomic<int> receiveSignal{ 0 };

	// Integer that says how many bytes the current message received is
	std::atomic<int> numBytes{ 0 };

	// Buffer which incoming messages is stored in
	char recvbuf[DEFAULT_BUFLEN];

	// Maximum length of a message
	int recvbuflen = DEFAULT_BUFLEN;
};

// Converts nth digit in an integer to a char. This is used when sending client IDs as chars to clients
char findNthDigitAndConvertToChar(int n, int value) {
	int temp = (int)(abs(value) / pow(10, n)) % 10;

	char temp2 = '0' + temp;

	return temp2;
}

// Returns how many digits there is in an integer
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

// Adds client ID to the back of the message 
char* addInformationToMessage(char* sendbuf, ClientInfo &clientInfo) {
	sendbuf[clientInfo.numBytes] = ',';
	for (int i = 0; i < numDigits(clientInfo.activeClientID); i++) {
		sendbuf[clientInfo.numBytes + i + 1] = findNthDigitAndConvertToChar(i, clientInfo.activeClientID);
	}
	return sendbuf;
}

// Send message to client
void sendMessage(ClientInfo &clientInfo, int &iSendResult, int &iResult, int ID, char* sendbuf) {
	iSendResult = send(clientInfo.ClientSockets[ID], sendbuf, iResult, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(clientInfo.ClientSockets[ID]);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}
	std::cout << "Bytes sent to " << ID << " = " << iSendResult << std::endl;
}

// Connects client socket to listening socket, when a client requests a connection
void waitForIncomingClient(SOCKET &ListenSocket, ClientInfo &clientInfo, int ID) {
	clientInfo.ClientSockets[ID] = accept(ListenSocket, NULL, NULL);

	std::cout << ID << " connected!" << std::endl;
	if (clientInfo.ClientSockets[ID] == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}
}

// Handles incoming message. Returns 1 if client wants to disconnect. Else 0.
int handleReceivedMessage(ClientInfo &clientInfo, int &iResult, int ID) {
	// Normal message
	if (iResult > 0) {
		std::cout << ID << " says: ";
		for (int i = 0; i < iResult; i++) {
			std::cout << clientInfo.recvbuf[i];
		}
		std::cout << std::endl;

		std::cout << "Bytes received from " << ID << " = " << iResult << std::endl;
	}

	// Disconnect
	if (clientInfo.recvbuf[0] == '0') {
		printf("Connection closing...\n");
		return 1;
	}

	// Error
	if (iResult == SOCKET_ERROR) {
		printf("recv failed with error: %d\n", WSAGetLastError());
		closesocket(clientInfo.ClientSockets[ID]);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}
	return 0;
}

// Function that sends a message to a client
void sendToClient(ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID, char* sendbuf) {
	while (true) {
		// It will only send a message, if a message has just been received, 
		// and if this client is not the client who sent the message
		if (clientInfo.receiveSignal == 1 && clientInfo.activeClientID != ID) {
			iResult = clientInfo.numBytes + numDigits(clientInfo.activeClientID) + 1;

			// Adds client ID to the back of the message
			sendbuf = addInformationToMessage(sendbuf, clientInfo);

			// Sends the message with the sender ID to this client
			sendMessage(clientInfo, iSendResult, iResult, ID, sendbuf);

			// Semaphor
			lock.lock();
			clientInfo.receiveSignal = 0;
			clientInfo.activeClientID = -1;
			lock.unlock();
		}
	}
}

void communicate(SOCKET &ListenSocket, ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID) {
	// This line polls the thread until the listening socket receives a connection from a client
	waitForIncomingClient(ListenSocket, clientInfo, ID);

	// When a client has connected, start a thread that takes care of sending messages to that client
	boost::thread sender(sendToClient, std::ref(clientInfo), iResult, iSendResult, ID, clientInfo.recvbuf);

	// Polling loop that takes care of handling incoming messages from this client
	while (true) {
		// This line polls the thread until it receives a message
		iResult = recv(clientInfo.ClientSockets[ID], clientInfo.recvbuf, clientInfo.recvbuflen, 0);
		
		// Semaphor
		lock.lock();
		clientInfo.activeClientID = ID;
		clientInfo.receiveSignal = 1;
		clientInfo.numBytes = iResult;
		lock.unlock();

		// Handle the message. If it returns 1, that means client wants to disconnect 
		if (handleReceivedMessage(clientInfo, iResult, ID) == 1) {
			break;
		}
	}

	// Finish thread
	sender.join();
}

int __cdecl main(void)
{
	// Data used for printing socket errors
	WSADATA wsaData;

	// Variable for checking for socket errors
	int iResult;

	// Listening socket
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
	// Dont need this info anymore
	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// Initialize client info
	ClientInfo clientInfo;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		clientInfo.errors[i] = false;
		clientInfo.ClientSockets[i] = INVALID_SOCKET;
	}

	// Array of threads for each client
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
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>



// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#pragma warning(disable: 4996)


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "7778"

#define NUM_BYTES_ID 4
#define NUM_BYTES_MESSAGE_SIZE 4

boost::mutex lock;

int convertCharToInt(char* buffer, int startIndex) {
	char ID[NUM_BYTES_ID + 1];

	for (int i = 0; i < NUM_BYTES_ID; i++) {
		ID[i] = buffer[startIndex + i];
	}
	ID[NUM_BYTES_ID] = '\0';
	return boost::lexical_cast<int>(ID);
}

// Thread for sending a message to the server
void sendMessage(SOCKET &ConnectSocket, char* sendbuf, int &iResult, std::string &input, bool &error) {
	while (sendbuf[0] != '0') {
		iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			error = true;
			return;
		}

		printf("Bytes Sent: %ld\n", iResult);
		for (int i = 0; i < strlen(sendbuf); i++) {
			std::cout << sendbuf[i];
		}
		std::cout << std::endl;

		std::cout << "Type message: ";
		std::getline(std::cin, input);

		std::copy(input.begin(), input.end(), sendbuf);
		sendbuf[input.size()] = '\0';
	}
}

// Thread that listens to incoming messages from the server
void receiveMessage(SOCKET &ConnectSocket, char* recvbuf, int &iResult, int recvbuflen, bool &error) {
	while (recvbuf[0] != '!') {
		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
		lock.lock();
		if (iResult > 0)
			printf("\nBytes received: %d\n", iResult);
		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

		int ID = convertCharToInt(recvbuf, 0);

		std::cout << "client " << ID << " says: ";

		for (int i = NUM_BYTES_ID + NUM_BYTES_MESSAGE_SIZE; i < iResult; i++) {
			std::cout << recvbuf[i];
		}
		std::cout << std::endl;

		std::cout << "Type message: ";
		lock.unlock();
	}
}

int __cdecl main(int argc, char **argv)
{
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;

	std::string input;

	std::cout << "Type message: ";
	std::getline(std::cin, input);

	// create new c-string with length equal to input + 1
	char *sendbuf = new char[input.size() + 1];

	// copy the content of input to sendbuf, equivalent to sendbuf = input
	std::copy(input.begin(), input.end(), sendbuf);

	// set last element to '\0' to end the c-string
	sendbuf[input.size()] = '\0';

	// print the message that will be sent
	for (int i = 0; i < input.size(); i++) {
		std::cout << input[i];
	}
	std::cout << std::endl;

	// c-string that will contain any incoming message from the server
	char recvbuf[DEFAULT_BUFLEN];
	int iResult;

	// length of incoming message
	int recvbuflen = DEFAULT_BUFLEN;

	// Validate the parameters
	if (argc != 2) {
		printf("usage: %s server-name\n", argv[0]);
		return 1;
	}

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}
	bool error = false;
	// Send an initial buffer

	boost::thread sending(sendMessage, ConnectSocket, sendbuf, iResult, input, error);
	boost::thread receiving(receiveMessage, ConnectSocket, recvbuf, iResult, recvbuflen, error);

	if (error) {
		return 1;
	}

	sending.join();
	receiving.join();
	

	// shutdown the connection since no more data will be sent
	iResult = shutdown(ConnectSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}


	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();
	delete[] sendbuf;

	system("pause");
	return 0;
}
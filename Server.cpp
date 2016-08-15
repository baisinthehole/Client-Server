#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <boost/thread.hpp>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "7778"
#define MAX_CLIENTS 1

void communicate(SOCKET *ClientSockets, SOCKET &ListenSocket, int socketIndex, int &iResult, int &iSendResult, char *recvbuf, int recvbuflen, bool &error) {
	// Receive until the peer shuts down the connection
	while (true) {
		iResult = recv(ClientSockets[socketIndex], recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			for (int i = 0; i < iResult; i++) {
				std::cout << recvbuf[i];
			}
			std::cout << std::endl;
			// Send the buffer to all other clients
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (i != socketIndex) {
					iSendResult = send(ClientSockets[i], recvbuf, iResult, 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						closesocket(ClientSockets[i]);
						WSACleanup();
						error = true;
						return;
					}
					printf("Bytes sent: %d\n", iSendResult);
				}
			}
		}
		if (recvbuf[0] == '0') {
			printf("Connection closing...\n");
			break;
		}
		if (iResult == SOCKET_ERROR) {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSockets[socketIndex]);
			WSACleanup();
			error = true;
			return;
		}
	}
}

int __cdecl main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSockets[MAX_CLIENTS];
	for (int i = 0; i < MAX_CLIENTS; i++) {
		ClientSockets[i] = INVALID_SOCKET;
	}

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult = 0;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

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

	// Accept client sockets
	for (int i = 0; i < MAX_CLIENTS; i++) {
		ClientSockets[i] = accept(ListenSocket, NULL, NULL);
		std::cout << "connected!" << std::endl;
		if (ClientSockets[i] == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
	}

	// Receive until the peer shuts down the connection
	bool error = false;
	boost::thread firstClient(communicate, ClientSockets, ListenSocket, 0, iResult, iSendResult, recvbuf, recvbuflen, error);
	//boost::thread secondClient(communicate, ClientSockets, ListenSocket, 1, iResult, iSendResult, recvbuf, recvbuflen, error);
	if (error) {
		return 1;
	}
	// Have to join before client socket shuts down, to prevent listening to an invalid socket
	firstClient.join();
	//secondClient.join();

	// No longer need server socket
	closesocket(ListenSocket);

	// shutdown the connection since we're done
	for (int i = 0; i < MAX_CLIENTS; i++) {
		iResult = shutdown(ClientSockets[i], SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSockets[i]);
			WSACleanup();
			return 1;
		}
	}

	// cleanup
	for (int i = 0; i < MAX_CLIENTS; i++) {
		closesocket(ClientSockets[i]);
	}
	WSACleanup();

	system("pause");
	return 0;
}
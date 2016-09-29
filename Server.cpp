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

// Length of char buffer for messages (Max length of message)
#define DEFAULT_BUFLEN 512

// Port number to use (make sure this is not a well-known port, like 80)
#define DEFAULT_PORT "7778"

// Maximum number of clients that can be connected to the server at the same time
#define MAX_CLIENTS 3

// Maximum number of messages that can be in the queue
#define QUEUE_LEN 30

// Number of bytes allocated as client ID field in the message buffer
#define NUM_BYTES_ID 4

// Number of bytes allocated telling the message size in the message buffer
#define NUM_BYTES_MESSAGE_SIZE 4

// Mutex to lock down critical parts of the code to avoid race conditions
boost::mutex lock;

// Struct that keeps info useful info about clients and threads
// All variables that are atomic are shared between threads
struct ClientInfo {

	// Array of sockets. One socket per client
	SOCKET ClientSockets[MAX_CLIENTS];

	// Array of error variables. One for each client. Variable is set to 1 if error occurs
	std::atomic<bool> errors[MAX_CLIENTS];

	// States how many clients that have received the current message
	std::atomic<int> numClientsReceivedMessage{ 0 };

	// The number of clients connected to the server
	std::atomic<int> numActiveClients = 0;

	// Hack variable to keep numActiveClients correct
	int hackActiveClients = 0;

	// Queue of messages, to handle lots of messages coming in in a short time interval
	char messageQueue[QUEUE_LEN];

	// Current index to add new messages to the queue
	std::atomic<int> currentQueueIndex{ 0 };

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

// Convert an int to a char, and insert it in into a char array
void convertFromIntToCharAndAddCharToArray(char* buffer, int number, int totalFieldBytes, int startIndex) {

	int numBytes = numDigits(number);

	for (int i = startIndex; i < totalFieldBytes + startIndex - numBytes; i++) {
		buffer[i] = '0';
	}
	int j = numBytes - 1;

	for (int i = startIndex + totalFieldBytes - numBytes; i < startIndex + totalFieldBytes; i++) {
		buffer[i] = findNthDigitAndConvertToChar(j, number);
		j--;
	}
}

// Convert a subset of a char array to an int
int convertCharToInt(char* buffer, int startIndex) {
	char ID[NUM_BYTES_ID + 1];

	for (int i = 0; i < NUM_BYTES_ID; i++) {
		ID[i] = buffer[startIndex + i];
	}
	ID[NUM_BYTES_ID] = '\0';
	return boost::lexical_cast<int>(ID);
}

// Adds client ID to the back of the message 
void addInformationToMessage(ClientInfo &clientInfo, int ID, int numBytesID, int numBytesMessageSize, int numBytesMessage) {
	for (int i = numBytesMessage - 1; i > -1; i--) {
		clientInfo.recvbuf[i + numBytesID + numBytesMessageSize] = clientInfo.recvbuf[i];
	}
	convertFromIntToCharAndAddCharToArray(clientInfo.recvbuf, ID, numBytesID, 0);
	convertFromIntToCharAndAddCharToArray(clientInfo.recvbuf, numBytesMessage, numBytesMessageSize, numBytesID);
}

// Send message to client
void sendMessage(ClientInfo &clientInfo, int &iSendResult, int &iResult, int ID) {
	iSendResult = send(clientInfo.ClientSockets[ID], clientInfo.recvbuf, iResult, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(clientInfo.ClientSockets[ID]);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}
	lock.lock();
	std::cout << "Bytes sent to " << ID << " = " << iSendResult << std::endl;
	lock.unlock();
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

	clientInfo.numActiveClients++;
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
	// Disconnect (TODO)
	if (clientInfo.recvbuf[0] == '!') {
		printf("Connection closing...\n");
		closesocket(clientInfo.ClientSockets[ID]);
		clientInfo.ClientSockets[ID] = INVALID_SOCKET;
		WSACleanup();
		return 1;
	}

	// Error (TODO)
	if (iResult == SOCKET_ERROR) {
		printf("recv failed with error: %d\n", WSAGetLastError());
		closesocket(clientInfo.ClientSockets[ID]);
		WSACleanup();
		clientInfo.errors[ID] = 1;
	}
	return 0;
}

// Add message to the end of the queue
void addMessageToQueue(ClientInfo &clientInfo) {
	clientInfo.messageQueue[clientInfo.currentQueueIndex] = *clientInfo.recvbuf;
	clientInfo.currentQueueIndex++;
}

// Return the message at the start of the queue (first message)
char getMessageFromQueue(ClientInfo &clientInfo) {
	return clientInfo.messageQueue[0];
}

// Rearrange the queue so that all messages are moved one place forward in line. The first message is removed
void removeFromAndRearrangeQueue(ClientInfo &clientInfo) {
	for (int i = 0; i < clientInfo.currentQueueIndex - 1; i++) {
		clientInfo.messageQueue[i] = clientInfo.messageQueue[i + 1];
	}
	clientInfo.currentQueueIndex--;
}

// Thread that sends a message to a client
void sendToClient(ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID) {
	while (true) {
		// It will only send a message if there are still messages in the queue, 
		// and if this client is not the client who sent the message
		if (clientInfo.currentQueueIndex > 0 && clientInfo.numClientsReceivedMessage == 0) {
			// Set the message to be sent equal to the first message in the queue
			lock.lock();
			*clientInfo.recvbuf = getMessageFromQueue(clientInfo);
			lock.unlock();

			// If the message was sent by another client than this, send the message to this client
			if (convertCharToInt(clientInfo.recvbuf, 0) != ID) {
				iResult = NUM_BYTES_ID + NUM_BYTES_MESSAGE_SIZE + convertCharToInt(clientInfo.recvbuf, NUM_BYTES_ID);

				// Sends the message with the sender ID to this client
				sendMessage(clientInfo, iSendResult, iResult, ID);

				// Confirm that this thread has sent its message to its client
				lock.lock();
				clientInfo.numClientsReceivedMessage++;
				lock.unlock();
			}
		}
	}
}

// Thread that listens to incoming messages from clients
void receiveFromClient(SOCKET &ListenSocket, ClientInfo &clientInfo, int &iResult, int &iSendResult, int ID) {
	int numBytesID = 0;
	int numBytesMessage = 0;

	// This line polls the thread until the listening socket receives a connection from a client
	waitForIncomingClient(ListenSocket, clientInfo, ID);

	// When a client has connected, start a thread that takes care of sending messages to that client
	boost::thread sender(sendToClient, std::ref(clientInfo), iResult, iSendResult, ID);

	// Polling loop that takes care of handling incoming messages from this client
	while (true) {
		// This line polls the thread until it receives a message
		iResult = recv(clientInfo.ClientSockets[ID], clientInfo.recvbuf, clientInfo.recvbuflen, 0);

		// Hack solution to keep numActiveClients correct
		lock.lock();
		if (clientInfo.numActiveClients > 1000) {
			clientInfo.numActiveClients = clientInfo.hackActiveClients;
		}
		else {
			clientInfo.hackActiveClients = clientInfo.numActiveClients;
		}
		lock.unlock();

		// Handle the message. If it returns 1, that means client wants to disconnect 
		if (handleReceivedMessage(clientInfo, iResult, ID) == 1) {
			break;
		}
		
		// Only add message to queue if there are other clients to send to
		if (clientInfo.numActiveClients > 1) {
			lock.lock();
			addInformationToMessage(clientInfo, ID, NUM_BYTES_ID, NUM_BYTES_MESSAGE_SIZE, iResult);
			addMessageToQueue(clientInfo);
			lock.unlock();
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
		clients[i] = boost::thread(receiveFromClient, ListenSocket, std::ref(clientInfo), iResult, iSendResult, i);
	}

	while (true) {
		// All clients have received the current message, so the main thread notifies the other threads
		// that they can start working on the next message
		if (clientInfo.numActiveClients > 1 && clientInfo.numClientsReceivedMessage >= clientInfo.numActiveClients - 1) {
			lock.lock();

			removeFromAndRearrangeQueue(clientInfo);

			std::cout << "all clients received message" << std::endl;

			clientInfo.numClientsReceivedMessage = 0;
			lock.unlock();
		}

		// Check if any client is disconnected. If yes, close the thread listening to it
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clientInfo.errors[i] == 1) {

				lock.lock();
				std::cout << i << " disconnected" << std::endl;
				clients[i].join();
				clientInfo.errors[i] = 0;
			}
		}
	}

	system("pause");
	return 0;
}
#include "Receiver.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

// PUBLIC

Receiver::Receiver() : ListenSocket(INVALID_SOCKET), ClientSocket(INVALID_SOCKET), result(nullptr), hints{0},
                       recvbuf("") {
}

Receiver::~Receiver() {
	cout << "[Cleanup]: Receiver destructor called." << endl;

	closeClientSocket();
	closeListenSocket();

	if (result != nullptr) {
		freeaddrinfo(result);
	}
}

// Démarre l'écoute sur le port spécifié
bool Receiver::startListening(const char *port) {
	senderPort = port;

	if (!resolveAddress()) return false;
	if (!setupListener()) return false;

	// Met le socket en mode écoute (attend les connexions)
	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		cerr << "listen failed with error: " << errno << endl;
		close(ListenSocket);
		ListenSocket = INVALID_SOCKET;
		return false;
	}

	cout << "Server listening on port " << senderPort << "..." << endl;

	return true;
}

// Accepte une connexion entrante
bool Receiver::acceptConnection() {
	if (ListenSocket == INVALID_SOCKET) {
		cerr << "Error: Listener not set up. Call startListening first." << endl;
		return false;
	}

	closeClientSocket(); // Ferme l'ancienne connexion client s'il y en avait une
	ClientSocket = accept(ListenSocket, nullptr, nullptr); // Crée un nouveau socket pour la communication.

	if (ClientSocket == INVALID_SOCKET) {
		cerr << "accept failed with error: " << errno << endl;
		return false;
	}

	cout << "Client connected. Handling communication." << endl;

	return true;
}

// Gère la réception de messages du client
bool Receiver::receiveMessages() {
	if (ClientSocket == INVALID_SOCKET) {
		cerr << "Error: No client connected." << endl;
		return false;
	}

	int iResult;
	string receivedData;

	do {
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0); // Tente de recevoir des données du client NOLINT(*-narrowing-conversions)

		if (iResult > 0) {
			receivedData.assign(recvbuf, iResult);

			{
				scoped_lock<mutex> lock(messagesMutex);
				receivedMessages.push(receivedData);
			}

			cout << "[Message received (console)]: " << receivedData << endl;
		} else if (iResult == 0) {
			cout << "Connection closed by peer." << endl;
		} else // iResult < 0 : erreur de socket
		{
			// ECONNRESET est une erreur courante qui signifie que le client a fermé la connexion de force.
			if (errno != ECONNRESET) {
				cerr << "recv failed with error: " << errno << endl;
			}
			closeClientSocket();
			return false;
		}
	} while (iResult > 0); // Continue à recevoir tant qu'il y a des données et la connexion n'est pas fermée

	closeClientSocket();

	return true;
}

string Receiver::getReceivedMessage() {
	// Verrouille le mutex pour assurer l'atomicité de la section critique
	scoped_lock<mutex> lock(messagesMutex);

	if (receivedMessages.empty()) {
		return "";
	}

	string message = receivedMessages.front();
	receivedMessages.pop();

	return message;
}

// PRIVATE

// Résout l'adresse locale (toutes les interfaces) et le port d'écoute
bool Receiver::resolveAddress() {
	memset(&hints, 0, sizeof(hints)); // Réinitialise la structure hints
	hints.ai_family = AF_INET; // Spécifie la famille d'adresses IPv4
	hints.ai_socktype = SOCK_STREAM; // Spécifie un socket de flux (TCP)
	hints.ai_protocol = IPPROTO_TCP; // Spécifie le protocole TCP
	hints.ai_flags = AI_PASSIVE; // Indique que le socket sera utilisé pour écouter

	// Appelle getaddrinfo pour obtenir la structure d'adresse pour l'écoute
	// 'nullptr' indique que nous voulons écouter sur toutes les interfaces locales

	int iResult = getaddrinfo(nullptr, senderPort.c_str(), &hints, &result);
	if (iResult != 0) {
		cerr << "getaddrinfo failed with error: " << iResult << endl;
		return false;
	}

	return true;
}

// Crée le socket d'écoute et le lie au port
bool Receiver::setupListener() {
	// Crée le socket d'écoute en utilisant les informations résolues
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		cerr << "socket failed with error: " << errno << endl;
		freeaddrinfo(result); // IMPORTANT
		result = nullptr;
		return false;
	}

	// Lie le socket au port spécifié
	if (bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
		cerr << "bind failed with error: " << errno << endl;
		freeaddrinfo(result); // IMPORTANT
		result = nullptr;
		close(ListenSocket);
		ListenSocket = INVALID_SOCKET;
		return false;
	}

	freeaddrinfo(result);
	result = nullptr;

	return true;
}

void Receiver::closeClientSocket() {
	if (ClientSocket != INVALID_SOCKET) {
		cout << "Closing client socket..." << endl;

		if (shutdown(ClientSocket, SHUT_WR) == SOCKET_ERROR) {
			cerr << "shutdown failed with error: " << errno << endl;
		}

		close(ClientSocket);
		ClientSocket = INVALID_SOCKET;
	}
}

void Receiver::closeListenSocket() {
	if (ListenSocket != INVALID_SOCKET) {
		cout << "Closing listen socket..." << endl;
		close(ListenSocket);
		ListenSocket = INVALID_SOCKET;
	}
}

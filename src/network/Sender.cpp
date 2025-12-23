#include "Sender.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

// PUBLIC

Sender::Sender() : ConnectSocket(INVALID_SOCKET), result(nullptr), ptr(nullptr), hints{0} {
}

Sender::~Sender() {
	cout << "[Cleanup]: Sender destructor called." << endl;

	closeConnectSocket();

	if (result != nullptr) {
		freeaddrinfo(result);
	}
}

// Établit la connexion
bool Sender::connectToReceiver(const char *address, const char *port) {
	receiverAddress = address;
	receiverPort = port;

	if (!resolveAddress()) return false;
	if (!attemptConnect()) return false;

	cout << "Successfully connected to " << address << ":" << port << endl;

	return true;
}

// Envoie un message au pair distant
int Sender::sendMessage(const char *message) const {
	if (ConnectSocket == INVALID_SOCKET) {
		cerr << "Error: Not connected to a server." << endl;
		return SOCKET_ERROR;
	}

	const int len = static_cast<int>(strlen(message));
	const int iResult = send(ConnectSocket, message, len, 0); // NOLINT(*-narrowing-conversions)

	if (iResult == SOCKET_ERROR) {
		cerr << "send failed with error: " << errno << endl;
		return SOCKET_ERROR;
	}

	cout << "Bytes Sent: " << iResult << endl;

	return iResult; // Retourne le nombre d'octets envoyés
}

// PRIVATE

// Tente de résoudre le nom de l'hôte et du port en adresses utilisables
bool Sender::resolveAddress() {
	memset(&hints, 0, sizeof(hints)); // Réinitialise la structure hints
	hints.ai_family = AF_UNSPEC; // Ne spécifie pas de famille (IPv4 ou IPv6)
	hints.ai_socktype = SOCK_STREAM; // Spécifie un socket de flux (TCP)
	hints.ai_protocol = IPPROTO_TCP; // Spécifie le protocole TCP

	// Appelle getaddrinfo pour obtenir la liste des adresses disponibles pour la connexion
	if (const int iResult = getaddrinfo(receiverAddress.c_str(), receiverPort.c_str(), &hints, &result); iResult != 0) {
		cerr << "getaddrinfo failed with error: " << iResult << endl;
		return false;
	}

	return true;
}

// Tente d'établir une connexion au serveur en utilisant la liste d'adresses (result)
bool Sender::attemptConnect() {
	// Ferme l'ancien socket s'il existe
	closeConnectSocket();

	// Parcourt la liste d'adresses (result)
	for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		// Crée un socket en utilisant les informations de l'adresse courante
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (ConnectSocket == INVALID_SOCKET) {
			cerr << "socket failed with error: " << errno << endl;

			// Tente l'adresse suivante
			continue;
		}

		// Tente de se connecter au serveur distant
		if (connect(ConnectSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR) {
			closeConnectSocket();

			// Tente l'adresse suivante
			continue;
		}

		break;
	}

	freeaddrinfo(result); // IMPORTANT
	result = nullptr;

	if (ConnectSocket == INVALID_SOCKET) {
		cerr << "Unable to connect to server!" << endl;

		return false;
	}

	return true;
}

void Sender::closeConnectSocket() {
	if (ConnectSocket != INVALID_SOCKET) {
		cout << "Closing connection socket..." << endl;

		if (shutdown(ConnectSocket, SHUT_WR) == SOCKET_ERROR) {
			cerr << "shutdown failed with error: " << errno << endl;
		}

		close(ConnectSocket);
		ConnectSocket = INVALID_SOCKET;
	}
}

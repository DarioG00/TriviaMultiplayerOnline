#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>

// ////////////////////////////////////////////////////////////////////////////////
// MACRO UTILI									  								  /
// ////////////////////////////////////////////////////////////////////////////////

#define ADDR "127.0.0.1"			// indirizzo del server
#define TYPE_SIZE 8					// lunghezza del tipo di un messaggio
#define TEXT_SIZE 1024				// massima lunghezza del contenuto di un messaggio
#define NICKNAME_SIZE 16			// massima lunghezza del nickname
#define THEME_SIZE 32				// massima lunghezza del nome del tema

// ////////////////////////////////////////////////////////////////////////////////
// FUNZIONI DEL GIOCO								  							  /
// ////////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------------------------
// sceltaOpzioneMenu:	Funzione invocata all'avvio del client. Ritorna 1 se il
//			client ha deciso di cominciare un quiz, altrimenti 2 se
//			ha deciso di uscire.
// --------------------------------------------------------------------------------
int sceltaOpzioneMenu(){
	char scelta[TEXT_SIZE];
	int ret;

	printf("\n");
	printf("Trivia Quiz\n");
	printf("++++++++++++++++++++++++++++++\n");
	printf("Menù:\n");
	printf("1 - Comincia una sessione di Trivia\n");
	printf("2 - Esci\n");
	printf("++++++++++++++++++++++++++++++\n");
	
	do{
		printf("La tua scelta: ");
		fgets(scelta, TEXT_SIZE, stdin);
		scelta[strcspn(scelta, "\n")] = 0;
				
		if(strlen(scelta) > 1){
			printf("Formato opzione errata: inserire opzione '1' oppure opzione '2'\n");
			continue;
		}
		
		ret = atoi(scelta);
		
		if(ret == 1){
			break;
		}else if(ret == 2){
			printf("Trivia in chiusura...\n");
			break;
		}
		
		printf("Formato opzione errata: inserire opzione '1' oppure opzione '2'\n");
	}while(1);

	return ret;
}			

// --------------------------------------------------------------------------------
// inserimentoNickname:	Funzione invocata quando il client si connette al
// 			server con successo. Ritorna il nickname digitato
//			dal client in 'buffer' passato come argomento.
// --------------------------------------------------------------------------------
void inserimentoNickname(char* buffer){

	printf("\n");
	printf("Trivia Quiz\n");
	printf("++++++++++++++++++++++++++++++\n");
	
	do{
		printf("Scegli un nickname (deve essere univoco): ");
		fgets(buffer, TEXT_SIZE, stdin);
		buffer[strcspn(buffer, "\n")] = 0;

		if(strlen(buffer) > NICKNAME_SIZE || (strlen(buffer) < 1)){
			printf("Formato opzione errata.\n");
			continue;
		}
		
		break;
	}while(1);
}

// --------------------------------------------------------------------------------
// mostraListaTemi:	Funzione invocata quando il client si è registrato per
// 			giocare ad un quiz.Ritorna l'indice del tema di un quiz
//			che il client ha deciso di cominciare,altrimenti -1 se ha
//			digitato un opzione scorretta.
// --------------------------------------------------------------------------------
int mostraListaTemi(const char* lista_quiz, const int n, char* tema_scelto){
	char tema[n][THEME_SIZE];
	char scelta[TEXT_SIZE];	
	int offset = 0;
	int ret = -1;

	printf("\n");
	printf("Quiz disponibili\n");
	printf("++++++++++++++++++++++++++++++\n");

	for(int i=0; i < n; i++){
		memcpy(tema[i], lista_quiz+offset, THEME_SIZE);
		offset += THEME_SIZE;
		printf("%d - %s\n", i+1, tema[i]);
	}
	
	printf("++++++++++++++++++++++++++++++\n");
	
	do{
		printf("La tua scelta: ");
		fgets(scelta, TEXT_SIZE, stdin);
		scelta[strcspn(scelta, "\n")] = 0;
		
		if((ret = atoi(scelta)) == 0){
			printf("Formato opzione errata. Inserire l'indice numerico di un tema.\n");
			continue;
		}
		
		if(ret > n){
			printf("Opzione invalida. Indice numerico del tema inesistente.\n");
			continue;
		}
		
		strcpy(tema_scelto, tema[ret-1]);
		
		break;
	}while(1);
	
	return ret;
}

// --------------------------------------------------------------------------------
// rispondiDomanda:	Funzione invocata ogni volta che arriva dal server una
//			domanda del quiz. Ritorna in 'risposta' la risposta
//			data dal client.
// --------------------------------------------------------------------------------
void rispondiDomanda(const char *tema, const char* domanda, char *risposta){

	printf("\n");
	printf("Quiz - %s\n", tema);
	printf("++++++++++++++++++++++++++++++\n");
	
	printf("%s", domanda);
	
	printf("\n");
	
	do{
		printf("Risposta: ");
		fgets(risposta, TEXT_SIZE, stdin);
		risposta[strcspn(risposta, "\n")] = 0;
		
		if((strlen(risposta) > TEXT_SIZE) || (strlen(risposta) < 1)){
			printf("Risposta non valida.\n");
			continue;
		}
		
		break;
	}while(1);
}

// --------------------------------------------------------------------------------
// mostraEsitoRisposta:	Funzione invocata ogni volta che arriva dal server un
//			esito della risposta data durante il quiz.
// --------------------------------------------------------------------------------
void mostraEsitoRisposta(char* esito){
	printf("\n");
	printf("%s", esito);
	printf("\n");
}

// --------------------------------------------------------------------------------
// inserimentoComando:	Funzione invocata dopo ogni volta che arriva dal server un
//			esito della risposta data durante il quiz.
// --------------------------------------------------------------------------------
void inserimentoComando(char* type_msg, char* command){
	printf("\n");
	
	do{
		printf("Inserisci comando: ");
		fgets(command, TEXT_SIZE, stdin);
		command[strcspn(command, "\n")] = 0;
		
		if(strcmp(command, "next") == 0){
			strcpy(type_msg, "NXT_REQ");
			break;
		}
		
		if(strcmp(command, "show score") == 0){
			strcpy(type_msg, "SHW_SCR");
			break;
		}
		
		if(strcmp(command, "endquiz") == 0){
			strcpy(type_msg, "END_REQ");
			break;
		}
		
		printf("\nComando non valido.\n");
		printf("Comandi possibili:\n");
		printf("- 'next' : passa alla prossima domanda.\n");
		printf("- 'show score' : mostra i punteggi attuali dei giocatori.\n");
		printf("- 'endquiz' : abbandono del gioco.\n\n");
	}while(1);
}

// --------------------------------------------------------------------------------
// mostraPunteggi:	Funzione invocata ogni volta che arrivano dal server i
//			punteggi dei giocatori negli 'n' temi diponibili attraverso 
//			il comando 'show score' inviato dal client stesso.
// --------------------------------------------------------------------------------
void mostraPunteggi(const char* punteggi, const int n){

	int offset = 0;
	int punteggio;
	char tema[THEME_SIZE] = {0}, tema_corrente[THEME_SIZE] = {0}, giocatore[NICKNAME_SIZE] = {0};

	printf("\n");
	printf("Punteggio dei giocatori\n");
	printf("++++++++++++++++++++++++++++++\n");

	for(int j=0; (j < n) && (offset < TEXT_SIZE); j++){
	
		memcpy(tema, punteggi+offset, THEME_SIZE);
		offset += THEME_SIZE;
		
		if(strcmp(tema_corrente, tema) != 0){
			printf("\n");
			strcpy(tema_corrente, tema);
			printf("Tema '%s'\n", tema_corrente);
		}
		
		memcpy(giocatore, punteggi+offset, NICKNAME_SIZE);
		offset += NICKNAME_SIZE;
		
		memcpy(&punteggio, punteggi+offset, sizeof(punteggio));
		punteggio = ntohl(punteggio);
		offset += sizeof(punteggio);
		
		printf("- %s %d\n", giocatore, punteggio);
	}
	
	printf("++++++++++++++++++++++++++++++\n");
}

// --------------------------------------------------------------------------------
// riceviMessaggio:	Funzione invocata ogni volta che il client attende un
// 			messaggio dal server. Ritorna 1 se la ricezione è avvenuta
//			con successo, -1 altrimenti.
// --------------------------------------------------------------------------------
int riceviMessaggio(const int socket, char* type, unsigned char* msg){
	int bytes_read;
	int msg_length;
	
	if((bytes_read = recv(socket, type, TYPE_SIZE-1, 0)) <= 0){
		// SERVER DISCONNESSO
		return -1;
	}
	
	type[bytes_read] = '\0';

	// RICEZIONE DELLA DIMENSIONE IN BYTE DEL CONTENUTO DEL MESSAGGIO
	if((bytes_read = recv(socket, &msg_length, sizeof(msg_length), 0)) <= 0){
		// SERVER DISCONNESSO
		return -1;
	}
	
	// RICEZIONE DEL CONTENUTO DEL MESSAGGIO (SE C'E')
	msg_length = ntohl(msg_length);
	if(msg_length > 0){
		if((bytes_read = recv(socket, msg, msg_length, 0)) <= 0){
			// SERVER DISCONNESSO
			return -1;
		}
	
		msg[bytes_read] = '\0';
	}else{
		memset(msg, 0, TEXT_SIZE);
	}
	
	// OK
	return 1;
}

// --------------------------------------------------------------------------------
// inviaMessaggio:	Funzione invocata ogni volta che il client invia un
//			messaggio al server. Ritorn -1 se c'è errore di invio
// 			messaggio al server (server disconesso), 1 altrimenti.
// --------------------------------------------------------------------------------
int inviaMessaggio(const int socket, const char* type, const unsigned char* msg, const int msg_length){
	int net_msg_length = htonl(msg_length);
	
	// Invio del tipo di messaggio
	if(send(socket, type, TYPE_SIZE-1, 0) <= 0){
		// SERVER DISCONNESSO
		return -1;
	}
	
	// Invio del messaggio (DIMENSIONE IN BYTE + CONTENUTO)
	if(send(socket, &net_msg_length, sizeof(net_msg_length), 0) <= 0){
		// SERVER DISCONNESSO
		return -1;
	}

	if(msg_length > 0){		
		if(send(socket, msg, msg_length, 0) <= 0){
			// SERVER DISCONNESSO
			return -1;
		}
	}
	
	return 1;
}

int main(int argc, char* argv[]){

	// ------------------------------------------------------
	// VARIABILI E INIZIALIZZAZIONI
	int client_fd;
	int port;
	struct sockaddr_in server_addr;
	char type_msg[TYPE_SIZE] = {0};
	char rcv_msg[TEXT_SIZE] = {0};
	char snd_msg[TEXT_SIZE] = {0};
	int num_quiz = 0;			// numero dei quiz disponibili
	int id_quiz;				// id del tema a cui sta giocando il client
	char tema[THEME_SIZE] = {0};		// nome del tema  a cui sta giocando il client
	int num_punteggi = 0;			// numero dei punteggi disponibili
	// ------------------------------------------------------
	
	// ------------------------------------------------------
	// CONTROLLO INFO. INPUT DAL TERMINALE
	if(argc == 2){
		port = atoi(argv[1]);
	}else{
		fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	// ------------------------------------------------------
	
	// ------------------------------------------------------
	// AVVIO DEL GIOCO
	if(sceltaOpzioneMenu() == 2){
		return 0;
	}
	// ------------------------------------------------------
	
	// ------------------------------------------------------
	// SOCKET TCP PER COMUNICARE CON IL SERVER
	if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
		perror("Errore nella creazione del socket");
		exit(EXIT_FAILURE);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	if(inet_pton(AF_INET, ADDR, &server_addr.sin_addr) <= 0){
		perror("Errore nella conversione dell'indirizzo IP del server");
		close(client_fd);
		exit(EXIT_FAILURE);
	}
	
	if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Errore nella connessione al server");
		close(client_fd);
		exit(EXIT_FAILURE);
	}
	// ------------------------------------------------------

	// ------------------------------------------------------
	// CONTROLLO DEI MESSAGGI CON IL SERVER
	
	while(1){
		
		// Ricezione del tipo del nuovo messaggio
		if(riceviMessaggio(client_fd, type_msg, (unsigned char*)rcv_msg) < 0){
			// SERVER DISCONNESSO
			perror("Errore: il server non è più raggiungibile");
			close(client_fd);
			exit(EXIT_FAILURE);
		}
		
		// Ricezione dal server che il numero massimo di giocatori è stato raggiunto
		if(strcmp(type_msg, "CLN_ERR") == 0){
			printf("%s\n", rcv_msg);
			close(client_fd);
			return 0;
		}
		
		// Ricezione dal server di registrare un nickname
		if(strcmp(type_msg, "NCK_REQ") == 0){
		
			if(strlen(rcv_msg) > 0){
				printf("%s\n", rcv_msg);
			}
			
			strcpy(type_msg, "NCK_ANS");
			inserimentoNickname(snd_msg);
			
			// Invio del nickname di registrazione
			if(inviaMessaggio(client_fd, type_msg, (unsigned char*)snd_msg, strlen(snd_msg)) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}

		// Ricezione del numero dei temi di quiz
		if(strcmp(type_msg, "THM_NMB") == 0){
			
			memcpy(&num_quiz, rcv_msg, sizeof(num_quiz));	// salvataggio del numero dei temi che
			num_quiz = ntohl(num_quiz);			// si aspetta il client
			
			strcpy(type_msg, "THM_LST");
			
			// Invio della richiesta della lista dei temi
			if(inviaMessaggio(client_fd, type_msg, NULL, 0) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione della lista dei temi di quiz
		if(strcmp(type_msg, "THM_LST") == 0){
			int net_id_quiz;
		
			strcpy(type_msg, "THM_REQ");
			id_quiz = mostraListaTemi(rcv_msg, num_quiz, tema);
			net_id_quiz = htonl(id_quiz);
			
			// Invio al server del tema scelto dal client
			if(inviaMessaggio(client_fd, type_msg, (unsigned char*)&net_id_quiz, sizeof(net_id_quiz)) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione di errore del tema richiesto perchè già completato
		if(strcmp(type_msg, "THM_CMP") == 0){
			
			printf("%s\n", rcv_msg);
		
			strcpy(type_msg, "THM_LST");
			
			// Invio della richiesta della lista dei temi
			if(inviaMessaggio(client_fd, type_msg, NULL, 0) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione di una domanda di un quiz
		if(strcmp(type_msg, "QST_REQ") == 0){
		
			strcpy(type_msg, "CHK_REQ");
			rispondiDomanda(tema, rcv_msg, snd_msg);
			
			// Invio al server dell'esito della risposta la quiz
			if(inviaMessaggio(client_fd, type_msg, (unsigned char*)snd_msg, strlen(snd_msg)) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione dell'esito di una risposta al quiz
		if(strcmp(type_msg, "CHK_ANS") == 0){
			
			mostraEsitoRisposta(rcv_msg);
			
			inserimentoComando(type_msg, snd_msg);
			
			// Invio al server di un comando
			if(inviaMessaggio(client_fd, type_msg, NULL, 0) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione del numero dei punteggi
		if(strcmp(type_msg, "SCR_NMB") == 0){
			memcpy(&num_punteggi, rcv_msg, sizeof(num_punteggi));	// salvataggio del numero dei punteggi che
			num_punteggi = ntohl(num_punteggi);			// si aspetta il client
			
			strcpy(type_msg, "SCR_LST");
			
			// Invio della richiesta della lista dei punteggi
			if(inviaMessaggio(client_fd, type_msg, NULL, 0) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
			
		}
		
		// Ricezione dei punteggi dei giocatori
		if(strcmp(type_msg, "SHW_SCR") == 0){
			
			mostraPunteggi(rcv_msg, num_punteggi);
			
			inserimentoComando(type_msg, snd_msg);
			
			// Invio al server di un comando
			if(inviaMessaggio(client_fd, type_msg, NULL, 0) < 0){
				// SERVER DISCONNESSO
				perror("Errore: il server non è più raggiungibile");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			continue;
		}
		
		// Ricezione di fine tema di un quiz
		if(strcmp(type_msg, "END_CLN") == 0){
		
			printf("%s\n", rcv_msg);
			
			// Ritorno al menu principale
			close(client_fd);
			
			if(sceltaOpzioneMenu() == 2){
				return 0;
			}
			
			// Collegamento di nuovo al server
			if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0){
				perror("Errore nella creazione del socket");
				exit(EXIT_FAILURE);
			}
			
			if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
				perror("Errore nella connessione al server");
				close(client_fd);
				exit(EXIT_FAILURE);
			}
			
			continue;
		}
	}
	// ------------------------------------------------------
	
	close(client_fd);
	return 0;
}

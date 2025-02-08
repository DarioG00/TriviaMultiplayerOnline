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
#include <signal.h>

// ////////////////////////////////////////////////////////////////////////////////
// MACRO UTILI									  /
// ////////////////////////////////////////////////////////////////////////////////
#define PORT 1234					// porta del server
#define ADDR "127.0.0.1"			// indirizzo del server
#define MAX_CLIENTS 10				// massimo delle connessioni client gestibili
#define MAX_QUIZ 2					// massimo numero dei temi di quiz
#define MAX_QUESTIONS 5				// massimo numero di domande per ogni tema di quiz
#define MAX_SESSIONS (MAX_CLIENTS*MAX_QUIZ)	// massimo numero di sessioni delle partite
#define NICKNAME_SIZE 16			// massima lunghezza del nickname
#define THEME_SIZE 32				// massima lunghezza del nome del tema
#define TEXT_SIZE 1024				// massima lunghezza del contenuto di un messaggio
#define TYPE_SIZE 8					// lunghezza della stringa del tipo di un messaggio

// ////////////////////////////////////////////////////////////////////////////////
// STRUTTURE DATI E VARIABILI PER IL GIOCO					  					  /
// ////////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------
// Socket del server:
int server_fd;
// ----------------------------------------------------

// ----------------------------------------------------
// Giocatori connessi:
struct Giocatore{
	uint32_t socket;
	uint8_t nickname[NICKNAME_SIZE];
	uint32_t tema;
};

struct Giocatore giocatori[MAX_CLIENTS] = {0};
int num_giocatori = 0;
// ----------------------------------------------------

// ----------------------------------------------------
// Lista dei temi del quiz:
uint8_t lista_temi[MAX_QUIZ][THEME_SIZE];
// ----------------------------------------------------

// ----------------------------------------------------
// Sessioni di quiz dei giocatori:
struct SessioneQuiz {
	uint32_t tema;
	uint8_t nickname[NICKNAME_SIZE];
	uint32_t punteggio;
	uint32_t domanda;
	struct SessioneQuiz* next;
};

struct SessioneQuiz* sessioni = NULL;
int num_sessioni = 0;
// ----------------------------------------------------

// ////////////////////////////////////////////////////////////////////////////////
// FUNZIONI DEL GIOCO								    					   	  /
// ////////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------------------------
// nicknameEsistente:	Funzione invocata ogni volta che il server riceve dal
//						client il nickname richiesto. Ritorna 0 se il nickname
//						del giocatore connesso  non esiste, 0 altrimenti.
// --------------------------------------------------------------------------------
int nicknameEsistente(const char* nickname){
	
	int result = 0;
	
	for(int i=0; i < MAX_CLIENTS; i++){
		if(strcmp((char*)giocatori[i].nickname, nickname) == 0){
			result = 1;
			break;
		}
	}	
	
	return result;
}

// --------------------------------------------------------------------------------
// creaSessione:	Funzione invocata dal server ogni volta che
//					un client inizia una sessione di quiz ad un
//					tema. La funzione crea un nuovo nodo di tipo
//					'SessioneQuiz' e ne restituisce l'indirizzo.
// --------------------------------------------------------------------------------
struct SessioneQuiz* creaSessione(const int tema, const char* g){
	struct SessioneQuiz* s = (struct SessioneQuiz*)malloc(sizeof(struct SessioneQuiz));
	s->tema = (uint32_t)tema;
	strcpy((char*)s->nickname, g);
	s->punteggio = (uint32_t)0;
	s->domanda = (uint32_t)1;
	s->next = NULL;
	
	return s;
}

// --------------------------------------------------------------------------------
// ottieniSessione:	Funzione invocata dal server ogni volta che un
//					client esce da una sessione di quiz. Aggiorna 
//					nel file 'quiz_files/sessions.txt' la sessione
//					indicata dai parametri 'tema' e 'nickname'.
// --------------------------------------------------------------------------------
struct SessioneQuiz* ottieniSessione(const int tema, const char* nickname){
	struct SessioneQuiz* s = sessioni;
	while(s != NULL){
		if((s->tema == tema) && (strcmp((char*)s->nickname, nickname) == 0)){
			break;
		}
		s = s->next;
	}
	return s;
}

// --------------------------------------------------------------------------------
// aggiungiSessioneInLista:	Funzione invocata dal server ogni volta che
//							un client inizia una sessione di quiz ad un
//							tema. La funzione inserisce in ordine crescente
//							per tema 'tema' e decrescente per 'punteggio'
//							ogni sessione di quiz.
// --------------------------------------------------------------------------------
void aggiungiSessioneInLista(struct SessioneQuiz* s){
	struct SessioneQuiz *p = sessioni, *precedente = NULL;
	
	while((p != NULL) && (s->tema >= p->tema)){
		if((s->tema == p->tema) && (s->punteggio >= p->punteggio)){
			break;
		}
		precedente = p;
		p = p->next;
	}
	s->next = p;
	
	if(precedente != NULL){
		// Inserimento o in coda o in mezzo
		precedente->next = s;
	}else{
		// Inserimento in testa
		sessioni = s;
	}
	
	num_sessioni++;
	
	return;
}

// --------------------------------------------------------------------------------
// aggiungiSessioneInLista:	Funzione invocata dal server ogni volta che
//							un client fa un punto al quiz in una sessione.
//							La funzione riordina la lista sessioni.
// --------------------------------------------------------------------------------
void aggiornaSessioneInLista(struct SessioneQuiz* s){
	struct SessioneQuiz *p = sessioni, *precedente = NULL;
	
	while(p != NULL){
		if((s->tema == p->tema) && (strcmp((char*)s->nickname, (char*)p->nickname) == 0)){
			break;
		}
		precedente = p;
		p = p->next;
	}
	
	if(precedente != NULL){
		// Tolgo o dalla coda o dal mezzo
		precedente->next = p->next;
	}else{
		// Tolgo dall testa
		sessioni = p->next;
	}
	
	num_sessioni--;
	
	aggiungiSessioneInLista(p);
	
	return;
}

// --------------------------------------------------------------------------------
// caricaSessioni:	Funzione invocata ogni volta all'avvio del server per
//					caricare le sessioni dei temi di quiz presenti nel
//					file 'quiz_files/sessions.txt'.
// --------------------------------------------------------------------------------
void caricaSessioni(){
	FILE *file;
	char buffer[TEXT_SIZE];
	struct SessioneQuiz* s;
	int tema, domanda, punteggio;
	char nickname[NICKNAME_SIZE] = {0};
	
	// caricamento lista dei temi
	if((file = fopen("./quiz_files/sessions.txt", "r")) == NULL){
		perror("Errore nell'apertura del file delle sessioni");
		exit(EXIT_FAILURE);
	}
	
	while(!feof(file)){
		memset(buffer, 0, TEXT_SIZE);
		memset(nickname, 0, NICKNAME_SIZE);
		
		fgets(buffer, TEXT_SIZE, file);

		if(feof(file))break;
		
		buffer[strcspn(buffer,"\n")] = 0;
		
		// Scomposizione tema, nickname, domanda
		// e punteggio di una sessione.
		tema = atoi(strtok(buffer, ":"));
		strcpy(nickname, strtok(NULL, ":"));
		domanda = atoi(strtok(NULL, ":"));
		punteggio = atoi(strtok(NULL, ":"));
		
		// Crea nodo con stato della sessione
		// e aggiungi alla lista delle sessioni.
		s = creaSessione(tema, nickname);
		s->domanda = (uint32_t)domanda;
		s->punteggio = (uint32_t)punteggio;

		aggiungiSessioneInLista(s);
	}

	fclose(file);	
}

// --------------------------------------------------------------------------------
// aggiornaSessioneInFile:	Funzione invocata dal server ogni volta che un
//							client esce da una sessione di quiz. Aggiorna 
//							nel file 'quiz_files/sessions.txt' la sessione
//							indicata dai parametri 'tema' e 'nickname'.
// --------------------------------------------------------------------------------
void aggiornaSessioneInFile(const struct SessioneQuiz* s){
	FILE *file, *filetmp;
	char buffer[TEXT_SIZE];
	int tema, domanda, punteggio;
	char nickname[NICKNAME_SIZE];
	int aggiornata = 0;
	
	// Apertura del file da leggere
	if((file = fopen("./quiz_files/sessions.txt", "r")) == NULL){
		perror("Errore nell'apertura del file delle sessioni");
		exit(EXIT_FAILURE);
	}
	// Apertura del file da modificare
	if((filetmp = fopen("./quiz_files/tmp_sessions.txt", "a")) == NULL){
		perror("Errore nell'apertura del file delle sessioni");
		exit(EXIT_FAILURE);
	}
	
	while(!feof(file)){

		fgets(buffer, TEXT_SIZE, file);

		if(feof(file))break;

		// Scomposizione tema e nickname una sessione.
		tema = atoi(strtok(buffer, ":"));
		strcpy(nickname, strtok(NULL, ":"));
		domanda = atoi(strtok(NULL, ":"));
		punteggio = atoi(strtok(NULL, ":"));
		
		
		memset(buffer, 0, TEXT_SIZE);
		if((tema == s->tema) && (strcmp(nickname, (char*)s->nickname) == 0)){
			// scrittura dell'entrata di una sessione aggiornata
			sprintf(buffer,"%d:%s:%d:%d\n", (int)s->tema, (char*)s->nickname, (int)s->domanda, (int)s->punteggio);
			fprintf(filetmp,"%s", buffer);
			aggiornata = 1;
		}else{
			// copia dell' entrata di una sessione
			sprintf(buffer,"%d:%s:%d:%d\n", tema, nickname, domanda, punteggio);
			fprintf(filetmp,"%s", buffer);
		}
	}
	
	fclose(file);
	
	if(!aggiornata){
		// scrittura dell'entrata di una sessione aggiornata
		// se non era mai stata salvata prima nel file.
		sprintf(buffer,"%d:%s:%d:%d\n", (int)s->tema, (char*)s->nickname, (int)s->domanda, (int)s->punteggio);
		fprintf(filetmp,"%s", buffer);
	}

	fclose(filetmp);
	
	// Rimuovo il vecchio file e rinomino quello nuovo
	remove("./quiz_files/sessions.txt");
	rename("./quiz_files/tmp_sessions.txt", "./quiz_files/sessions.txt");
}

// --------------------------------------------------------------------------------
// mostraListaTemiQuiz:	Funzione invocata all'avvio del server
//						per mostrare la lista dei temi di quiz disponibili.
// --------------------------------------------------------------------------------
void mostraListaTemiQuiz(){
	FILE *file;
	char buffer[THEME_SIZE];
	char* separator;
	int id;
	
	// caricamento lista dei temi
	if((file = fopen("./quiz_files/quiz.txt", "r")) == NULL){
		perror("Errore nell'apertura del file della lista dei temi");
		exit(EXIT_FAILURE);
	}
	
	while(!feof(file)){

		fgets(buffer, THEME_SIZE, file);

		if(feof(file))break;
		
		buffer[strcspn(buffer,"\n")] = 0;

		// Scomposizione id e tema del quiz
		separator = strchr(buffer, ':');
		*separator = '\0';
		sscanf(buffer,"%d", &id);
		strcpy((char*)lista_temi[id-1], separator+1);
	}

	fclose(file);
	
	
	printf("Trivia Quiz\n");
	printf("++++++++++++++++++++++++++++++\n");
	printf("Temi:\n");
	for(int i=0; i < MAX_QUIZ; i++){
		printf("%d - %s\n", i+1, lista_temi[i]);
	}
	printf("++++++++++++++++++++++++++++++\n");
	printf("\n");
}

// --------------------------------------------------------------------------------
// mostraClientConnessi:	Funzione invocata da 'mostraStatoGioco()' per
// 							mostrare i client connessi.
// --------------------------------------------------------------------------------
void mostraClientConnessi(){

	printf("Partecipanti (%d)\n", num_giocatori);
	for(int i=0; i < MAX_CLIENTS; i++){
		if(strlen((char*)giocatori[i].nickname) > 0){
			printf("- %s\n", (char*)giocatori[i].nickname);
		}
	}
	printf("\n");
}

// --------------------------------------------------------------------------------
// mostraPunteggi:	Funzione invocata da 'mostraStatoGioco()' per mostrare
// 					i punteggi nei vari temi dei giocatori.
// --------------------------------------------------------------------------------
void mostraPunteggi(){
	
	struct SessioneQuiz* s = sessioni;
	for(int i=0; i < MAX_QUIZ; i++){
		
		printf("Punteggio tema %d\n", i+1);
		while((s != NULL) && (s->tema == (i+1))){
			printf("- %s %d\n", (char*)s->nickname, (int)s->punteggio);
			s = s->next;
		}
		printf("\n");
	}
}

// --------------------------------------------------------------------------------
// serializzaPunteggi:	Funzione invocata dal server ogni volta che il client
//						richiede la lista dei punteggi dei giocatori. Serializza
//						in 'buffer' solamente il nome del tema con i nickname ed
//						i punteggi.
// --------------------------------------------------------------------------------
void serializzaPunteggi(unsigned char* buffer){
	struct SessioneQuiz* s = sessioni;
	int offset = 0;
	uint32_t punteggio;
	while((s != NULL) && (offset < TEXT_SIZE)){
		
		memcpy(buffer+offset, lista_temi[s->tema-1], THEME_SIZE);
		offset += THEME_SIZE;
		
		memcpy(buffer+offset, s->nickname, NICKNAME_SIZE);
		offset += NICKNAME_SIZE;
	
		punteggio = (uint32_t)htonl(s->punteggio);
		memcpy(buffer+offset, &punteggio, sizeof(punteggio));
		offset += sizeof(punteggio);

		s = s->next;
	}
}

// --------------------------------------------------------------------------------
// mostraQuizCompletati:	Funzione invocata ogni volta che il client manda un
// 							messaggio al server. Mostra i giocatori che hanno
//							terminato i temi.
// --------------------------------------------------------------------------------
void mostraQuizCompletati(){

	struct SessioneQuiz* s = sessioni;
	for(int i=0; i < MAX_QUIZ; i++){
		
		printf("Quiz tema %d completato\n", i+1);
		while((s != NULL) && (s->tema == (i+1))){
			if(s->domanda > MAX_QUESTIONS){
				printf("- %s\n", (char*)s->nickname);
			}
			s = s->next;
		}
		printf("\n");
	}
}

// --------------------------------------------------------------------------------
// controllaQuizCompletato:	Funzione invocata dal server ogni volta che il
//							client richiede di giocare ad un tema. Ritorna
//							0 se 'nickname' non ha ancora completato il
//							'tema_richiesto', 1 altrimenti.
// --------------------------------------------------------------------------------
int controllaQuizCompletato(const int tema_richiesto, const char* nickname){

	struct SessioneQuiz* s = sessioni;
	int result = 0;
		
	while(s != NULL){
		if((s->tema == tema_richiesto) &&
		   (strcmp((char*)s->nickname, nickname) == 0) &&
		   (s->domanda > MAX_QUESTIONS)){
		   
			result = 1;
			break;
		}
		s = s->next;
	}
	return result;
}

// --------------------------------------------------------------------------------
// tuttiTemiCompletati:	Funzione invocata dal server ogni volta che il
//						client richiede di giocare ad un tema. Ritorna
//						0 se 'nickname' non ha ancora completato tutti
//						i temi disponibili, 1 altrimenti.
// --------------------------------------------------------------------------------
int tuttiTemiCompletati(const char* nickname){
	struct SessioneQuiz* s = sessioni;
	int result = 0;
		
	while(s != NULL){
		if((strcmp((char*)s->nickname, nickname) == 0) && (s->domanda > MAX_QUESTIONS)){
			result++;
		}
		s = s->next;
	}
	
	if(result == MAX_QUIZ){
		result = 1;
	}else{
		result = 0;
	}
	
	return result;
}

// --------------------------------------------------------------------------------
// mostraStatoGioco:	Funzione invocata ogni volta che il client manda un
// 						messaggio al server. Mostra i giocatori connessi, i
//						punteggi dei giocatori nei vari quiz ed i temi 
//						completati dai giocatori.
// --------------------------------------------------------------------------------
void mostraStatoGioco(){
	
	mostraListaTemiQuiz();
	mostraClientConnessi();
	mostraPunteggi();
	mostraQuizCompletati();
	printf("-----\n");
}

// --------------------------------------------------------------------------------
// caricaDomanda:	Funzione invocata dal server per caricare dal file
// 					"./quiz_files/questions.txt" la domanda del tema 'i'
// 					e domanda 'j' nel buffer 'domanda'.
// --------------------------------------------------------------------------------
void caricaDomanda(int i, int j, char* domanda){
	FILE *file;
	char buffer[TEXT_SIZE];
	int tema, num_domanda;
	
	// caricamento lista dei temi
	if((file = fopen("./quiz_files/questions.txt", "r")) == NULL){
		perror("Errore nell'apertura del file della lista delle domande");
		exit(EXIT_FAILURE);
	}
	
	while(!feof(file)){

		fgets(buffer, TEXT_SIZE, file);

		if(feof(file))break;

		// Scomposizione tema, num_domanda del quiz
		tema = atoi(strtok(buffer, ":"));
		num_domanda = atoi(strtok(NULL, ":"));
		
		if((tema == i) && (num_domanda == j)){
			strcpy(domanda, strtok(NULL, ":"));
			break;
		}
	}

	fclose(file);
}

// --------------------------------------------------------------------------------
// rispostaCorretta:	Funzione invocata dal server per controllare dal file
// 						"./quiz_files/answers.txt" la risposta 'risposta' del
// 						tema 'i' e domanda 'j'. Ritorna 1 se la risposta è
//						corretta, 0 altrimenti.
// --------------------------------------------------------------------------------
int rispostaCorretta(const int i, const int j, const char* risposta){
	int result = 0;
	FILE *file;
	char buffer[TEXT_SIZE];
	int tema, domanda;
	
	// caricamento lista dei temi
	if((file = fopen("./quiz_files/answers.txt", "r")) == NULL){
		perror("Errore nell'apertura del file della lista delle risposte");
		exit(EXIT_FAILURE);
	}
	
	while(!feof(file)){

		fgets(buffer, TEXT_SIZE, file);

		if(feof(file))break;
		
		buffer[strcspn(buffer, "\n")] = 0;

		// Scomposizione tema, domanda e risposta del quiz
		tema = atoi(strtok(buffer, ":"));
		domanda = atoi(strtok(NULL, ":"));
		
		if((tema == i) && (domanda == j)){
			
			if(strcmp(risposta, strtok(NULL, ":")) == 0){
				result = 1;
			}
			
			break;
		}
	}

	fclose(file);
	
	return result;
}

// --------------------------------------------------------------------------------
// serializzaListaTemi:	Funzione invocata dal server per serializzare la lista dei
//						temi dei quiz in 'buffer' passata come parametro.
// --------------------------------------------------------------------------------
void serializzaListaTemi(char* buffer){
	int offset = 0;
	for(int i=0; i < MAX_QUIZ; i++){
		memcpy(buffer+offset, lista_temi[i], THEME_SIZE);
		offset += THEME_SIZE;
		
		if(offset > TEXT_SIZE)break; // La lista dei temi non rientra 
				  	     // in un unico messaggio.
	}
}

// --------------------------------------------------------------------------------
// liberaClient:	Funzione invocata ogni volta che il server deve rimuovere
//					un client che si è disconnesso per far spazio ad un altro.
// --------------------------------------------------------------------------------
void liberaClient(const int socket){
	
	for(int i=0; i < MAX_CLIENTS; i++){
		if(giocatori[i].socket == socket){
					
			if(strlen((char*)giocatori[i].nickname) > 0){
				num_giocatori--;
			}
			
			memset(&giocatori[i], 0, sizeof(giocatori[i]));
			break;
		}
	}
	
	close(socket);
}

// --------------------------------------------------------------------------------
// riceviMessaggio:	Funzione invocata ogni volta che il server attende un
// 					messaggio dal client. Ritorna 1 se la ricezione è avvenuta
//					con successo, -1 altrimenti.
// --------------------------------------------------------------------------------
int riceviMessaggio(const int socket, char* type, unsigned char* msg){
	int bytes_read;
	int msg_length;
	
	if((bytes_read = recv(socket, type, TYPE_SIZE-1, 0)) <= 0){
		// CLIENT DISCONNESSO
		return -1;
	}
	
	type[bytes_read] = '\0';

	// RICEZIONE DELLA DIMENSIONE IN BYTE DEL CONTENUTO DEL MESSAGGIO
	if((bytes_read = recv(socket, &msg_length, sizeof(msg_length), 0)) <= 0){
		// CLIENT DISCONNESSO
		return -1;
	}
	
	// RICEZIONE DEL CONTENUTO DEL MESSAGGIO (SE C'E')
	msg_length = ntohl(msg_length);
	if(msg_length > 0){
		if((bytes_read = recv(socket, msg, msg_length, 0)) <= 0){
			// CLIENT DISCONNESSO
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
// inviaMessaggio:	Funzione invocata ogni volta che il server invia un
//					messaggio al client. Ritorn -1 se c'è errore di invio
// 					messaggio al client (client disconesso), 1 altrimenti.
// --------------------------------------------------------------------------------
int inviaMessaggio(const int socket, const char* type, const unsigned char* msg, const int msg_length){
	int net_msg_length = htonl(msg_length);
	
	// Invio del tipo di messaggio
	if(send(socket, type, TYPE_SIZE-1, 0) <= 0){
		// CLIENT DISCONNESSO
		return -1;
	}
	
	// Invio del messaggio (DIMENSIONE IN BYTE + CONTENUTO)
	if(send(socket, &net_msg_length, sizeof(net_msg_length), 0) <= 0){
		// CLIENT DISCONNESSO
		return -1;
	}

	if(msg_length > 0){		
		if(send(socket, msg, msg_length, 0) <= 0){
			// CLIENT DISCONNESSO
			return -1;
		}
	}
	
	return 1;
}

// --------------------------------------------------------------------------------
// chiudiConnessioniClients:	Funzione invocata ogni volta che il server termina
//								la sua esecuzione.
// --------------------------------------------------------------------------------
void chiudiConnessioniClients(int sig){
	
	for(int i=0; i < MAX_CLIENTS; i++){
		if(giocatori[i].socket > 0){
			close(giocatori[i].socket);
		}
	}
	
	close(server_fd);
	printf("\nServer in chiusura...\n");
	exit(0);
}

int main(){
	
	// ------------------------------------------------------
	// VARIABILI E INIZIALIZZAZIONI
	//int server_fd;
	struct sockaddr_in server_addr;
	int addrlen = sizeof(server_addr);
	fd_set readfds;
	int max_fd;
	int activity;
	int new_socket;
	char type_msg[TYPE_SIZE] = {0};
	char rcv_msg[TEXT_SIZE] = {0};
	char snd_msg[TEXT_SIZE] = {0};
	// ------------------------------------------------------

	// ------------------------------------------------------
	// SOCKET TCP DEL SERVER
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Errore nella crezione del socket server");
		exit(EXIT_FAILURE);
	}
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);
	
	if((bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0){
		perror("Errore nella bind dell'indirizzo al socket server");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	
	if((listen(server_fd, 10)) < 0){
		perror("Errore nella listen del socket server");
		close(server_fd);
		exit(EXIT_FAILURE);
	}
	// ------------------------------------------------------

	// ------------------------------------------------------
	// AVVIO DEL GIOCO
	caricaSessioni();
	mostraStatoGioco();
	// ------------------------------------------------------

	// ------------------------------------------------------
	// GESTIONE DISCONNESSIONE DEL SERVER
	signal(SIGINT, chiudiConnessioniClients);
	signal(SIGHUP, chiudiConnessioniClients);
	signal(SIGTSTP, chiudiConnessioniClients);
	// ------------------------------------------------------

	// ------------------------------------------------------
	// CONTROLLO DEI MESSAGGI
	while(1){
		FD_ZERO(&readfds);
		
		FD_SET(server_fd, &readfds);
		max_fd = server_fd;
		
		for(int i=0; i < MAX_CLIENTS; i++){
			if(giocatori[i].socket > 0){
				FD_SET(giocatori[i].socket, &readfds);
			}
			if(giocatori[i].socket > max_fd){
				max_fd = giocatori[i].socket;
			}
		}
		
		activity = select(max_fd+1, &readfds, NULL, NULL, NULL);
		
		if((activity < 0) && (errno != EINTR)){
			perror("Errore nella select");
		}
		
		// ------------------------------------------------------
		// CONTROLLO NUOVA CONNESSIONE CLIENT
		if(FD_ISSET(server_fd, &readfds)){
		
			if((new_socket = accept(server_fd, (struct sockaddr *)&server_addr, (socklen_t*)&addrlen)) < 0){
				perror("Errore nella accept del socket server");
				exit(EXIT_FAILURE);
			}
			
			if(num_giocatori >= MAX_CLIENTS){
				
				// Invio messaggio di errore: numero massimo di client ospitati raggiunto
				strcpy(type_msg, "CLN_ERR");
				strcpy(snd_msg, "Numero massimo di client ospitati raggiunto. Riprovare più tardi.");
				inviaMessaggio(new_socket, type_msg, (unsigned char*)snd_msg, strlen(snd_msg));
				close(new_socket);
			}else{
				// Aggiunta di un nuovo socket client
				for(int i=0; i < MAX_CLIENTS; i++){
					if(giocatori[i].socket == 0){
						giocatori[i].socket = new_socket;
						break;
					}
				}
						
				// Invio di richiesta del nickname al client
				strcpy(type_msg, "NCK_REQ");
				if(inviaMessaggio(new_socket, type_msg, NULL, 0) < 0){
					// CLIENT DISCONNESSO
					liberaClient(new_socket);
				}
			}
		}
		// ------------------------------------------------------
		
		// ------------------------------------------------------
		// CONTROLLO RICEZIONE DATI DAI CLIENT
		for(int i=0; i < MAX_CLIENTS; i++){
			if(FD_ISSET(giocatori[i].socket, &readfds)){

				// Ricezione tipo messaggio
				if(riceviMessaggio(giocatori[i].socket, type_msg, (unsigned char*)rcv_msg) < 0){
					// CLIENT DISCONNESSO
					liberaClient(giocatori[i].socket);
				}
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA NICKNAME
				if(strcmp(type_msg,"NCK_ANS") == 0){
					if(tuttiTemiCompletati(rcv_msg)){
						
						strcpy(type_msg, "END_CLN");
						strcpy(snd_msg, "Hai già terminato tutti i temi.");
						
						// Invio messaggio di errore: nickname già esistente					
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							//liberaClient(giocatori[i].socket);
						}
						
						liberaClient(giocatori[i].socket);

					}else if(!nicknameEsistente((char*)rcv_msg)){
					
						int num_quiz;
						
						num_giocatori++;
						strcpy((char*)giocatori[i].nickname, rcv_msg);

						strcpy(type_msg, "THM_NMB");
						num_quiz = htonl(MAX_QUIZ);
						
						// Invio del numero dei temi di quiz disponibili
						if(inviaMessaggio(giocatori[i].socket, type_msg, 
							(unsigned char*)&num_quiz, sizeof(num_quiz)) < 0){
							
							// CLIENT DISCONNESSO
							liberaClient(giocatori[i].socket);
						}
					}else{
					
						strcpy(type_msg, "NCK_REQ");
						strcpy(snd_msg, "Spiacenti, il nickname è già esistente.");
						
						// Invio messaggio di errore: nickname già esistente					
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							liberaClient(giocatori[i].socket);
						}
					}
				}
				// ------------------------------------------------------

				// ------------------------------------------------------
				// GESTIONE RICHIESTA DELLA LISTA DEI TEMI GIOCABILI
				if(strcmp(type_msg,"THM_LST") == 0){
					
					// strcpy(type_msg, "THM_LST");
					serializzaListaTemi(snd_msg);
					
					// Invio al client della lista dei temi di quiz
					if(inviaMessaggio(giocatori[i].socket, type_msg,
						(unsigned char*)snd_msg, (MAX_QUIZ*THEME_SIZE)) < 0){
						
						// CLIENT DISCONNESSO
						liberaClient(giocatori[i].socket);
					}
					continue;
				}
				// ------------------------------------------------------

				// ------------------------------------------------------
				// GESTIONE RICHIESTA DI UN TEMA DEL QUIZ
				if(strcmp(type_msg,"THM_REQ") == 0){
				
					// Inizializzazione di una sessione al quiz
					int tema_richiesto = 0;
					
					memcpy(&tema_richiesto, rcv_msg, sizeof(tema_richiesto));
					tema_richiesto = ntohl(tema_richiesto);
					
					if(!controllaQuizCompletato(tema_richiesto, (char*)giocatori[i].nickname)){
						struct SessioneQuiz* s;
						
						giocatori[i].tema = (uint32_t)tema_richiesto;
						
						s = ottieniSessione(tema_richiesto, (char*)giocatori[i].nickname);
						
						if(s == NULL){
							s = creaSessione(tema_richiesto, (char*)giocatori[i].nickname);
							aggiungiSessioneInLista(s);
						}
						
						strcpy(type_msg, "QST_REQ");
						caricaDomanda(s->tema, s->domanda, snd_msg);
						
						// Invio al client della prima domanda del quiz scelto
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							liberaClient(giocatori[i].socket);
						}
					}else{
						strcpy(type_msg, "THM_CMP");
						strcpy(snd_msg, "Tema scelto già completato. Chiedi un altro tema disponibile non ancora completato.");
						// Invio al client di inserire un altro tema disponibile
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							liberaClient(giocatori[i].socket);
						}
					}
					
					
				}
				// ------------------------------------------------------
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA ESITO RISPOSTA DOMANDA DEL QUIZ
				if(strcmp(type_msg,"CHK_REQ") == 0){
					
					struct SessioneQuiz* s = ottieniSessione(giocatori[i].tema,
										(char*)giocatori[i].nickname);
					
					// Controllo correttezza risposta data dal client
					if(rispostaCorretta((int)s->tema,
						(int)s->domanda, rcv_msg)){
						
						s->punteggio++;
						aggiornaSessioneInLista(s);
						strcpy(snd_msg,"Risposta corretta");
					}else{
						strcpy(snd_msg,"Risposta errata");
					}
					
					s->domanda++;
					
					strcpy(type_msg,"CHK_ANS");
					
					// Invio al client della esito
					if(inviaMessaggio(giocatori[i].socket, type_msg,
						(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
						
						// CLIENT DISCONNESSO
						liberaClient(giocatori[i].socket);
					}
				}
				// ------------------------------------------------------
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA PROSSIMA DOMANDA DEL QUIZ
				if(strcmp(type_msg,"NXT_REQ") == 0){
				
					struct SessioneQuiz* s = ottieniSessione(giocatori[i].tema,
										(char*)giocatori[i].nickname);
				
					// Controllo del termine di un quiz
					if(s->domanda <= MAX_QUESTIONS){
						
						strcpy(type_msg, "QST_REQ");
						caricaDomanda(s->tema, s->domanda, snd_msg);
					
						// Invio al client della prossima domanda del quiz
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							liberaClient(giocatori[i].socket);
						}
					}else{
						// Quiz finito
						
						aggiornaSessioneInFile(s);
						
						strcpy(type_msg,"END_CLN");
						sprintf(snd_msg, "Tema '%s' completato.", lista_temi[s->tema-1]);
						
						// Invio al client che ha completato il tema del quiz
						if(inviaMessaggio(giocatori[i].socket, type_msg,
							(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
							
							// CLIENT DISCONNESSO
							//liberaClient(giocatori[i].socket);
						}
						
						liberaClient(giocatori[i].socket);
					}
				}
				// ------------------------------------------------------
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA VISTA DEI PUNTEGGI ATTUALI DEI GIOCATORI CONNESSI
				if(strcmp(type_msg,"SHW_SCR") == 0){
					int num_punteggi;

					strcpy(type_msg, "SCR_NMB");
					num_punteggi = htonl(num_sessioni);
						
					// Invio del numero dei punteggi totali disponibili
					if(inviaMessaggio(giocatori[i].socket, type_msg, 
						(unsigned char*)&num_punteggi, sizeof(num_punteggi)) < 0){
						
						// CLIENT DISCONNESSO
						liberaClient(giocatori[i].socket);
					}
				}
				// ------------------------------------------------------
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA LISTA DEI PUNTEGGI
				if(strcmp(type_msg,"SCR_LST") == 0){
					int num_punteggi = num_sessioni;
					int dim_punteggio = THEME_SIZE + NICKNAME_SIZE + sizeof(uint32_t);

					strcpy(type_msg, "SHW_SCR");
					serializzaPunteggi((unsigned char*)snd_msg);
					
					// Invio del numero dei temi di quiz disponibili
					if(inviaMessaggio(giocatori[i].socket, type_msg, 
						(unsigned char*)snd_msg, (num_punteggi*dim_punteggio)) < 0){
						
						// CLIENT DISCONNESSO
						liberaClient(giocatori[i].socket);
					}
				}
				// ------------------------------------------------------
				
				// ------------------------------------------------------
				// GESTIONE RICHIESTA ABBANDONO DEL GIOCO DI UN GIOCATORE
				if(strcmp(type_msg,"END_REQ") == 0){
				
					struct SessioneQuiz* s = ottieniSessione(giocatori[i].tema,
										(char*)giocatori[i].nickname);
					
					aggiornaSessioneInFile(s);
					
					strcpy(type_msg,"END_CLN");
					strcpy(snd_msg, "Partita al quiz abbandonata.");
					if(inviaMessaggio(giocatori[i].socket, type_msg, 
						(unsigned char*)snd_msg, strlen(snd_msg)) < 0){
						// CLIENT DISCONNESSO
						//liberaClient(giocatori[i].socket);
					}
					
					liberaClient(giocatori[i].socket);
				}
				// ------------------------------------------------------
				
				mostraStatoGioco();
			}
		}
	}
	// ------------------------------------------------------
	
	close(server_fd);
	return 0;
}

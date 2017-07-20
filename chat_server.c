#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct clientData{
	int sockd;
	int threadid;
	struct sockaddr_in tcpSocket;
	char username[50];
	char status[20];
	char room[20];
}cliData;

const int Q_LEN = 5;  // number of waiting clients
cliData *clientList;

#define UsrDoc "users.csv"
char UsrList[] = {UsrDoc};

void init_clients();
void insertClient(char *username, char *status, struct sockaddr_in tcpSocket, int sockd, char *room);
void removeClient(char *username);
void kickClient(char *username);
int server_socket(unsigned short port);
int validate(char *BUFFER, char *username, char *status, char *room);
void clientGeneralHandle(void *ptr);
void clientStudentHandle(void *ptr);
void clientFacultyHandle(void *ptr);
void broadcast(char *conversation);
void roomBcast(char *conversation, char *room);

/***********************************************************************************/
int main(int argcount, char *argvector[])
{ 
   int no_delay = 1;
   struct sockaddr_in cli_sin;
   unsigned int addrLen;
   unsigned short portno;
   int sock_listen;
   int sock;
   int flag;
   int ret;
   int n;
   int curr_clients=0;
   
   cliData client[Q_LEN];
   clientList = client;
   pthread_t cliThread[Q_LEN];
   pthread_attr_t attr;
     
   char username[50];
   bzero(username,sizeof(username));
   char status[20];
   bzero(status,sizeof(status));
   char room[20];
   bzero(room,sizeof(room));

   char BUFFER[512];
   
   init_clients();
   portno = 4545;
   printf("Server Started\n");
   sock_listen = server_socket(portno);
   
   while (1)
   {
		sock = accept(sock_listen,(struct sockaddr *) &cli_sin, &addrLen);
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&no_delay, sizeof(no_delay));
  
		bzero(BUFFER,sizeof(BUFFER));
		recv(sock,BUFFER,sizeof(BUFFER),0);
		//Verify that user/pass exists
		if(validate(BUFFER,username,status,room) > 0){
			bzero(BUFFER,sizeof(BUFFER));
			sprintf(BUFFER,"valid~");
			send(sock,BUFFER,sizeof(BUFFER),0);
			//Start General Chat Room if chosen
			if(strncmp(room,"General",3) == 0){
				insertClient(username,status,cli_sin,sock,room);
				bzero(BUFFER,sizeof(BUFFER));
				sprintf(BUFFER,"%s JOINED.",username);
				roomBcast(BUFFER,room);
				printf("GENERAL-%s has joined the chatroom\n",username);
				pthread_create(&cliThread[curr_clients],NULL,(void *)clientGeneralHandle,(void *)&client[curr_clients]);
			}
			//Start Student Chat Room if chose
			else if(strncmp(room,"Student",3) == 0){
				insertClient(username,status,cli_sin,sock,room);
				bzero(BUFFER,sizeof(BUFFER));
				sprintf(BUFFER,"%s JOINED.",username);
				roomBcast(BUFFER,room);
				printf("STUDENT-%s has joined the chatroom\n",username);
				pthread_create(&cliThread[curr_clients],NULL,(void *)clientStudentHandle,(void *)&client[curr_clients]);
			}
			//Start Faculty Chat Room if chosen & client has sufficient privileges
			else if(strncmp(room,"Faculty",3) == 0){
				if((strncmp(status,"admin",3) == 0) || (strncmp(status,"mod",3) == 0)){
					insertClient(username,status,cli_sin,sock,room);
					bzero(BUFFER,sizeof(BUFFER));
					sprintf(BUFFER,"%s JOINED.",username);
					roomBcast(BUFFER,room);
					printf("FACULTY-%s has joined the chatroom\n",username);
					pthread_create(&cliThread[curr_clients],NULL,(void *)clientFacultyHandle,(void *)&client[curr_clients]);
				}
			}
			
			curr_clients++;			
		}
		//Invalid User
		else{
			bzero(BUFFER,sizeof(BUFFER));
			sprintf(BUFFER,"invalid~");
			printf("invalid login attempt:%s\n",username);
			send(sock,BUFFER,sizeof(BUFFER),0);
		}	 
    }   
    /*Close listening socket*/
    close (sock_listen);
   
   return 0;
}
//Initialize all structs
void init_clients()
{
	int counter=0;
	for(counter=0; counter<Q_LEN; counter ++){
		clientList[counter].sockd = -1;
		bzero((char *)&clientList[counter].tcpSocket, sizeof(clientList[counter].tcpSocket));
		bzero(clientList[counter].username,sizeof(clientList[counter].username));
		strcpy(clientList[counter].username,"empty");
		bzero(clientList[counter].status, sizeof(clientList[counter].status));
		strcpy(clientList[counter].status,"none");
	}
}
//Insert a user and their information into the struct once they have been confirmed
void insertClient(char *username, char *status, struct sockaddr_in tcpSocket, int sockd, char *room)
{
	int counter =0;
	for(counter=0; counter < Q_LEN; counter++){
		if(strcmp(clientList[counter].username,"empty") == 0){
			clientList[counter].sockd = sockd;
			clientList[counter].tcpSocket = tcpSocket;
			sprintf(clientList[counter].username,"%s",username);
			sprintf(clientList[counter].status,"%s",status);
			sprintf(clientList[counter].room,"%s",room);
			return;
		}
	}
	
}
//Remove a user from the struct, freeing up their space
void removeClient(char *username)
{
	int counter=0;
	for(counter=0; counter<Q_LEN; counter++){
		if(strcmp(clientList[counter].username, username) == 0){
			close(clientList[counter].sockd);
			clientList[counter].sockd = -1;
			bzero((char *)&clientList[counter].tcpSocket, sizeof(clientList[counter].tcpSocket));
			bzero(clientList[counter].username, sizeof(clientList[counter].username));
			strcpy(clientList[counter].username,"empty");
			bzero(clientList[counter].status, sizeof(clientList[counter].status));
			strcpy(clientList[counter].status,"none");
		}
	}
}
//Send a kick "kill" message to user, first have to find the correct user socket descriptor
void kickClient(char *username)
{
	int counter=0;
	char terminateMsg[50];
	for(counter=0; counter<Q_LEN; counter++){
		if(strcmp(clientList[counter].username, username) == 0){
			sprintf(terminateMsg,"$$kicked");
			send(clientList[counter].sockd,terminateMsg,strlen(terminateMsg),0);
		}
	}
}
//Set up a listening socket
int server_socket(unsigned short port)
{
   struct sockaddr_in serv_sin;
   int sock_listen;
   
   // Setup address structure
   bzero((char *) &serv_sin, sizeof(serv_sin));
   serv_sin.sin_family = AF_INET;
   serv_sin.sin_addr.s_addr = INADDR_ANY; 
   serv_sin.sin_port = htons(port);
   
   // Setup listening socket
   sock_listen = socket(PF_INET, SOCK_STREAM, 0);
   if (sock_listen < 0)
   {
      printf("Failed to create listening (server) socket\n");
      exit(1);
   }
   
   if (bind(sock_listen, (struct sockaddr *) &serv_sin,
                       sizeof(serv_sin)) < 0)
   {
      printf(" Failed to bind listening socket to address\n");
      exit(1);
   }
   
   if (listen(sock_listen, Q_LEN) < 0)
   {
      printf("Failed to listen\n");
      exit(1);
   }
   return sock_listen;
}
//Compare user credentials to .csv list
int validate(char *BUFFER, char *username, char *status, char *room)
{
	char user[50], tmpuser[50];
	char pass[50], tmppass[50];
	char tmpstatus[50];
	char usrinfo[200];
	char *token;

	int validation = 0;
	
	FILE *UL; //UserList

	token=strtok(BUFFER,"~");
	strcpy(user,token);
	
	token=strtok(NULL,"~");
	strcpy(pass,token);	
	
	token=strtok(NULL,"~");
	strcpy(room,token);
	
	
	UL = fopen(UsrList,"r");
	if(UL == NULL)
		perror("Error while opening file.\n");
	
	//Determine if client user/pass exists in the file
	while(fgets(usrinfo, sizeof(usrinfo),UL) != NULL){
		//Extract username from file
		token = strtok(usrinfo,";");
		strcpy(tmpuser, token);
		token=strtok(NULL,";");
		
		//Extract password from file
		strcpy(tmppass, token);
		token=strtok(NULL,";");
		
		//Extract status from file
		strcpy(tmpstatus, token);
		token=strtok(NULL,";");
		
		//Compare client user/pass 
		if((strcmp(user,tmpuser) == 0) && (strcmp(pass,tmppass) == 0)){
			validation++;
			strcpy(username,user);
			strcpy(status,tmpstatus);
		}
	}
	
	return validation;
}
//General chat room loop
void clientGeneralHandle(void *ptr)
{
	cliData *client;
	client = (cliData *)ptr;
	
	char cliBuffer[500];
	char sendBuffer[500];
	bzero(cliBuffer,sizeof(cliBuffer));	
	bzero(sendBuffer,sizeof(sendBuffer));
	
	char *tail;
	
	
	while(1){
		
		recv(client->sockd,cliBuffer,sizeof(cliBuffer),0);
		tail=strtok(cliBuffer,"~");
		strcpy(sendBuffer,tail);

		if(strcmp(sendBuffer,"exitchatprogram") == 0){
			if(strcmp(client->username,"empty") == 0)
				pthread_exit(EXIT_SUCCESS);

			printf("GENERAL-%s has left.\n",client->username);
			//broadcast user has left
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s has left.",client->username);
			roomBcast(cliBuffer,client->room);
			removeClient(client->username);
			pthread_exit(EXIT_SUCCESS);
		}
		//if firstchat = $ then special request from elevated status
		else if(sendBuffer[0] == '$'){
			char *commandtoken;
			char command[10];
			char user[20];
			char status[10];
			strcpy(status,client->status);
		
			commandtoken = strtok(sendBuffer," ");
			strcpy(command,commandtoken);
			commandtoken = strtok(NULL,"");
			strcpy(user,commandtoken);
			
			//warn command - admin and mod only
			if(strcmp(command,"$warn") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("GENERAL-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("GENERAL-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
			   }
			}
			//global command - admin and mod only
			if(strcmp(command,"$global") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("GENERAL-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("GENERAL-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			}
			//kick command - admin only
			if(strcmp(command,"$kick") == 0 && strncmp(client->status,"admin",3)==0){
				kickClient(user);
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("GENERAL-%s KICKED %s\n",client->username,user);
				sprintf(cliBuffer,"%s KICKED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
					
			}
		}
		else{
			//advertise conversation on server
			printf("GENERAL-%s:%s\n",client->username,sendBuffer);
			//call broadcast for clients
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s:%s",client->username,sendBuffer);
			roomBcast(cliBuffer,client->room);
		}
	}
}
//Student chat room loop
void clientStudentHandle(void *ptr)
{
	cliData *client;
	client = (cliData *)ptr;
	
	char cliBuffer[500];
	char sendBuffer[500];
	bzero(cliBuffer,sizeof(cliBuffer));	
	bzero(sendBuffer,sizeof(sendBuffer));
	
	char *tail;
	
	
	while(1){
		
		recv(client->sockd,cliBuffer,sizeof(cliBuffer),0);
		tail=strtok(cliBuffer,"~");
		strcpy(sendBuffer,tail);

		if(strcmp(sendBuffer,"exitchatprogram") == 0){
			if(strcmp(client->username,"empty") == 0)
				pthread_exit(EXIT_SUCCESS);

			printf("STUDENT-%s has left.\n",client->username);
			//broadcast user has left
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s has left.",client->username);
			roomBcast(cliBuffer,client->room);
			removeClient(client->username);
			pthread_exit(EXIT_SUCCESS);
		}
		//if firstchat = $ then special request from elevated status
		else if(sendBuffer[0] == '$'){
			char *commandtoken;
			char command[10];
			char user[20];
			char status[10];
			strcpy(status,client->status);
		
			commandtoken = strtok(sendBuffer," ");
			strcpy(command,commandtoken);
			commandtoken = strtok(NULL,"");
			strcpy(user,commandtoken);
			
			//warn command - admin and mod only
			if(strcmp(command,"$warn") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("STUDENT-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("STUDENT-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
			   }
			}
			//global command - admin and mod only
			if(strcmp(command,"$global") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("STUDENT-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("STUDENT-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			}
			//kick command - admin only
			if(strcmp(command,"$kick") == 0 && strncmp(client->status,"admin",3)==0){
				kickClient(user);
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("STUDENT-%s KICKED %s\n",client->username,user);
				sprintf(cliBuffer,"%s KICKED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
					
			}
		}
		else{
			//advertise conversation on server
			printf("STUDENT-%s:%s\n",client->username,sendBuffer);
			//call broadcast for clients
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s:%s",client->username,sendBuffer);
			roomBcast(cliBuffer,client->room);
		}
	}
}
//Faculty chat room loop
void clientFacultyHandle(void *ptr)
{
	cliData *client;
	client = (cliData *)ptr;
	
	char cliBuffer[500];
	char sendBuffer[500];
	bzero(cliBuffer,sizeof(cliBuffer));	
	bzero(sendBuffer,sizeof(sendBuffer));
	
	char *tail;
	
	
	while(1){
		
		recv(client->sockd,cliBuffer,sizeof(cliBuffer),0);
		tail=strtok(cliBuffer,"~");
		strcpy(sendBuffer,tail);

		if(strcmp(sendBuffer,"exitchatprogram") == 0){
			if(strcmp(client->username,"empty") == 0)
				pthread_exit(EXIT_SUCCESS);

			printf("FACULTY-%s has left.\n",client->username);
			//broadcast user has left
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s has left.",client->username);
			roomBcast(cliBuffer,client->room);
			removeClient(client->username);
			pthread_exit(EXIT_SUCCESS);
		}
		//if firstchat = $ then special request from elevated status
		else if(sendBuffer[0] == '$'){
			char *commandtoken;
			char command[10];
			char user[20];
			char status[10];
			strcpy(status,client->status);
		
			commandtoken = strtok(sendBuffer," ");
			strcpy(command,commandtoken);
			commandtoken = strtok(NULL,"");
			strcpy(user,commandtoken);
			
			//warn command - admin and mod only
			if(strcmp(command,"$warn") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("FACULTY-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("FACULTY-%s WARNED %s\n",client->username,user);
				sprintf(cliBuffer,"%s WARNED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
			   }
			}
			//global command - admin and mod only
			if(strcmp(command,"$global") == 0){
			   if(strncmp(client->status,"mod",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("FACULTY-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			   else if(strncmp(client->status,"admin",3)==0){
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("FACULTY-%s GLOBAL:%s\n",client->username,user);
				sprintf(cliBuffer,"%s GLOBAL %s\n",client->username,user);
				broadcast(cliBuffer);	
			   }
			}
			//kick command - admin only
			if(strcmp(command,"$kick") == 0 && strncmp(client->status,"admin",3)==0){
				kickClient(user);
				bzero(cliBuffer,sizeof(cliBuffer));
				printf("FACULTY-%s KICKED %s\n",client->username,user);
				sprintf(cliBuffer,"%s KICKED %s\n",client->username,user);
				roomBcast(cliBuffer,client->room);
					
			}
		}
		else{
			//advertise conversation on server
			printf("FACULTY-%s:%s\n",client->username,sendBuffer);
			//call broadcast for clients
			bzero(cliBuffer,sizeof(cliBuffer));
			sprintf(cliBuffer,"%s:%s",client->username,sendBuffer);
			roomBcast(cliBuffer,client->room);
		}
	}
}
//global broadcast
void broadcast(char *conversation)
{
		int counter=0;
		while(counter < Q_LEN){
			if(strcmp(clientList[counter].username,"empty") != 0){
				send(clientList[counter].sockd,conversation,strlen(conversation),0);
			}
			counter++;			
		}
}
//broadcasts to an individual room
void roomBcast(char *conversation, char *room)
{
	int counter=0;
		while(counter < Q_LEN){
			if(strcmp(clientList[counter].username,"empty") != 0){
			  if(strcmp(clientList[counter].room,room) == 0){
				send(clientList[counter].sockd,conversation,strlen(conversation),0);
			  }
			}
			counter++;			
		}
}


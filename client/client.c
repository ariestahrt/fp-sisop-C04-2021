#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>
#include <stdbool.h>

#define PORT 8080
#define SIZE 1024

char cwd[PATH_MAX];
char buffer[SIZE];

char GLOBAL_USERNAME[SIZE] = "";
char GLOBAL_PASSWORD[SIZE] = "";
char GLOBAL_DATABASE[SIZE] = "";
char *GLOBAL_DELIMITER = "[,]";

char* convertToCharPtr(char *str){
	int len=strlen(str);
	char* ret = malloc((len+1) * sizeof(char));
	for(int i=0; i<len; i++){
		ret[i] = str[i];
	}
	ret[len] = '\0';
	return ret;
}

char* readServer(int server_fd){
	bzero(buffer, SIZE);
	read(server_fd, buffer, SIZE);
	buffer[strcspn(buffer, "\n")] = 0;
	return convertToCharPtr(buffer);
}

void reply(int server_fd, char *message){
	send(server_fd, message, strlen(message), 0);
}

int main(int argc, char const *argv[]) {
    if(getuid()){
        // This is not root
        printf("Running client as not root\n");
        if(argc == 5){
            if(!strcmp(argv[1], "-u") && !strcmp(argv[3], "-p")){
                sprintf(GLOBAL_USERNAME, "%s", argv[2]);
                sprintf(GLOBAL_PASSWORD, "%s", argv[4]);
            }else{
                printf("Your argument is invalid");
                exit(0);
            }
        }else if(argc == 7){
            if(!strcmp(argv[1], "-u") && !strcmp(argv[3], "-p") && !strcmp(argv[5], "-d")){
                sprintf(GLOBAL_USERNAME, "%s", argv[2]);
                sprintf(GLOBAL_PASSWORD, "%s", argv[4]);
                sprintf(GLOBAL_DATABASE, "%s", argv[6]);
            }else{
                printf("Your argument is invalid");
                exit(0);
            }
        }else{
            printf("Your argument is invalid");
            exit(0);
        }
        // jangan kasih mereka nginput user sebagai root
        if(!strcmp(GLOBAL_USERNAME, "ROOT")){
            printf("You can't set yourself as root!~");
            exit(0);
        }
    }else{
        printf("Running client as root\n");
        // this is root
        sprintf(GLOBAL_USERNAME, "%s", "ROOT");
        sprintf(GLOBAL_PASSWORD, "%s", "");
        sprintf(GLOBAL_DATABASE, "%s", "INFORMATION_SCHEMA");
    }

    getcwd(cwd, sizeof(cwd));
    int server_fd;
    bzero(buffer, SIZE);
    
    // set socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed\n");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    // setup socket option
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Initialize
    struct sockaddr_in server;
    memset(&server, '0', sizeof(server)); 

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    int message_in = inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    if(message_in < 0){
        perror("Client Error: IP not initialized successfully");
        return 0;
    }

    // Connect to given server
    message_in = connect(server_fd, (struct sockaddr*) &server, sizeof(server));
    if(message_in < 0){
        perror("Client Error: Connection failed");
    }

    // KIRIM AUTH
    char AUTH_MESSAGE[SIZE];
    sprintf(AUTH_MESSAGE, "%s%s%s%s", GLOBAL_USERNAME, GLOBAL_DELIMITER, GLOBAL_PASSWORD, GLOBAL_DELIMITER);
    reply(server_fd, AUTH_MESSAGE);
    // BACA RESPONSE
    recv(server_fd, buffer, 256, 0);
    if(buffer[0] != 'Y'){
        // AUTH GAGAL
        printf("[~] Invalid username or password\n");
        exit(0);
    }
    fflush(stdout);

    // cek kalau ada 7 argumen, maka kirim USE DATABASE GLOBAL_DATABASE;
    if(argc == 7){
        char TEMP_MESSAGE[SIZE];
        sprintf(TEMP_MESSAGE, "USE DATABASE %s;", GLOBAL_DATABASE);
        // KIRIM
        reply(server_fd, TEMP_MESSAGE);
        // BACA RESPONSE
        recv(server_fd, buffer, 256, 0);
    }
    
    // untuk menerima pesan dari server
    bzero(buffer,256);
    int n;
    while(1){
        bzero(buffer,256);
        // Read message from server first!
        if((n = recv(server_fd, buffer, 256, 0)) > 0){
            printf("%s",buffer);
            fflush(stdout);

            char message[SIZE];
            fgets(message, 255, stdin);
            reply(server_fd, message);
            
            if(!strcmp(message, "exit\n")){ exit(EXIT_SUCCESS); }                
        }
    }
    return 0;
}
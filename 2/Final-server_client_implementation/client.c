#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>


// Global variables
char **queries;
int numberOfQueries;

// Function prototypes
int communicateWithServer(char *ipAddr, int port);
int readQueryFile(char *filePath, int id);


int 
main(int argc, char **argv){

    //./client –i 1 -a 127.0.0.1 -p 333 -o queryFile.txt
    if(argc < 9){
        fprintf(stderr, "Usage: ./client –i id -a 127.0.0.1 -p PORT -o pathToQueryFile\n");
        return -1;
    }

    char *ipAddr, *pathToQueryFile;
    int port, id;

    int option;
    while((option = getopt(argc, argv, "i:a:p:o:"))!= -1){
        switch(option){
            case 'i':
                id = atoi(optarg);
                fprintf(stderr, "id = %d\n", id);
                break;
            case 'a':
                ipAddr = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'o':
                pathToQueryFile = optarg;
                break;
        }
    }

    if(readQueryFile(pathToQueryFile, id) == -1){
        fprintf(stderr, "Query file could not read.\n");
        return -1;
    }

    if(communicateWithServer(ipAddr, port) == -1){
        fprintf(stderr, "Communication is failed\n");
        return -1;
    }

    for(int i=0; i<numberOfQueries; ++i){
        free(queries[i]);
    }
    free(queries);

    return 0;
}

int 
readQueryFile(char *filePath, int id){

    int fd = open(filePath, O_RDONLY);
    if(fd == -1){
        perror("File open error");
        return -1;
    }

    struct stat fs;
    if(fstat(fd, &fs) == -1){
        perror("fstat error");
        return -1;
    }

    int buffLen = fs.st_size; // check size of file in bytes

    char *buff = calloc(buffLen, sizeof(char));
    char *tempBuff = calloc(buffLen, sizeof(char));

    if(read(fd, buff, buffLen) == -1){
        perror("read error");
        return -1;
    }
    strncpy(tempBuff, buff, buffLen);
    
    char *line;
    numberOfQueries = 0;
    line = strtok(tempBuff, "\n\r");
    while(line != NULL){
        if((line[0]-'0') == id){
            ++numberOfQueries;
        }
        line = strtok(NULL, "\n\r");
    }

    queries = calloc(numberOfQueries, sizeof(char*));
    strncpy(tempBuff, buff, strlen(buff));
    
    line = strtok(tempBuff, "\n\r");
    int i=0;
    while(line != NULL){

        if((line[0]-'0') == id){
            queries[i]  = calloc(strlen(line), sizeof(char));
            line+=2;
            strcpy(queries[i], line);
            ++i;
        }
        line = strtok(NULL, "\n\r");
    }

    for(int i=0; i<numberOfQueries; ++i){
        fprintf(stderr, "[%d] => %s\n", i, queries[i]);
    }

    free(buff);
    free(tempBuff);
    return 0;
}

int 
communicateWithServer(char *ipAddr, int port){

    int fd;
    struct sockaddr_in addr;

    int msgLen=0;
    char *response;
    // int readRes;
    for(int i=0; i<numberOfQueries; ++i){

        fd = socket (PF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ipAddr);
        addr.sin_port = htons(port);
        connect (fd, (struct sockaddr*)&addr, sizeof(addr));

        fprintf(stderr, "[%s] is being sent\n", queries[i]);
        msgLen = strlen(queries[i]);
        if( write(fd, &msgLen, sizeof(int)) == -1 || 
            write(fd, queries[i], msgLen) == -1 
        ){
            perror("write error");
            close(fd);
            return -1;
        }
        
        if(read(fd, &msgLen, sizeof(int)) == -1){
            perror("read error");
            break;
        }
        fprintf(stderr, "msgLen is %d\n", msgLen);
        response = calloc(msgLen, sizeof(char));
        if(read(fd, response, msgLen) == -1){
            perror("read error");
            close(fd);
            return -1;
        }
        fprintf(stderr, "response:%s\n", response);
        free(response);
        close (fd);
    }

    return 0;
}
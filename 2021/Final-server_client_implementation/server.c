#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

// STRUCTS *******************************************************************
struct tInfo {              /* Used as argument to thread_start()   */
    pthread_t thread_id;    /* ID returned by pthread_create()      */
    int       thread_num;   /* Application-defined thread #         */
};

struct Select {
    char isDistinct,
         isAll;
    int numOfCol;
    char **colNames;
};

struct Update {
    int numOfCol;
    char **colNames;
    char **colValues;
    char *condColumn, *condValue;
};

union ParsedQuery {
    struct Select select;
    struct Update update;
};

enum QueryType {Select, Update, Unknown}; 

struct Request{
    enum QueryType type;
    union ParsedQuery parsedQuery;
};

struct Response{
    int len;
    char *table;
};

// MACROS ********************************************************************
#define MAX_FD 10000
#define LOCALHOST_ADDR "127.0.0.1"
#define LISTEN_BACKLOG 100000 // max pending connections
#define BUFF_SIZE 1024

// GLOBAL VARIABLES **********************************************************
char uniqueString[] = "HüsnüAKÇAK_161044112_*;23841sss!4}'-_))<j093!@#$_)$#*&ADFVS^]SystemProgramming-344\n";
char msgServerRunning[] = "The server is running....\n";
char msgServerTerminated[]  = "The server is terminated.\n";
int logFD;
struct tInfo *threadPool;
char ***dataset;
int dataRows, dataColumns;
int numOfThreads;
int workingThreads=0;
sig_atomic_t sigInt =0;
int connFd;
int AR=0, WR=0, AW=0, WW=0;
pthread_cond_t okToRead =PTHREAD_COND_INITIALIZER,
               okToWrite=PTHREAD_COND_INITIALIZER,
               threadAvailable =PTHREAD_COND_INITIALIZER, 
               requestAvailable=PTHREAD_COND_INITIALIZER;
pthread_mutex_t mWR = PTHREAD_MUTEX_INITIALIZER,
                mLogFile = PTHREAD_MUTEX_INITIALIZER,
                mConnDelegate = PTHREAD_MUTEX_INITIALIZER,
                mRequest = PTHREAD_MUTEX_INITIALIZER;

// FUNCTION PROTOTYPES *******************************************************
int becomeDeamon(); // becomeDeamon code is taken from course slides(week5, page 40-41)
int adjustHandler(int sigNo, void handler(int));
static void* handleRequest(void *args);
int openAndRedirectToLogFile();
int loadDataset(char *dataSetPath);
int prepareThreadPool();
int waitForRequests(int port);
int parseQuery(char *query, int len, struct Request **res);
int countCommas(char *str);
int performSelect(struct Request *req, struct Response *res);
int performUpdate(struct Request *req, char **res);

// SIGNAL HANDLERS ***********************************************************
void handlerInt(int sigNo){

    sigInt = 1;

    fprintf(stderr, "Termination signal received, waiting for ongoing threads to complete.\n");
    pthread_cond_broadcast(&requestAvailable);
    pthread_cond_broadcast(&threadAvailable);
    for(int i=0; i<numOfThreads; ++i){
        pthread_join(threadPool[i].thread_id , NULL);
        fprintf(stderr, "%d th thread is being waited\n", i);
    }

    fprintf(stderr, "SIGINT is received\n");
    lseek(STDERR_FILENO, 0, 0);
    fprintf(stderr, "%s", msgServerTerminated);
    
    for(int i=0; i<dataRows; ++i){
        for(int j=0; j<dataColumns; ++j){
            free(dataset[i][j]);
        }
        free(dataset[i]);
    }
    free(dataset);

    pthread_cond_destroy(&okToRead);
    pthread_cond_destroy(&okToWrite);
    pthread_cond_destroy(&threadAvailable);
    pthread_cond_destroy(&requestAvailable);
    free(threadPool);
    close(STDERR_FILENO);
    exit(EXIT_SUCCESS);
}

int 
main(int argc, char **argv){

    if(argc < 9){
        fprintf(stderr, "Usage: ./server -p PORT -o pathToLogFile –l poolSize –d datasetPath\n");
        return -1;
    }

    int port;
    char *logFileName, *datasetPath;
    int option;
    while((option = getopt(argc, argv, "p:o:l:d:"))!= -1){
        switch(option){
            case 'p':
                port = atoi(optarg);
                break;
            case 'o':
                logFileName = optarg;
                break;
            case 'l':
                numOfThreads = atoi(optarg);
                break;
            case 'd':
                datasetPath = optarg;
                break;
        }
    }

    if(openAndRedirectToLogFile(logFileName) == -1){
        return -1;
    }
    
    adjustHandler(SIGINT, handlerInt);

    if(loadDataset(datasetPath) == -1){
        lseek(STDERR_FILENO, 0, 0);
            fprintf(stderr, "%s", msgServerTerminated);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        close(logFD);
        return -1;
    }

    fprintf(stderr, "p: %d, o: %s, l: %d, d: %s\n", port, logFileName, numOfThreads, datasetPath);
    if(becomeDeamon() == -1){
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        close(logFD);
        fprintf(stderr, "Server program is terminating, becomeDeamon is failed.\n");
        return -1;
    }

    if(prepareThreadPool() == -1){
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        close(logFD);
        return -1;
    }

    waitForRequests(port);

    // sleep(1111);  
    lseek(logFD, 0, 0);
    fprintf(stderr, "Normal termination");  
    return 0;
}

int
waitForRequests(int port){

    int srvFd =0;
    struct sockaddr_in srvAddr, cliAddr;
    socklen_t cliAddrSize;
    // struct hostent* hostinfo;

    srvFd = socket(PF_INET, SOCK_STREAM, 0);
    if(srvFd == -1){
        perror("socket error");
        return -1;
    }

    memset(&srvAddr, 0, sizeof(struct sockaddr_in));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);
    srvAddr.sin_port = htons(port);

    if (bind(srvFd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) == -1){
        perror("bind error");
        close(srvFd);
        return -1;
    }

    if (listen(srvFd, LISTEN_BACKLOG) == -1){
        perror("listen error");
        close(srvFd);
        return -1;
    }
   
    while(sigInt == 0){

        pthread_mutex_lock(&mConnDelegate);
        connFd = accept(srvFd, (struct sockaddr*)&cliAddr, &cliAddrSize);
        if(sigInt>0){
            
            break;
        }

        if(connFd == -1){
            perror("Accept error");
            close(srvFd);
            return -1;
        }

        if(workingThreads == numOfThreads){
            fprintf(stderr, "No thread is available! Waiting…\n");
            pthread_cond_wait(&threadAvailable, &mConnDelegate);
            if(sigInt>0){
                pthread_cond_broadcast(&requestAvailable);
                break;
            }
        }
        
        pthread_cond_signal(&requestAvailable);
    }

    close(srvFd);
    return 0;
}

static void *
handleRequest(void *args){

    struct tInfo *tinfo = (struct tInfo*)args;
    int localConnFd,
        msgLen; 
    char *request = NULL;

    while(sigInt == 0){
        fprintf(stderr, "Thread #%d: waiting for connection\n", tinfo->thread_num);

        pthread_cond_wait(&requestAvailable, &mRequest);
        if(sigInt>0){
            close(connFd);
            return tinfo;
        } 
        pthread_mutex_lock(&mWR);
        ++workingThreads;
        pthread_mutex_unlock(&mWR);

        localConnFd = connFd;
        pthread_mutex_unlock(&mConnDelegate);

        if(read(localConnFd, &msgLen, sizeof(int)) == -1){
            perror("read error");
        }
        request = calloc(msgLen, sizeof(char));
        fprintf(stderr, "msgLen is %d\n", msgLen);
        if(read(localConnFd, request, msgLen) == -1){
            perror("read error");
        }
        request = strtok(request, "\n");
        fprintf(stderr, "Request: %s\n", request);
        
        // parse the request 
        struct Request *req;
        parseQuery(request, msgLen, &req);
        // prepare the response
        switch (req->type)
        {
            case Select:
                pthread_mutex_lock(&mWR);
                while ((AW + WW) > 0) { 
                    WR++; 
                    pthread_cond_wait(&okToRead,&mWR);
                    WR--;
                }
                AR++; 
                pthread_mutex_unlock(&mWR);
                // Access Table
                struct Response res;
                performSelect(req, &res);
                if( write(localConnFd, &(res.len), sizeof(int)) == -1 ||
                    write(localConnFd, res.table, res.len) == -1
                ){
                    perror("write error");
                }

                pthread_mutex_lock(&mWR);
                AR--;
                if (AR == 0 && WW > 0){
                    pthread_cond_signal(&okToWrite);
                }
                pthread_mutex_unlock(&mWR);
                break;
            case Update:
                pthread_mutex_lock(&mWR);
                while ((AW + AR) > 0) {
                    WW++; 
                    pthread_cond_wait(&okToWrite, &mWR);
                    WW--;
                }
                AW++; 
                pthread_mutex_unlock(&mWR);
                // Access Table
                pthread_mutex_lock(&mWR);
                AW--;
                if (WW > 0)
                    pthread_cond_wait(&okToWrite, &mWR);
                else if (WR > 0)
                    pthread_cond_broadcast(&okToRead);
                pthread_mutex_unlock(&mWR);
                break;
        
            case Unknown:
                close(localConnFd);
                return tinfo;
                break;
        }

        pthread_mutex_lock(&mWR);
        --workingThreads;
        pthread_mutex_unlock(&mWR);
        pthread_cond_signal(&threadAvailable);

        close(localConnFd);
    }

    // fprintf(stderr, "THREAD #%d is returned\n", tinfo->thread_num);
    return tinfo;
}

int 
performSelect(struct Request *req, struct Response *res){

    struct Select sel = req->parsedQuery.select;
    res->len = dataRows*dataColumns*2;
   
    if(sel.isAll){
     
        for(int i=0; i<dataRows; ++i){
            for(int j=0; j<dataColumns; ++j){
                res->len += strlen(dataset[i][j]);
            }
        }
        res->table = calloc((res->len), sizeof(char));
     
        int cursor=0;
        for(int i=0; i<dataRows; ++i){
            for(int j=0; j<dataColumns; ++j){
                for(int k=0; k<strlen(dataset[i][j]); ++k){
                    res->table[cursor++] = dataset[i][j][k];
                }
                res->table[cursor++] = ',';
            }
            res->table[cursor++] = '\n';
        }
    }
    else{
        char *colSings = calloc(dataColumns, sizeof(char));
        for(int i=0; i<sel.numOfCol; ++i){
            for(int j=0; j<dataColumns; ++j){
                if(strcmp(sel.colNames[i], dataset[0][j]) == 0){
                    colSings[j] = 1;
                }
            }
        }

        for(int i=0; i<dataRows; ++i){
            for(int j=0; j<dataColumns; ++j){
                if(colSings[j] == 1)
                    res->len += strlen(dataset[i][j]);
            }
        }
        res->table = calloc((res->len), sizeof(char));
     
        int cursor=0;
        for(int i=0; i<dataRows; ++i){
            for(int j=0; j<dataColumns; ++j){
                if(colSings[j] == 1){
                    for(int k=0; k<strlen(dataset[i][j]); ++k){
                        res->table[cursor++] = dataset[i][j][k];
                    }
                    res->table[cursor++] = ',';
                }
            }
            res->table[cursor++] = '\n';
        }

    }
    
    return 0;
}

int 
performUpdate(struct Request *req, char **res){
    
    return 0;
}

int
parseQuery(char *query, int len, struct Request **res){

    char *tok, *tempQuery = calloc(len, sizeof(char));
    *res = malloc(sizeof(struct Request));
    char *savePtr1, *savePtr2;

    for(int i=0; i<len; ++i){
        query[i] = tolower(query[i]);
    }

    strncpy(tempQuery, query, len);

    tok =strtok_r(query, " ,", &savePtr1);
    if(tok == NULL){
        (*res)->type = Unknown;
        return -1;
    }
    else if(strcmp(tok, "select") == 0){
        fprintf(stderr, "Received query %s\n", tempQuery);
        (*res)->type = Select;

        tok = strtok_r(NULL, " ,", &savePtr1);
        while (tok != NULL)
        {
            if(strcmp(tok, "distinct") == 0){
                (*res)->parsedQuery.select.isDistinct = 1;
            }
            else if(strcmp(tok, "*") == 0){
                (*res)->parsedQuery.select.isAll = 1;
            }
            else if(strcmp(tok, "from") == 0){
                break; // no need to reveal rest of the query, because there is only one table
            }
            else {
                if((*res)->parsedQuery.select.isAll == 1){
                    fprintf(stderr, "THERE IS SOMETHING WRONG HAPPEND, read tok: %s\n", tok);
                    break;
                }
                // count commas in the query to detect number of columns desired to get
                (*res)->parsedQuery.select.numOfCol = countCommas(tempQuery) + 1; 
                (*res)->parsedQuery.select.colNames = calloc((*res)->parsedQuery.select.numOfCol, sizeof(char*));

                int i=0;
                while(tok != NULL && strcmp(tok, "from") != 0){
                    (*res)->parsedQuery.select.colNames[i] = calloc(strlen(tok), sizeof(char));
                    strcpy((*res)->parsedQuery.select.colNames[i], tok);
                    ++i;
                    tok = strtok_r(NULL, " ,", &savePtr1);
                }

                // fprintf(stderr, "(*res)->parsedQuery.select.isAll: %d\n", (*res)->parsedQuery.select.isAll);
                // fprintf(stderr, "(*res)->parsedQuery.select.isDistinct: %d\n", (*res)->parsedQuery.select.isDistinct);
                // fprintf(stderr, "(*res)->parsedQuery.select.numOfCol: %d\n", (*res)->parsedQuery.select.numOfCol);
                // for(int i=0; i<(*res)->parsedQuery.select.numOfCol; ++i){
                //     fprintf(stderr, "column[%d]: %s\n", i, (*res)->parsedQuery.select.colNames[i]);
                // }
                break;
            }
            tok = strtok_r(NULL, " ,", &savePtr1);
        }
    }
    else if(strcmp(tok, "update") == 0){
        char *l, *r;
        fprintf(stderr, "Received query %s\n", tempQuery);
        (*res)->type = Update;
        (*res)->parsedQuery.update.numOfCol = countCommas(tempQuery)+1;
        (*res)->parsedQuery.update.colNames = calloc((*res)->parsedQuery.update.numOfCol, sizeof(char*));
        (*res)->parsedQuery.update.colValues = calloc((*res)->parsedQuery.update.numOfCol, sizeof(char*));
        
        while(tok != NULL && strcmp(tok, "set") != 0){
            tok = strtok_r(NULL, " ", &savePtr1);
        } 
        tok = strtok_r(NULL, ", ", &savePtr1);
        int i=0;
        while(tok != NULL && strcmp(tok, "where") != 0){

            // fprintf(stderr, "TOK: %s\n", tok);
            l = strtok_r(tok, "=", &savePtr2);
            r = strtok_r(NULL, "=", &savePtr2);
            (*res)->parsedQuery.update.colNames[i] = calloc(strlen(l), sizeof(char));
            strncpy((*res)->parsedQuery.update.colNames[i], l, strlen(l));
            (*res)->parsedQuery.update.colValues[i] = calloc(strlen(r), sizeof(char));
            strncpy((*res)->parsedQuery.update.colValues[i], r, strlen(r));
            tok = strtok_r(NULL, ", ", &savePtr1);
            ++i;
        }

        tok = strtok_r(NULL, " =", &savePtr1);
        (*res)->parsedQuery.update.condColumn = calloc(strlen(tok), sizeof(char));
        strncpy((*res)->parsedQuery.update.condColumn, tok, strlen(tok));

        tok = strtok_r(NULL, " =", &savePtr1);
        (*res)->parsedQuery.update.condValue = calloc(strlen(tok), sizeof(char));
        strncpy((*res)->parsedQuery.update.condValue, tok, strlen(tok));

        // fprintf(stderr, "(*res)->parsedQuery.update.numOfColumn: %d\n", (*res)->parsedQuery.update.numOfCol);
        // for(int i=0; i<(*res)->parsedQuery.update.numOfCol; ++i){
        //     fprintf(stderr, "%s => %s\n", (*res)->parsedQuery.update.colNames[i], (*res)->parsedQuery.update.colValues[i]);
        // }
        // fprintf(stderr, "(*res)->parsedQuery.update.condColumn: %s\n", (*res)->parsedQuery.update.condColumn);
        // fprintf(stderr, "(*res)->parsedQuery.update.condValue: %s\n", (*res)->parsedQuery.update.condValue);
    }
    else{
        fprintf(stderr, "Unknown query\n");
        (*res)->type = Unknown;
        return -1;
    }

    return 0;
}

int 
countCommas(char *str){

    int count=0;
    for(int i=0; i<strlen(str); ++i){
        if(str[i] == ',')
            ++count;
    }
    return count;
}

int 
loadDataset(char *datasetPath){
    
    fprintf(stderr, "Loading dataset...\n");
    int fd = open(datasetPath, O_RDONLY);
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
        close(fd);
        return -1;
    }
    strncpy(tempBuff, buff, buffLen);

    // count lines
    char *line=NULL;
    dataRows = 0;
    for(line = strtok(tempBuff, "\n\r"); 
        tempBuff!=NULL && line != NULL;
        line = strtok(NULL, "\n\r")
    ){
        ++dataRows;
    }

    dataset = calloc(dataRows, sizeof(char*));

    strncpy(tempBuff, buff, buffLen);
    // count columns
    
    line = strtok(tempBuff, "\n\r");
    char *col;
    dataColumns = 0; 
    for(col = strtok(line, ",\n");
        col != NULL;
        col = strtok(NULL, ",\n")
    ){
        ++dataColumns;
    }

    for(int i=0; i<dataRows; ++i){
        dataset[i] = calloc(dataColumns, sizeof(char*));
    }

    strncpy(tempBuff, buff, buffLen);
    // fill the dataset
    char *savePtr1 = NULL, *savePtr2 = NULL;
    line = NULL;
    int r, c;
    for(r=0, line = strtok_r(tempBuff, "\n\r", &savePtr1); 
        r < dataRows;
        ++r, line = strtok_r(NULL, "\n\r", &savePtr1) 
    ){
        for(c=0, col = strtok_r(line, ",\n", &savePtr2);
            c < dataColumns;
            ++c, col = strtok_r(NULL, ",\n", &savePtr2)
        ){
            dataset[r][c] = calloc(strlen(col), sizeof(char));
            strncpy(dataset[r][c], col, strlen(col));
        }
    }

    for(int i=0; i<dataColumns; ++i){
        for(int j=0; j<strlen(dataset[0][i]); ++j){
            dataset[0][i][j] = tolower(dataset[0][i][j]);
        }
    }
    // for(int i=0; i<dataRows; ++i){
    //     for(int j=0; j<dataColumns; ++j){
    //         fprintf(stderr, "%s, ", dataset[i][j]);
    //     }
    //     fprintf(stderr, "\n");
    // }

    fprintf(stderr, "Dataset is loaded...\n");
    free(buff);
    free(tempBuff);
    close(fd);
    return 0;
}

int 
prepareThreadPool(){

    threadPool = calloc(numOfThreads, sizeof(struct tInfo));
    for(int i=0; i<numOfThreads; ++i){
        threadPool[i].thread_num = i;
        pthread_create(&threadPool[i].thread_id , NULL, handleRequest, &threadPool[i]);
    }

    return 0;
}

int 
adjustHandler(int sigNo, void handler(int)){
    
    struct sigaction sact;
    sact.sa_flags = 0;
    sact.sa_handler = handler;

    if(sigemptyset(&sact.sa_mask) == -1 || sigaction(sigNo, &sact, NULL) == -1){
        perror("sigaction() error");
        return -1;
    }

    return 0;
}

int 
openAndRedirectToLogFile(char *filename){

    pthread_mutex_lock(&mLogFile);
    int fd = open(filename, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if(fd == -1){
        perror("open error");
        pthread_mutex_unlock(&mLogFile);
        return -1;
    }

    size_t lenTestStr = strlen(uniqueString)+strlen(msgServerRunning);
    char testStr[lenTestStr+1];
    char buff[lenTestStr];
    sprintf(testStr, "%s%s", msgServerRunning, uniqueString);
    read(fd, buff, lenTestStr);
    if(strncmp(buff, testStr, lenTestStr)== 0){
        fprintf(stderr, "The server is already running. %d is terminating...\n", getpid());
        pthread_mutex_unlock(&mLogFile);
        return -1;
    }
    close(fd);

    logFD = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(logFD == -1){
        perror("open error");
        pthread_mutex_unlock(&mLogFile);
        return -1;
    }
    
    // redirection 
    dup2(logFD, STDERR_FILENO);
    dup2(logFD, STDOUT_FILENO);

    fprintf(stderr, "%s", msgServerRunning);
    fprintf(stderr, "%s", uniqueString);
    fprintf(stderr, "Log file has been opened by pid: %d\n", getpid());

    pthread_mutex_unlock(&mLogFile);
    return 0;
}

// * WARNING *****************************************************************
// becomeDeamon Function code is taken from course slides(week5, page 40-41)
// ***************************************************************************
int 
becomeDeamon(){
    int maxfd, fd;
    switch (fork()) { /* Become background process */
        case -1: return -1;
        case 0: break; /* Child falls through; adopted by init */
        default: _exit(EXIT_SUCCESS); /* parent terminates and shell prompt is back*/
    }

    if (setsid() == -1) /* Become leader of new session, dissociate from tty */
        return -1; /* can still acquire a controlling terminal */

    switch (fork()) { /* Ensure we are not session leader */
        case -1: return -1; /* thanks to 2nd fork, there is no way of acquiring a tty */
        case 0: break;
        default: _exit(EXIT_SUCCESS);
    }
    
    umask(0); 
    chdir("/");

    maxfd = sysconf(_SC_OPEN_MAX);
    if (maxfd == -1)
        maxfd = MAX_FD; 

    for (fd = 0; fd < maxfd; fd++){
        if(fd != STDERR_FILENO)
            close(fd);
    }

    close(STDIN_FILENO); /* Reopen standard fd's to /dev/null */
    fd = open("/dev/null", O_RDWR);
    
    if (fd != STDIN_FILENO) /* 'fd' should be 0 */
        return -1;
    
    if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
        return -1;
    
    // if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
    //     return -1;
    
    return 0;
}
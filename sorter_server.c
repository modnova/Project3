/*THIS IS THE CODE FOR THE SERVER*/
//Imports
#include "mergesort.c"
#include "sorter_server.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
// Stores the first line of the CSV. (Represents the column headers)
char *first_line;
// Create the locks for the threads
// pthread_mutex_t MUTEX = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **(argv)){
    // Check for correct number or args
    if(argc != 3) {
    printf("Error: Invalid Number of Arguments\n");
    return 0;
    }

    // Check for correct flag
    if(strcmp(argv[1], "-p") != 0){
        printf("Error: Incorrect flag/parameter\n");
        return 0;
    }

    // Check for correct port number
    int portNum;
    if((portNum = atoi(argv[2])) == 0){
        printf("Error: Value for argument 3(Port Number) is incorrect\n");
        return 0;
    }

    // Set socket params
    int setupSocket, clientSocket;
    char message[65535]; // Message set to max number of TCP bytes
    struct addrinfo hints, *results;
    socklen_t addr_size;

    // Create the socket w/ TCP connection
    setupSocket = socket(PF_INET, SOCK_STREAM, 0);

    // Configure Server Settingss
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Get address information
    int s;
    if((s = getaddrinfo(NULL, argv[2], &hints, &results)) != 0){
        printf("getaddrinfo: %s\n", gai_strerror(s));
        return 0;
    }

    // Bind the address struct to the socket
    bind(setupSocket, results->ai_addr, results->ai_addrlen);

    // Listen to the socket and set the maximum number of connections(which we set to 255 because that is the max number of threads)
    if(listen(setupSocket, 255) == 0){
        printf("Server has been initialized. Listening...\n");
    } else {
        printf("Error: Something went wrong with initializing the listener.\n");
        return 0;
    }

    // Create resulting address
    struct sockaddr_in *result_addr = (struct sockaddr_in *)results->ai_addr;
    printf("Listening on FD %d and port %d\n", setupSocket, ntohs(result_addr->sin_port));

    // Create client TID array
    // pthread_t *client_tids = (pthread_t *)malloc(sizeof(pthread_t));
    int max_tid = 0;
    
    // Create new client sockets and spawn new threads
    printf("Waiting for connections...\n");
    while(1)
    while((clientSocket = accept(setupSocket, NULL, NULL)) != -1){
        // Print the IP address of the client
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(struct sockaddr_in);
        int res = getpeername(clientSocket, (struct sockaddr *)&addr, &addr_size);
        printf("Connection received from %s\n", inet_ntoa(addr.sin_addr));
        
        // Put clientSocket into struct to pass to thread
        client_args *clientArgs = (client_args *)malloc(sizeof(client_args));
        clientArgs->client_sock = clientSocket;

        // Spawn a new thread
        pthread_t client_threadid;
        pthread_create(&client_threadid, NULL, handle_connection, (void *)clientArgs);
        pthread_join(client_threadid, NULL);
        // Add TID to TID array
        // client_tids[max_tid] = client_threadid;
        //client_tids = (pthread_t *)realloc(client_tids, sizeof(client_tids) + sizeof(pthread_t));
        max_tid++; 
    }
    
    // Wait for all child threads to complete
    int i;
    for(i=0;i<max_tid;i++){
        //pthread_join((pthread_t)client_tids[i], NULL);
    }
    
    return 0;
}

void *handle_connection(void *arg){
    // Create an integer to hold the client IP value
    client_args *c_args = arg;
    int client_sock = c_args -> client_sock;
    // Create global variables to store CSV data
    data_row **big_db;
    big_db = (data_row**)malloc(sizeof(data_row));
    //printf("BIG DB HAS BEEN INITIALIZED TO SIZE: %d\n", sizeof(big_db));
    int big_lc = 0;
    // Get data from the client
    char buffer[BUFSIZ];
    char req[10];
    int len;
    // TODO Optimization: Let's spawn a thread to handle this while loop.
    while((len = read(client_sock, req, 10)) > 0){
      // printf("RECIEVED REQUEST with len: %d\n", len);
        req[len] = '\0';

        // Get the request string
        // printf("REQUEST FULL: %s\n", req);
        char request[5];
        strncpy(request, req, 4);
        request[4] = '\0';
	     // printf("REQUEST: %s\n", request);
        // Check to see the request type
        if(strcmp(request, "DUMP") == 0){
            // printf("Sorting then dumping...\n");
            // Get the remaining 2 args
            char column[3];
            // printf("buffer: %s\n",req);
            char *request_nums;
            request_nums = strtok(req, "-");
            request_nums = strtok(NULL, "-");
            // printf("Client request with 2: %s\n",request_nums);
            strcpy(column, request_nums);
            int column_to_sort = atoi(column);
            request_nums = strtok(NULL, "-");
            char data[3];
            // printf("Client request with 1: %s\n", request_nums);
            strcpy(data, request_nums);
            int data_flag = atoi(data);
            char buf[PATH_MAX];
            // Sort the big DB
	        // printf("Column to sort: %d\n", column_to_sort);
	        // printf("Data flag: %d\n", data_flag);
	        //printf("\n-sorting-\n");
            //printf("line count: [%d]\n", big_lc);
            sort(&big_db, column_to_sort,data_flag, 0,big_lc-1);
	        // printf("Done sorting\n");
            char *file_name = "file_buffer.csv";
            
            int sent_bytes = 0;
            off_t offset;
            int remain_data;
            ssize_t len;
            // Creates a file from big_db
            //printf("Printing %d lines to CSV \n", big_lc);
            print_to_csv(big_db, big_lc, file_name, first_line);
            int sorted_fd = open("file_buffer.csv", O_RDONLY);
            
            struct stat file_stat;
	        fstat(sorted_fd, &file_stat);
	        offset = 0;
            remain_data = file_stat.st_size;
            
            // Get file size
            char file_size[256];
            sprintf(file_size, "%d", file_stat.st_size);
             // Sending file size 
            len = send(client_sock, file_size, 256,0);
            
            /* Sending file data */
            while ((remain_data > 0) && ((sent_bytes = sendfile(client_sock, sorted_fd, &offset, BUFSIZ)) > 0))
            {
                //printf("Sent %d bytes", sent_bytes);
                remain_data -= sent_bytes;
            }

            close(sorted_fd);
            remove("file_buffer.csv");

            printf("File has been dumped\n");

            // Disconnects for specific client by exiting thread
            //close(client_sock);
            pthread_exit(0);
        }
	else if(strcmp(request, "SORT") == 0) { // This is Sort and a file descriptor is provided

            // Receive the file size
        memset(req,0,sizeof(req));
	    int fsize_len = 0;
	    fsize_len = read(client_sock, buffer, 256);
        int file_size = atoi(buffer);
	    int remaining_data = 0;	    
	    int len = 0;
	    remaining_data = file_size;

	    // Output the file size
            
	    // Clear buffer and print some shit
	    memset(buffer,0,BUFSIZ);
	    //printf("GETTING FILE FROM CLIENT\n");
	    //printf("Remaining Data: %d\n", remaining_data);
	    
	    // Create file descriptor to recieve file
	    int fd = open("file_buffer.csv", O_RDWR | O_APPEND | O_CREAT, 0644);
	    
	    // Read the data from the socket
            while ((remaining_data > 0) && ((len = read(client_sock, buffer, BUFSIZ)) > 0))
            { 
              write(fd, buffer, len);
	          memset(buffer,0,BUFSIZ);
              remaining_data -= len;
            }
            
            // Write newline character to file
            // write(fd, "\n", 1);

	    // Close the CSV
	    //printf("Closing CSV\n");
	    close(fd);

	    // Clear the memory for the next buffer and call process CSV 
	    memset(buffer,0,sizeof(buffer));
	    big_lc = process_csv(&big_db, big_lc);
	    // Remove file that was created, after data is stored in memory
	    remove("file_buffer.csv");
	    printf("File has been sorted and saved\n");

	    // Send client response 
	    char *response = "Recieved file";             
            len = write(client_sock, response, strlen(response) + 1); 
        
        }
	else {
	  printf("Sort or Send request not received\n");
	}
    }
}


/* This function takes in a csv_file, and appends the rows of data to a data structure */
/* in the heap for later sorting */
int process_csv(data_row ***big_db, int big_lc){
  /* Processes the CSV file */
  //printf("Processing CSV\n");

  // Open the CSV for reading
  FILE *csv_file = fopen("file_buffer.csv", "r"); 

  // Define path variables
  char curr_dir[_POSIX_PATH_MAX] = {0};
  char *path = NULL;
  if(csv_file==NULL){
     printf("NULL FILE exiting\n");
     exit(1);
  }
  char delims[] = ",";
  big_db[0][big_lc] = (data_row*)malloc(sizeof(data_row)); // 1 data row
  char line[600]; // one line from the file
  memset(line,0,600);
  int line_counter = -1; // count what line we're on to keep track of the struct array
  int word_counter = 0; // keep track of what word were on for assignment in the struct
  int type_flag = 0; // 0:STRING, 1:INT, 2:FLOAT

  //printf("Getting line now\n");  
  while(fgets(line, 600, csv_file) != NULL){
    int i;
    if(line_counter < 0){
      line_counter++;
      if(first_line == NULL){
        first_line = (char *) malloc(sizeof(char)*strlen(line)+1);
        strcpy(first_line,line);
      }
      if(!(is_csv_correct(line))){
	    printf("[%s]\n", line);
        printf("Incorrect CSV\n");
        fflush(stdout);
        return;
      }
      //pthread_mutex_unlock(&MUTEX);
      continue;
    }
    
    //IF first char is ',' in the line
    if(line[0] == ','){
      char null_val[15];
	    char templine[600];
      sprintf(null_val, "NULL_VALUE%d", line_counter);
	    strcpy(templine,null_val);
	    templine[strlen(null_val)] = '\0';
	    strcat(templine, line);
	    strcpy(line, templine);
    }

    //IF ",," exists in the line
    for(i = 0;i<strlen(line);i++){
	    if(line[i] == ',' && line[i+1] == ','){
	      char null_val[15];
	      char templine[600]; // Buffer to put the modified line in
	      strncpy(templine,line,i+1); //TODO: Change this to i+1 if it copies incorrent # of chars
	      templine[i+1] = '\0';
	      sprintf(null_val, "NULL_VALUE%d", line_counter);
	      strcat(templine,null_val);
	      strcat(templine,line+i+1);
	      strcpy(line, templine);
	    }
    }

    //IF the line ends with a ','
    if(line[strlen(line) -1] == ','){
      char null_val[15];
      sprintf(null_val, "NULL_VALUE%d", line_counter);
      strcat(line,null_val);
    }

    // Tokenize until end of line
    char *word;
    word = strtok(line, delims);
    
    while(word){
      //IF string has commas in the middle somwhere
	    if(word[0] == '\"'){
        char buffer[100];
        strcpy(buffer, word);
	      word = strtok(NULL, ",");
	      while(strpbrk(word, "\"") == NULL){
	        strcat(buffer, ",\0");
	        strcat(buffer, word);
	        word = strtok(NULL, ",");
	      }
	      strcat(buffer, ",\0");
        strcat(buffer, word);
	      strcat(buffer, "\0");
        big_db[0][big_lc]->col[word_counter] = (char *)malloc((strlen(buffer)+1)*sizeof(char));
	      strcpy(big_db[0][big_lc]->col[word_counter], buffer);
	      word_counter++;
	      word = strtok(NULL,",");
	      continue;
      }

      // Allocate enough space for the string to be placed in the array
      big_db[0][big_lc]->col[word_counter] = (char *)malloc((strlen(word)+1)*sizeof(char));
      // Copy the string into the array and add trailing string ender
      strcpy(big_db[0][big_lc]->col[word_counter], word);
      // Move to the next token
      word_counter++;
      word = strtok(NULL, delims);
    }
    word_counter = 0;
    line_counter++;
    big_lc++;
    big_db[0] = (data_row**)realloc(big_db[0], (sizeof(data_row)*(big_lc +1))); // Realloc DB
    big_db[0][big_lc] = (data_row*)malloc(sizeof(data_row)); //Malloc for next row
    // pthread_mutex_unlock(&MUTEX);
  }
  
  // Close the file and return the line counter
  fclose(csv_file);
  return big_lc;
}

void print_to_csv(data_row **db, int line_counter, char *file_path_name, char *first_line) {
  char buffer[200];
  FILE *f;
  f = fopen(file_path_name, "w");
  int i, j;
  for (i = -1; i < line_counter; i++) {
    //Print first line to csv
    if(i == -1){
        fprintf(f,first_line);
    	continue;
    }
    for (j = 0; j < 28; j++) {
      if(strstr(db[i]->col[j],"NULL") != NULL){
	    fprintf(f,",");
	    if(j == 27){
	        fprintf(f,"\n");
	    }
	    continue;
      }
      
      if(j != 27){
      	char tmp[200];
        strcpy(tmp,db[i]->col[j]);
        strcat(tmp,",\0");
     	fprintf(f,tmp);
     	memset(tmp,0,sizeof(tmp));
     	continue;
	  }
      fprintf(f,db[i]->col[j]);
    }
   }
   fclose(f);
}


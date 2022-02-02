#ifndef _IO_HELPER_H_
#define _IO_HELPER_H_

#define PROCESS_NAME_LEN 25

// promt operations
void print_usage(char *process_name);

// file operations
int open_file(char * file_path);

// print the message to the stderr end terminate program
void err_exit(char* error_msg);

#endif

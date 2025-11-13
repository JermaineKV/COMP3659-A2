#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// This file handles file operations (read/write) for the web server

/*******************************
read_file: reads the contents of a file
and returns a pointer to the data

*******************************/
read_file(const char* filepath, ssize_t file_size, char* buffer) {
    
    struct stat file_stat; // holds file information - does this need to be in globals?
    int fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        return -1; //file not found
    } 
    else 
    {
        file_size = file_stat.st_size; // get file size
    }

    close(fd);
}

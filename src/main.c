#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>



int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }
    
    const char *command = argv[1];
    
    if (strcmp(command, "init") == 0) {
        // You can use print statements as follows for debugging, they'll be visible when running tests.
        fprintf(stderr, "Logs from your program will appear here!\n");

        // Uncomment this block to pass the first stage
        
        if (mkdir(".git", 0755) == -1 || 
            mkdir(".git/objects", 0755) == -1 || 
            mkdir(".git/refs", 0755) == -1) {
            fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
            return 1;
        }
        
        FILE *headFile = fopen(".git/HEAD", "w");
        if (headFile == NULL) {
            fprintf(stderr, "Failed to create .git/HEAD file: %s\n", strerror(errno));
            return 1;
        }
        fprintf(headFile, "ref: refs/heads/main\n");
        fclose(headFile);
        
        printf("Initialized git directory\n");
    }

    else if (strcmp(command, "cat-file") == 0){
        if (argc < 4 || strcmp(argv[2], "-p") != 0){
            fprintf(stderr, "Usage: ./your_program cat-file -p <object_hash>\n", strerror(errno));
            return 1;
        }
        
        const char *object_hash = argv[3];
        char dir[3];
        char filename[39];
        const char *base_dir = ".git/objects/";
        char path[1024];

        // get the first two chars from hash
        strncpy(dir, object_hash, 2);
        dir[2] = '\0';

        // get the rest of the hash
        strncpy(filename, object_hash + 2, 38);
        filename[38] = '\0';  // Null terminate

        snprintf(path, sizeof(path), "%s%s/%s", base_dir, dir, filename);
    

        // Open file and validate if path doesnt exist
        FILE *file = fopen(path, "rb");
        if (!file){
            fprintf(stderr, "Error: Could not open object file %s: %s\n", path, strerror(errno));
            return 1;
        }

        // Get the size of the file
        long f_size;
        if (fseek(file, 0, SEEK_END) != 0 || (f_size = ftell(file)) == -1) {
            fprintf(stderr, "Error getting file size: %s\n", strerror(errno));
            fclose(file);
            return 1;
        }
        rewind(file);

        // Allocate memory
        unsigned char *compressed_data = malloc(f_size);
        if (!compressed_data){
            fprintf(stderr, "Memory allocation failed\n");
            fclose(file);
            return 1;
        }

        // Read the file
        fread(compressed_data, 1, f_size, file);
        fclose(file);

        // Initializing decompression and validate
        z_stream stream = {0};

        if (inflateInit(&stream) != Z_OK){
            fprintf(stderr, "Failed to initialize Zlib \n");
            free(compressed_data);
            return 1;
        } 

        unsigned long decompressed_size = f_size * 4;
        unsigned char *decompressed_data = malloc(decompressed_size);
        if (!decompressed_data) {
            fprintf(stderr, "Memory allocation failed for decompressed data\n");
            inflateEnd(&stream);
            free(compressed_data);
            return 1;
        }

        // Input and output buffers
        stream.next_in = compressed_data;
        stream.avail_in = f_size;
        stream.next_out = decompressed_data;
        stream.avail_out = decompressed_size;

        // Perform decompression
        while (1) {
            int status = inflate(&stream, Z_SYNC_FLUSH);
            if (status == Z_STREAM_END) break;
            if (status != Z_OK) {
                fprintf(stderr, "Failed to decompress data\n");
                inflateEnd(&stream);
                free(compressed_data);
                free(decompressed_data);
                return 1;
            }
        }
        inflateEnd(&stream);
        free(compressed_data);
        

        // Ensure decompressed data is null-terminated
        if (stream.total_out < decompressed_size) {
            decompressed_data[stream.total_out] = '\0';
        } 
        else {
            decompressed_data[decompressed_size - 1] = '\0'; // Ensure safe null termination
        }
         
        char *content = (char *)decompressed_data;
        char *null_pos = strchr(content, '\0');

        if (null_pos == NULL) {
            fprintf(stderr, "Invalid Git object format\n");
            free(decompressed_data);
            return 1;
        }

        content = null_pos + 1;
        printf("%s", content);

        free(decompressed_data);
    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }
    return 0;
}

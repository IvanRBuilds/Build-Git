#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <zlib.h>
#include <openssl/sha.h>


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
            fprintf(stderr, "Usage: ./your_program cat-file -p <object_hash>\n");
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

    else if (strcmp(command, "hash-object") == 0){
        if (argc < 4 || strcmp(argv[2], "-w") != 0){
            fprintf(stderr, "Usage: ./your-program hash-object -w <file>\n");
            return 1;
        }
        const char *file_name = argv[3];

        // Open file and validate
        FILE *file = fopen(file_name, "rb");
        if (!file){
            fprintf(stderr, "Error opening file %s: %s\n", file_name, strerror(errno));
            return 1;
        }

        // Get the size of the file
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        rewind(file);

        // Allocate appropiate amount of memory with validation
        unsigned char *file_content = malloc(file_size);
        if (!file_content){
            fprintf(stderr, "Memory allocation failed");
            fclose(file);
            return 1;
        }

        // Read the file then close it once file_content has the data
        fread(file_content, 1, file_size, file);
        fclose(file);

        // Build the blob
        char header[64];
        int header_length = snprintf(header, sizeof(header), "blob %ld", file_size);

        // Allocate appropriate memory for blob with validation
        unsigned long blob_size = header_length + 1 +file_size;
        unsigned char *blob = malloc(blob_size);
        if (!blob){
            fprintf(stderr, "Memory allocation failed for blob\n");
            free(file_content);
            return 1;
        }

        // Add to blob the header first
        memcpy(blob, header, header_length);

        // Add the null after the header
        blob[header_length] = '\0';

        // Add the file contents to the blob
        memcpy(blob + header_length + 1, file_content, file_size);

        // SSH build
        unsigned char sha1_hash[SHA_DIGEST_LENGTH]; // 20 bytes in length
        SHA1(blob, blob_size, sha1_hash);

        // Convert binary hash to hex hash
        char sha1_hash_hex[41]; // 40 chars but 1 byte for null
        for (int i = 0; i < SHA_DIGEST_LENGTH; i++){
            sprintf(sha1_hash_hex + (i * 2), "%02x", sha1_hash[i]);
        }
        sha1_hash_hex[40] = '\0';
        
        // Create directory and filename
        char dir[3];
        char filename[39];
        strncpy(dir, sha1_hash_hex, 2);
        dir[2] = '\0';
        strncpy(filename, sha1_hash_hex + 2, 38);
        filename[38] = '\0';
        
        // Create the actual directory
        char dir_path[256];
        snprintf(dir_path, sizeof(dir_path), ".git/objects/%s", dir);

        if (mkdir(dir_path, 0755) == -1 && errno!= EEXIST){
            fprintf(stderr, "Failed to create directory %s: %s\n", dir_path, strerror(errno));
            free(blob);
            free(file_content);
            return 1;
        }

        char object_path[256];
        snprintf(object_path, sizeof(object_path), ".git/objects/%s/%s", dir, filename);

        //Alocate space for data
        unsigned long compressed_max_size = compressBound(blob_size);
        unsigned char *compressed_blob = malloc(compressed_max_size);
        if (!compressed_blob){
            fprintf(stderr, "Memory allocation failed for compressed blob\n");
            free(blob);
            free(file_content);
            return 1;
        }

        // Perform compression
        if (compress(compressed_blob, &compressed_max_size, blob, blob_size) != Z_OK){
            fprintf(stderr, "Compression failed\n");
            free(blob);
            free(file_content);
            free(compressed_blob);
            return 1;
        }

        // Open the objext file for writing
        FILE *object_file = fopen(object_path, "wb");
        if (!object_file){
            fprintf(stderr, "Failed to create object file %s: %s\n", object_path, strerror(errno));
            free(blob);
            free(file_content);
            free(compressed_blob);
            return 1;
        }

        // Write the compressed blob into the file
        if (fwrite(compressed_blob, 1, compressed_max_size, object_file) != compressed_max_size){
            fprintf(stderr, "Failed to write compressed data to file\n");
            fclose(object_file);
            free(blob);
            free(file_content);
            free(compressed_blob);
            return 1;
        }

        // Clean up
        fclose(object_file);
        free(blob);
        free(file_content);
        free(compressed_blob);

    }
    
    else if (strcmp(command, "ls-tree") == 0){
        if (argc < 3){
            fprintf(stderr, "Usage: ./your-program ls-tree <tree_sha>\n");
            return 1;
        }
        // Set constant for reading sha
        const char *tree_sha = argv[2];
        char dir[3], filename[39], path[1024];

        strncpy(dir, tree_sha, 2);
        dir[2] = '\0';
        strncpy(filename, tree_sha + 2, 38);
        filename[38] = '\0';
        snprintf(path, sizeof(path), ".git/objects/%s/%s", dir, filename);

        // Open object file
        FILE *file = fopen(path, "rb");
        if (!file){
            fprintf(stderr, "Error opening tree object: %s\n", strerror(errno));
            return 1;
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long f_size = ftell(file);
        rewind(file);

        // Read compressed data
        unsigned char *compressed_data = malloc(f_size);
        fread(compressed_data, 1, f_size, file);
        fclose(file);

        // Prepare zlib stream
        z_stream stream = {0};
        inflateInit(&stream);

        stream.next_in = compressed_data;
        stream.avail_in = f_size;

        // Allocate space for decompressed data
        unsigned long out_size = f_size * 4; // TODO: Consider making a dynamic buffer for larger tree objects
        unsigned char *decompressed = malloc(out_size);
        stream.next_out = decompressed;
        stream.avail_out = out_size;

        // Decompress
        int result = inflate(&stream, Z_FINISH);
        if (result != Z_STREAM_END){
            fprintf(stderr, "Failed to decompress tree object\n");
            inflateEnd(&stream);
            free(compressed_data);
            free(decompressed);
            return 1;
        }

        // Decompressed now has the full tree object data
        inflateEnd(&stream);
        free(compressed_data);

        // Move pointer past "tree <size>\0"
        char *entry_ptr = (char*)decompressed; // type cast unsigned char to char
        while (*entry_ptr != '\0') entry_ptr++;
        entry_ptr++; // pass the null byte

        while (entry_ptr < (char*)decompressed + stream.total_out){
            // mode like "100646", max 6 + null
            char mode[7]; 
            int i = 0;
            while(*entry_ptr != ' '){
                mode[i++] = *entry_ptr++;
            }
            mode[i] = '\0';
            entry_ptr++; //skip space

            // Git filenames are usually < 256 characters
            char filename[256]; 
            i = 0;
            while (*entry_ptr != '\0'){
                filename[i++] = *entry_ptr++;
            }
            filename[i] = '\0';
            entry_ptr++; // skip null byte

            unsigned char sha_raw[20];
            memcpy(sha_raw, entry_ptr, 20);
            entry_ptr += 20;
            
            // 40 chars + null terminator
            char sha_hex[41];
            for (int j = 0; j < 20; j++){
                sprintf(sha_hex + j * 2, "%02x", sha_raw[j]);
            }
            sha_hex[40] = '\0';

            printf("%s %s %s\t%s\n", mode, strcmp(mode, "4000") == 0 ? "tree" : "blob", sha_hex, filename);
        
        }

    }
    else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }
    return 0;
}

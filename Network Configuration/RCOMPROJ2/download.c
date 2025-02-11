#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>


#define FTP_PORT 21                // Control connection port
#define BUFFER_SIZE 1024


// Structure to store parsed URL information for FTP connection
struct URL {
    char host[256];   // A little above 253 maximum
    char resrc[512];   // 512 if the url is too long, due to a very long query
    char file[256];   // A little above 255 which is the standard
    char ip[40];   // Guarantees coverage for IPv4(15) and IPv6(39)
    char user[64];   // Usual to be limited to 32 chars
    char pass[64];   // Complex passwords can reach 64 characters

};


// Function to parse the URL and extract FTP connection details
int parseurl(const char *urlinfo, struct URL *founded) {

    // Check if username and password are provided in the URL
    int pass = (strstr(urlinfo, "@") != NULL);  // Indicates presence of password
    int user = (strstr(urlinfo, ":") != NULL);  // Indicates presence of username

    if (!pass || !user) {
        // URL is for anonymous login, extract host and resource path
        int len = sscanf(urlinfo, "ftp://%[^/]/%s", founded->host, founded->resrc);
        if (len != 2) {
            perror("URL is invalid for anonymous login");
            return 1;
        }

        // Set default values for anonymous user and password
        strcpy(founded->user, "anonymous");
        strcpy(founded->pass, "anonymous");
    } else {
        // URL includes username and password, extract them along with host and resource path
        int len = sscanf(urlinfo, "ftp://%[^:]:%[^@]@%[^/]/%s", founded->user, founded->pass, founded->host, founded->resrc);
        if (len != 4) {
            perror("URL is invalid for login with user and password");
            return 1;
        }
    }

    // Extract file name from the last part of the URL path
    strcpy(founded->file, strrchr(urlinfo, '/') + 1);

    // Resolve the host to an IP address
    struct hostent *host;
    if ((host = gethostbyname(founded->host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    // Store the IP address
    strcpy(founded->ip, inet_ntoa(*((struct in_addr *) host->h_addr)));

    return 0;
}

// Function to send a command to the FTP server with an optional argument
void send_command(int sockfd, const char *command, const char *argument) {
    char buffer[BUFFER_SIZE];
    if (argument) {
        // If there's an argument, format the command with it
        snprintf(buffer, sizeof(buffer), "%s %s\r\n", command, argument);
    } else {
        // If no argument, just send the command
        snprintf(buffer, sizeof(buffer), "%s\r\n", command);
    }
    // Send the command over the socket
    write(sockfd, buffer, strlen(buffer));
    printf("Client: %s", buffer);
}

// Function to receive and print the server's response
void receive_response(int sockfd, char *buffer) {
    ssize_t bytes = read(sockfd, buffer, BUFFER_SIZE - 1);
    if (bytes > 0) {
        // Null-terminate the buffer and print the server's response
        buffer[bytes] = '\0';
        printf("Server: %s", buffer);
    } else if (bytes < 0) {
        // Handle read error
        perror("read()");
        exit(EXIT_FAILURE);
    }
}


// Function to parse the PASV response ip
void parse_pasv_response(const char *response, char *server_ip, int *data_port) {
    int a1, a2, a3, a4, p1, p2;

    // Check for "227 " at the beginning of the response
    if (strncmp(response, "227", 3) != 0) {
        fprintf(stderr, "Invalid PASV response: %s\r\n", response);
        exit(EXIT_FAILURE);
    }

    // Extract the six numbers from the response
    if (sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
               &a1, &a2, &a3, &a4, &p1, &p2) != 6) {
        fprintf(stderr, "Failed to parse PASV response: %s\r\n", response);
        exit(EXIT_FAILURE);
    }

    // Construct the server IP address
    sprintf(server_ip, "%d.%d.%d.%d", a1, a2, a3, a4);

    // Calculate the port
    *data_port = p1 * 256 + p2;

    printf("Parsed PASV response:\r\n");
    printf("Server IP: %s\r\n", server_ip);
    printf("Data Port: %d\r\n", *data_port);
}

int main(int argc , char *argv[]) {

    int control_sock, data_sock;
    struct sockaddr_in server_addr, data_addr;
    struct hostent *server;
    char buffer[BUFFER_SIZE];
    char server_ip[16];
    int data_port;
    
    if(argc != 2){
        printf("Input an URL please.\r\n");
        exit(-1);
    }

    struct URL urlinf;

    if(parseurl(argv[1], &urlinf) != 0){
        printf("Error parsing URL in main\r\n");
        exit(-1);
    }

    // Print the parsed URL information

    printf("Host: %s\r\n", urlinf.host);
    printf("resource: %s\r\n", urlinf.resrc);
    printf("File: %s\r\n", urlinf.file);
    printf("User: %s\r\n", urlinf.user);
    printf("Pass: %s\r\n", urlinf.pass);
    printf("IP: %s\r\n", urlinf.ip);

    

    // Step 1: Create the control socket

     memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(urlinf.ip);
    server_addr.sin_port = htons(FTP_PORT);

    if ((control_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating control socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(control_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    // Receive initial server response
    receive_response(control_sock, buffer);

    // Step 2: Anonymous login
    send_command(control_sock, "USER", urlinf.user);
    receive_response(control_sock, buffer);
    send_command(control_sock, "PASS", urlinf.pass);
    receive_response(control_sock, buffer);

    // Step 3: Enter passive mode
    send_command(control_sock, "PASV", NULL);
    receive_response(control_sock, buffer);

    // Step 4: Parse the PASV response
    parse_pasv_response(buffer, server_ip, &data_port);

    // Step 5: Create data socket
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating data socket");
        exit(EXIT_FAILURE);
    }

    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = inet_addr(server_ip);
    data_addr.sin_port = htons(data_port);

    if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        perror("Error connecting to data socket");
        exit(EXIT_FAILURE);
    }

    // Step 6: Request file transfer
    send_command(control_sock, "RETR", urlinf.resrc);
    receive_response(control_sock, buffer);

    // Step 7: Receive file data
    FILE *file = fopen(urlinf.file, "wb");
    if (!file) {
        perror("Error opening file");
        close(data_sock);
        exit(EXIT_FAILURE);
    }

    int bytes;
    while ((bytes = read(data_sock, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes, file);
    }

    if (bytes < 0) {
        perror("Error reading data from data socket");
    }

    fclose(file);
    close(data_sock);
    printf("File transfer completed.\r\n");

    // Step 8: Close control connection
    send_command(control_sock, "QUIT", NULL);
    receive_response(control_sock, buffer);

    close(control_sock);
    return 0;
}



// minimal TCP listener (will evolve)
// Build: make
// Run:   ./server 8080 server.log

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <LogsFile>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) { fprintf(stderr, "Invalid port\n"); return 1; }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }
    int yes = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in srv; memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, 32) < 0) { perror("listen"); return 1; }

    fprintf(stderr, "Server listening on %d\n", port);

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        const char *msg = "OK Welcome\n";
        send(cfd, msg, strlen(msg), 0);
        close(cfd);
    }
    close(sfd);
    return 0;
}
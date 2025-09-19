// minimal TCP listener + one thread per client + clients list + telemetry thread + 
// Server with roles (OBSERVER/ADMIN), AUTH, ROLE?, LIST USERS
// Build: make
// Run:   ./server 8080 server.log

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG   32
#define MAX_LINE  2048

typedef enum { ROLE_OBSERVER=0, ROLE_ADMIN=1 } role_t;

typedef struct client_s {
    int fd;
    struct sockaddr_in addr;
    struct client_s *next;
} client_t;

typedef struct session_s {
    int fd;
    struct sockaddr_in addr;
    role_t role;
    char name[64];
} session_t;

static client_t *g_clients = NULL;
static pthread_mutex_t g_clients_mx = PTHREAD_MUTEX_INITIALIZER;

// ---------------- client registry ----------------
static void add_client(client_t *c) {
    pthread_mutex_lock(&g_clients_mx);
    c->next = g_clients;
    g_clients = c;
    pthread_mutex_unlock(&g_clients_mx);
}

static void remove_client(int fd) {
    pthread_mutex_lock(&g_clients_mx);
    client_t **pp = &g_clients, *c = g_clients;
    while (c) {
        if (c->fd == fd) {
            *pp = c->next;
            close(c->fd);
            free(c);
            break;
        }
        pp = &c->next;
        c = c->next;
    }
    pthread_mutex_unlock(&g_clients_mx);
}

static void list_users_to(int fd) {
    pthread_mutex_lock(&g_clients_mx);
    int count = 0;
    for (client_t *c = g_clients; c; c = c->next) count++;
    dprintf(fd, "OK %d users\n", count);
    for (client_t *c = g_clients; c; c = c->next) {
        char ip[64];
        inet_ntop(AF_INET, &c->addr.sin_addr, ip, sizeof(ip));
        dprintf(fd, "USER %s:%u ROLE=? NAME=?\n", ip, ntohs(c->addr.sin_port));
    }
    pthread_mutex_unlock(&g_clients_mx);
}

// ---------------- client thread ----------------
static void *client_thread(void *arg) {
    client_t *cli = (client_t*)arg;
    session_t s = { .fd = cli->fd, .addr = cli->addr, .role = ROLE_OBSERVER, .name = "" };

    send(s.fd, "OK Welcome. Commands: HELLO|AUTH|ROLE?|LIST USERS|QUIT\n", 55, 0);

    char buf[MAX_LINE], *p, *nl;
    for (;;) {
        ssize_t n = recv(s.fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        p = buf;
        while ((nl = strchr(p, '\n'))) {
            *nl = '\0';
            size_t L = strlen(p);
            if (L && p[L - 1] == '\r') p[L - 1] = '\0';

            if (strcmp(p, "QUIT") == 0) {
                send(s.fd, "BYE\n", 4, 0);
                goto out;
            } else if (strncmp(p, "HELLO", 5) == 0) {
                const char *k = strstr(p, "name=");
                if (k) {
                    k += 5;
                    strncpy(s.name, k, sizeof(s.name) - 1);
                }
                dprintf(s.fd, "OK hello %s\n", s.name[0] ? s.name : "observer");
            } else if (strncmp(p, "AUTH ", 5) == 0) {
                char u[64] = {0}, pw[64] = {0};
                if (sscanf(p + 5, "%63s %63s", u, pw) == 2 &&
                    strcmp(u, "admin") == 0 && strcmp(pw, "admin123") == 0) {
                    s.role = ROLE_ADMIN;
                    dprintf(s.fd, "OK admin\n");
                } else {
                    dprintf(s.fd, "ERR invalid credentials\n");
                }
            } else if (strcmp(p, "ROLE?") == 0) {
                dprintf(s.fd, "OK %s\n", s.role == ROLE_ADMIN ? "ADMIN" : "OBSERVER");
            } else if (strcmp(p, "LIST USERS") == 0) {
                if (s.role != ROLE_ADMIN)
                    dprintf(s.fd, "ERR forbidden\n");
                else
                    list_users_to(s.fd);
            } else {
                dprintf(s.fd, "ERR unknown\n");
            }
            p = nl + 1;
        }
    }

out:
    remove_client(s.fd);
    return NULL;
}

// ---------------- main ----------------
int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <LogsFile>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }
    int yes = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in srv; memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons((uint16_t)port);
    if (bind(sfd, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, BACKLOG) < 0) { perror("listen"); return 1; }

    fprintf(stderr, "Server listening on %d\n", port);

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr*)&cli, &cl);
        if (cfd < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        client_t *c = calloc(1, sizeof(*c));
        c->fd = cfd; c->addr = cli; add_client(c);
        pthread_t th; pthread_create(&th, NULL, client_thread, c); pthread_detach(th);
    }
    close(sfd);
    return 0;
}
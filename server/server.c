// minimal TCP listener + one thread per client + clients list + telemetry thread
// + Server with roles (OBSERVER/ADMIN), AUTH, ROLE?, LIST USERS
// + Vehicle state + control commands (SPEED UP/DOWN, TURN LEFT/RIGHT)
// + Telemetry broadcaster every 10 seconds to all clients
// Build: make
// Run:   ./server <port> <LogsFile>   (LogsFile ignored in v5; added in v6)

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BACKLOG   32
#define MAX_LINE  2048

typedef enum { ROLE_OBSERVER=0, ROLE_ADMIN=1 } role_t;
typedef enum { DIR_N=0, DIR_E=1, DIR_S=2, DIR_W=3 } dir_t;

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

// ---------- global client registry ----------
static client_t *g_clients = NULL;
static pthread_mutex_t g_clients_mx = PTHREAD_MUTEX_INITIALIZER;

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

// ---------- vehicle state + control ----------
static pthread_mutex_t g_state_mx = PTHREAD_MUTEX_INITIALIZER;
static int   g_speed   = 0;    // 0..100
static int   g_battery = 100;  // 0..100
static int   g_temp    = 35;   // Celsius
static dir_t g_dir     = DIR_N;

static const char* dir_str(dir_t d) {
    switch (d) {
        case DIR_N: return "N";
        case DIR_E: return "E";
        case DIR_S: return "S";
        case DIR_W: return "W";
        default:    return "N";
    }
}

// change speed by +5 or -5; returns 1 OK / 0 ERR and writes reason/status in `why`
static int apply_speed_change(int delta, char *why, size_t wsz) {
    int ok = 1;
    pthread_mutex_lock(&g_state_mx);
    if (g_battery < 15) {
        snprintf(why, wsz, "battery low");
        ok = 0;
    } else {
        int ns = g_speed + delta;
        if (ns < 0)  { ns = 0;  snprintf(why, wsz, "min speed"); ok = 0; }
        else if (ns > 100) { ns = 100; snprintf(why, wsz, "max speed"); ok = 0; }
        else { g_speed = ns; snprintf(why, wsz, "speed=%d", g_speed); ok = 1; }
    }
    pthread_mutex_unlock(&g_state_mx);
    return ok;
}

static void apply_turn_left(int is_left) {
    pthread_mutex_lock(&g_state_mx);
    int di = (int)g_dir + (is_left ? -1 : +1);
    if (di < 0) di = 3;
    if (di > 3) di = 0;
    g_dir = (dir_t)di;
    pthread_mutex_unlock(&g_state_mx);
}

// ---------- telemetry broadcaster (every 10s) ----------
static void broadcast_tlm(void) {
    char line[256];
    time_t now = time(NULL);

    pthread_mutex_lock(&g_state_mx);
    int sp = g_speed, bt = g_battery, tp = g_temp;
    const char *ds = dir_str(g_dir);
    pthread_mutex_unlock(&g_state_mx);

    int n = snprintf(line, sizeof(line),
        "TLM speed=%d;battery=%d;temp=%d;dir=%s;ts=%ld\n",
        sp, bt, tp, ds, (long)now);

    pthread_mutex_lock(&g_clients_mx);
    for (client_t *c = g_clients; c; c = c->next) {
        if (c->fd >= 0) send(c->fd, line, (size_t)n, 0);
    }
    pthread_mutex_unlock(&g_clients_mx);
}

static void *telemetry_thread(void *arg) {
    (void)arg;
    for (;;) {
        // simple simulation for battery/temp
        pthread_mutex_lock(&g_state_mx);
        if (g_speed > 0 && g_battery > 0) g_battery -= (g_speed >= 60 ? 2 : 1);
        if (g_battery < 0) g_battery = 0;
        if (g_speed > 70 && g_temp < 80) g_temp++;
        else if (g_temp > 35) g_temp--;
        pthread_mutex_unlock(&g_state_mx);

        broadcast_tlm();
        for (int i = 0; i < 10; ++i) sleep(1);
    }
    return NULL;
}

// ---------- client thread: protocol parsing ----------
static void *client_thread(void *arg) {
    client_t *cli = (client_t*)arg;
    session_t s = { .fd = cli->fd, .addr = cli->addr, .role = ROLE_OBSERVER, .name = "" };

    send(s.fd,
         "OK Welcome. Commands: HELLO|AUTH|ROLE?|LIST USERS|SPEED ...|TURN ...|QUIT\n",
         73, 0);

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

            // --- commands ---
            if (strcmp(p, "QUIT") == 0) {
                send(s.fd, "BYE\n", 4, 0);
                goto out;

            } else if (strncmp(p, "HELLO", 5) == 0) {
                const char *k = strstr(p, "name=");
                if (k) {
                    k += 5;
                    while (*k == ' ') k++;
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
                if (s.role != ROLE_ADMIN) dprintf(s.fd, "ERR forbidden\n");
                else list_users_to(s.fd);

            } else if (strcmp(p, "SPEED UP") == 0 || strcmp(p, "SLOW DOWN") == 0) {
                if (s.role != ROLE_ADMIN) {
                    dprintf(s.fd, "ERR forbidden\n");
                } else {
                    char why[64];
                    int ok = apply_speed_change(strstr(p, "UP") ? +5 : -5, why, sizeof(why));
                    dprintf(s.fd, "%s %s\n", ok ? "OK" : "ERR", why);
                }

            } else if (strcmp(p, "TURN LEFT") == 0 || strcmp(p, "TURN RIGHT") == 0) {
                if (s.role != ROLE_ADMIN) {
                    dprintf(s.fd, "ERR forbidden\n");
                } else {
                    apply_turn_left(strstr(p, "LEFT") != NULL);
                    pthread_mutex_lock(&g_state_mx);
                    const char *ds = dir_str(g_dir);
                    pthread_mutex_unlock(&g_state_mx);
                    dprintf(s.fd, "OK dir=%s\n", ds);
                }

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

// ---------- main ----------
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

    struct sockaddr_in srv; bzero(&srv, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    srv.sin_port = htons((uint16_t)port);
    if (bind(sfd, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, BACKLOG) < 0) { perror("listen"); return 1; }

    // start telemetry thread
    pthread_t th_tlm; pthread_create(&th_tlm, NULL, telemetry_thread, NULL);
    pthread_detach(th_tlm);

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

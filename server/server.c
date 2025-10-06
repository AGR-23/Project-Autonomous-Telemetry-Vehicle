// Autonomous Vehicle Project - C Server (Berkeley Sockets + pthreads)
// Transport: TCP (control + telemetry)
// Run: ./server <port> <LogsFile>
//
// Application protocol (text, \n-terminated):
//  Client -> Server:
//    HELLO [name=<text>]
//    AUTH <user> <pass>          (admin: admin / admin123)
//    ROLE?
//    LIST USERS                  (ADMIN only)
//    SPEED UP | SLOW DOWN        (ADMIN only)
//    TURN LEFT | TURN RIGHT      (ADMIN only)
//    QUIT
//  Server -> Client:
//    OK <msg> | ERR <reason> | BYE
//    TLM speed=<int>;battery=<int>;temp=<int>;dir=<N|E|S|W>;ts=<epoch>
//
// Concurrency: 1 thread per client + 1 telemetry broadcaster thread (every 10s)
// Logging: console + file with timestamp and client ip:port

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
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
    int fd; struct sockaddr_in addr;
    struct client_s *next;
} client_t;

typedef struct session_s {
    int fd; struct sockaddr_in addr; role_t role;
    char name[64];
} session_t;

// Globals
static volatile sig_atomic_t g_sigstop = 0;
static atomic_int g_stop = 0;
static FILE *g_logf = NULL;
static pthread_mutex_t g_log_mx = PTHREAD_MUTEX_INITIALIZER;

static client_t *g_clients = NULL;
static pthread_mutex_t g_clients_mx = PTHREAD_MUTEX_INITIALIZER;

// Vehicle state
static pthread_mutex_t g_state_mx = PTHREAD_MUTEX_INITIALIZER;
static int   g_speed   = 0;   // 0..100
static int   g_battery = 100; // 0..100
static int   g_temp    = 35;  // °C
static dir_t g_dir     = DIR_N;

// ---------- Utils / Logging ----------
static void on_sigint(int sig){ (void)sig; g_sigstop = 1; atomic_store(&g_stop,1); }

static void peer_id(const struct sockaddr_in* a, char *out, size_t sz){
    char ip[64]; inet_ntop(AF_INET,&a->sin_addr,ip,sizeof(ip));
    snprintf(out,sz,"%s:%u", ip, ntohs(a->sin_port));
}

static void log_line(const char *peer, const char *fmt, ...){
    va_list ap; va_start(ap, fmt);
    pthread_mutex_lock(&g_log_mx);
    time_t now=time(NULL); struct tm tm; localtime_r(&now,&tm);
    char ts[32]; strftime(ts,sizeof(ts),"%Y-%m-%d %H:%M:%S",&tm);
    fprintf(stderr,"[%s] %s ", ts, peer?peer:"-");
    vfprintf(stderr, fmt, ap); fprintf(stderr,"\n");
    if (g_logf){
        fprintf(g_logf,"[%s] %s ", ts, peer?peer:"-");
        va_list ap2; va_start(ap2,fmt);
        vfprintf(g_logf, fmt, ap2); va_end(ap2);
        fprintf(g_logf,"\n"); fflush(g_logf);
    }
    pthread_mutex_unlock(&g_log_mx);
    va_end(ap);
}

static const char* dir_str(dir_t d){ return (d==DIR_N?"N":d==DIR_E?"E":d==DIR_S?"S":"W"); }

// ---------- Client registry ----------
static void add_client(client_t *c){
    pthread_mutex_lock(&g_clients_mx);
    c->next = g_clients; g_clients = c;
    pthread_mutex_unlock(&g_clients_mx);
}
static void remove_client(int fd){
    pthread_mutex_lock(&g_clients_mx);
    client_t **pp=&g_clients, *c=g_clients;
    while(c){ if(c->fd==fd){ *pp=c->next; close(c->fd); free(c); break; } pp=&c->next; c=c->next; }
    pthread_mutex_unlock(&g_clients_mx);
}
static void list_users_to(int fd){
    pthread_mutex_lock(&g_clients_mx);
    int count=0; for(client_t *c=g_clients;c;c=c->next) count++;
    dprintf(fd, "OK %d users\n", count);
    for(client_t *c=g_clients;c;c=c->next){
        char ip[64]; inet_ntop(AF_INET,&c->addr.sin_addr,ip,sizeof(ip));
        dprintf(fd, "USER %s:%u ROLE=? NAME=?\n", ip, ntohs(c->addr.sin_port));
    }
    pthread_mutex_unlock(&g_clients_mx);
}

// ---------- Vehicle control ----------
static int apply_speed_change(int delta, char *why, size_t wsz){
    int ok=1;
    pthread_mutex_lock(&g_state_mx);
    if (g_battery < 15) { snprintf(why,wsz,"battery low"); ok=0; }
    else {
        int ns = g_speed + delta;
        if (ns < 0) { ns = 0; snprintf(why,wsz,"min speed"); ok=0; }
        else if (ns > 100) { ns = 100; snprintf(why,wsz,"max speed"); ok=0; }
        else { g_speed = ns; snprintf(why,wsz,"speed=%d", g_speed); ok=1; }
    }
    pthread_mutex_unlock(&g_state_mx);
    return ok;
}
static void apply_turn_left(int left){
    pthread_mutex_lock(&g_state_mx);
    int di = (int)g_dir + (left?-1:+1);
    if(di<0) di=3; if(di>3) di=0; g_dir=(dir_t)di;
    pthread_mutex_unlock(&g_state_mx);
}

// ---------- Telemetry ----------
static void broadcast_tlm(void){
    char line[256];
    time_t now=time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    pthread_mutex_lock(&g_state_mx);
    int sp=g_speed, bt=g_battery, tp=g_temp;
    const char* ds=dir_str(g_dir);
    pthread_mutex_unlock(&g_state_mx);
    snprintf(line, sizeof(line), "TLM speed=%d;battery=%d;temp=%d;dir=%s;ts=%s\n", 
             sp, bt, tp, ds, ts);    
    pthread_mutex_lock(&g_clients_mx);
    for(client_t *c=g_clients; c; c=c->next) send(c->fd, line, strlen(line), 0);
    pthread_mutex_unlock(&g_clients_mx);
}
static void *telemetry_thread(void *arg){
    (void)arg;
    while(!atomic_load(&g_stop)){
        pthread_mutex_lock(&g_state_mx);
        if (g_speed>0 && g_battery>0) g_battery -= (g_speed>=60?2:1);
        if (g_battery < 0) g_battery = 0;
        if (g_speed>70 && g_temp<80) g_temp++; else if (g_temp>35) g_temp--;
        pthread_mutex_unlock(&g_state_mx);
        broadcast_tlm();
        for(int i=0;i<10 && !atomic_load(&g_stop);i++) sleep(1);
    }
    return NULL;
}

// ---------- Client thread ----------
static void *client_thread(void *arg){
    client_t *cli=(client_t*)arg;
    session_t s = { .fd=cli->fd, .addr=cli->addr, .role=ROLE_OBSERVER, .name = "" };
    char pid[80]; peer_id(&s.addr,pid,sizeof(pid));
    log_line(pid, "connected");

    dprintf(s.fd,"OK Welcome. Commands: HELLO|AUTH|ROLE?|LIST USERS|SPEED ...|TURN ...|QUIT\n");

    char buf[MAX_LINE], *p, *nl;
    for(;;){
        ssize_t n = recv(s.fd, buf, sizeof(buf)-1, 0);
        if(n<=0) break; buf[n]='\0'; p=buf;
        while((nl=strchr(p,'\n'))){
            *nl='\0'; size_t L=strlen(p); if(L&&p[L-1]=='\r') p[L-1]='\0';
            log_line(pid, "REQ: %s", p);

            if(strcmp(p,"QUIT")==0){
                dprintf(s.fd,"BYE\n"); log_line(pid,"BYE"); goto out;
            } else if(strncmp(p,"HELLO",5)==0){
                const char *k=strstr(p,"name="); if(k){ k+=5; while(*k==' ') k++; strncpy(s.name,k,sizeof(s.name)-1); }
                dprintf(s.fd,"OK hello %s\n", s.name[0]?s.name:"observer");
            } else if(strncmp(p,"AUTH ",5)==0){
                char u[64]={0}, pw[64]={0};
                if(sscanf(p+5,"%63s %63s",u,pw)==2 && strcmp(u,"admin")==0 && strcmp(pw,"admin123")==0){
                    s.role=ROLE_ADMIN; dprintf(s.fd,"OK admin\n");
                } else dprintf(s.fd,"ERR invalid credentials\n");
            } else if(strcmp(p,"ROLE?")==0){
                dprintf(s.fd,"OK %s\n", s.role==ROLE_ADMIN?"ADMIN":"OBSERVER");
            } else if(strcmp(p,"LIST USERS")==0){
                if(s.role!=ROLE_ADMIN) dprintf(s.fd,"ERR forbidden\n");
                else list_users_to(s.fd);
            } else if(strcmp(p,"SPEED UP")==0 || strcmp(p,"SLOW DOWN")==0){
                if(s.role!=ROLE_ADMIN) dprintf(s.fd,"ERR forbidden\n");
                else { char why[64]; int ok=apply_speed_change(strstr(p,"UP")?+5:-5, why, sizeof(why));
                       dprintf(s.fd,"%s %s\n", ok?"OK":"ERR", why); }
            } else if(strcmp(p,"TURN LEFT")==0 || strcmp(p,"TURN RIGHT")==0){
                if(s.role!=ROLE_ADMIN) dprintf(s.fd,"ERR forbidden\n");
                else { apply_turn_left(strstr(p,"LEFT")!=NULL);
                       pthread_mutex_lock(&g_state_mx);
                       const char *ds=dir_str(g_dir);
                       pthread_mutex_unlock(&g_state_mx);
                       dprintf(s.fd,"OK dir=%s\n", ds); }
            } else {
                dprintf(s.fd,"ERR unknown\n");
            }
            log_line(pid, "DONE");
            p = nl+1;
        }
    }
out:
    remove_client(s.fd);
    return NULL;
}

// ---------- main ----------
int main(int argc, char **argv){
    if (argc!=3){ fprintf(stderr,"Usage: %s <port> <LogsFile>\n", argv[0]); return 1; }
    int port = atoi(argv[1]); if(port<=0 || port>65535){ fprintf(stderr,"Invalid port\n"); return 1; }
    g_logf = fopen(argv[2],"a"); /* optional */

    signal(SIGINT, on_sigint);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd<0){ perror("socket"); return 1; }
    int yes=1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in srv; bzero(&srv,sizeof(srv));
    srv.sin_family=AF_INET; srv.sin_addr.s_addr=htonl(INADDR_ANY); srv.sin_port=htons((uint16_t)port);
    if (bind(sfd,(struct sockaddr*)&srv,sizeof(srv))<0){ perror("bind"); return 1; }
    if (listen(sfd,BACKLOG)<0){ perror("listen"); return 1; }

    pthread_t th_tlm; pthread_create(&th_tlm,NULL,telemetry_thread,NULL);

    fprintf(stderr,"Server listening on %d (Ctrl+C to stop)\n", port);
    for(;;){
        if (g_sigstop) break;
        struct sockaddr_in cli; socklen_t cl=sizeof(cli);
        int cfd = accept(sfd,(struct sockaddr*)&cli,&cl);
        if (cfd<0){ if(errno==EINTR) continue; perror("accept"); break; }
        client_t *c = calloc(1,sizeof(*c)); c->fd=cfd; c->addr=cli; add_client(c);
        pthread_t th; pthread_create(&th,NULL,client_thread,c); pthread_detach(th);
    }
    atomic_store(&g_stop,1);
    pthread_join(th_tlm,NULL);

    pthread_mutex_lock(&g_clients_mx);
    for(client_t *c=g_clients;c;){
        client_t *n=c->next; close(c->fd); free(c); c=n;
    }
    g_clients=NULL;
    pthread_mutex_unlock(&g_clients_mx);

    close(sfd);
    if (g_logf) fclose(g_logf);
    return 0;
}
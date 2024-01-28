#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define NUM_PROC 5
#define TAMP_SIZE 3
#define TAMP_KEY 16
#define MSG_STR_SIZE 30

#pragma region RETURNS
// demand is too big, waiting for more resources to be available...
#define ALC_2BIG 1
// allocated successfully.
#define ALC_SUCCESS 0
// empty wait queue.
#define ALC_NOBLQ -1
// no donors to provide resources.
#define ALC_NODNR -2
#pragma endregion

#define SEM_MUTEX_FNAME "/sem_mutex"
#define SEM_NVIDE_FNAME "/sem_nvide"

#pragma region STRUCTS
typedef struct {
    int pid, type, s1, s2, s3;
} req_t;

typedef struct {
    bool bloq;
    int tmpAtt;
} procInf_t;

typedef struct {
    int s1, s2, s3;
} alloc_t;

typedef struct {
    req_t T[TAMP_SIZE];
    int cpt;
} tamp_t;

typedef struct {
    bool type;
    char text[MSG_STR_SIZE];
} message_t;

typedef struct {
    long type;
    message_t body;
} mesg_buffer;
#pragma endregion

sem_t *mutex, *nvide;
int tampon_id;
tamp_t* tampon;

int msg_qids[NUM_PROC+1];
const char msg_lib_format[] = "liber(%d, %d, %d)";
const char msg_aloc_success[] = "resource allocated.";
const char msg_aloc_fail[] = "failed to allocate.";
const char msg_lib_success[] = "resource liberated.";
const char msg_lib_fail[] = "failed to liberate.";

void dispo_sub(int *dispo, int *demand, int* aloc){
    if(*dispo >= *demand){
        *dispo -= *demand;
        *aloc += *demand;
        *demand = 0;
    }else{
        int rest = *demand - *dispo;
        *demand -= rest;
        *dispo -= *demand;
        *aloc += *demand;
        *demand = rest;
    }
}

bool is_in(int *T, int a, int size){
    for(int i = 0; i<size ; i++){
        if(T[i]==a)
            return true;
    }
    return false;
}

void status(int Dispo[], req_t Dem[], procInf_t Stat[], alloc_t Alloc[]) {
    int p;
    printf("  Dispo:\n");
    printf("| %8d | %8d | %8d |\n", Dispo[0], Dispo[1], Dispo[2]);
    printf("----------------------------------\n");
    printf("  Demandes:\n| %-8s | %-8s | %-8s | %-8s | %-8s |\n", "pid", "type", "size1", "size2", "size3");
    for (p = 0; p < NUM_PROC; p++) {
        printf("| %8d | %8d | %8d | %8d | %8d |\n", p + 1, Dem[p].type, Dem[p].s1, Dem[p].s2, Dem[p].s3);
    }
    printf("-----------------------------------------------------\n");
    printf("  Status:\n| %-8s | %-8s | %-8s |\n", "pid", "blocked", "tmp_att");
    for (p = 0; p < NUM_PROC; p++) {
        printf("| %8d | %-8s | %8d |\n", p + 1, Stat[p].bloq ? "true" : "false", Stat[p].tmpAtt);
    }
    printf("---------------------------------\n");
    printf("  Allocations:\n| %-8s | %-8s | %-8s | %-8s |\n", "pid", "size1", "size2", "size3");
    for (p = 0; p < NUM_PROC; p++) {
        printf("| %8d | %8d | %8d | %8d |\n", p + 1, Alloc[p].s1, Alloc[p].s2, Alloc[p].s3);
    }
    printf("-----------------------------------------------\n");
}

int satisfy(int* Dispo, req_t* Dem, alloc_t* Alloc, procInf_t* Stat){
    // find the most desering process
    int target = -1, max_wait = -1;
    for(int p=0; p<NUM_PROC ; p++){
        if(Stat[p].bloq && Stat[p].tmpAtt>max_wait){
            max_wait = Stat[p].tmpAtt;
            target = p;
        }
    }
    if(target == -1)
        return ALC_NOBLQ;

    bool satisfied = false;
    // provide resources from Dispo
    dispo_sub(&Dispo[0], &Dem[target].s1, &Alloc[target].s1);
    dispo_sub(&Dispo[1], &Dem[target].s2, &Alloc[target].s2);
    dispo_sub(&Dispo[2], &Dem[target].s3, &Alloc[target].s3);
    if(Dem[target].s1 == 0 && Dem[target].s2 == 0 && Dem[target].s3 == 0){
        satisfied = true;
        // send message to liberate
        mesg_buffer msg = {.type = (long)(100+target), .body = {.type = true}};
        strcpy(msg.body.text, msg_aloc_success);
        msgsnd(msg_qids[0], &msg, sizeof(msg.body), 0);
        // reset
        Stat[target].bloq = false;
        Stat[target].tmpAtt = 0;

        return ALC_SUCCESS;
    }

    // provide resources from other processes, starting from the newest in queue
    // check for potential donors
    int d_count=0;
    for(int d=0; d<NUM_PROC ; d++){
        if(d != target && Stat[d].bloq)
            d_count++;
    }
    if(d_count==0)
        return ALC_NODNR;
    int dsize = d_count; //**dont remove.**
    int *donors = malloc(sizeof(int)*dsize);

    // find a source
    int source = -1, min_wait = INT_MAX;
    while(d_count && !satisfied){
        for(int p = 0; p<NUM_PROC ; p++){
            if(p != target && Stat[p].bloq && Stat[p].tmpAtt < min_wait 
                    && !is_in(donors, p, dsize)){
                source = p;
                min_wait = Stat[p].tmpAtt;
            }
        }
        if(source == -1)
            return ALC_NODNR;
        int before = Alloc[target].s1;
        dispo_sub(&Alloc[source].s1, &Dem[target].s1, &Alloc[target].s1);
        Dem[source].s1 += Alloc[target].s1 - before;

        before = Alloc[target].s2;
        dispo_sub(&Alloc[source].s2, &Dem[target].s2, &Alloc[target].s2);
        Dem[source].s2 += Alloc[target].s2 - before;

        before = Alloc[target].s3;
        dispo_sub(&Alloc[source].s3, &Dem[target].s3, &Alloc[target].s3);
        Dem[source].s3 += Alloc[target].s3 - before;

        if(Dem[target].s1 == 0 && Dem[target].s2 == 0 && Dem[target].s3 == 0){
            satisfied = true;
            // send message to liberate
            mesg_buffer msg = {.type = (long)(100+target), .body = {.type = true}};
            strcpy(msg.body.text, msg_aloc_success);
            msgsnd(msg_qids[0], &msg, sizeof(msg.body), 0);
            // reset
            Stat[target].bloq = false;
            Stat[target].tmpAtt = 0;

            return ALC_SUCCESS;
        }
        d_count--;
        donors[d_count] = source;
    };

    free(donors);
    return ALC_2BIG;
}

// append
void req_send(req_t* request) {
    sem_wait(nvide);
    sem_wait(mutex);
    memcpy(&tampon->T[tampon->cpt], request, sizeof(req_t));
    tampon->cpt++;
    sem_post(mutex);
}

// pull
bool req_receive(req_t* request) {
    sem_wait(mutex);
    if (tampon->cpt > 0) {
        memcpy(request, &tampon->T[0], sizeof(req_t));
        for (int _ = 0; _ < tampon->cpt - 1; _++)
            memcpy(&tampon->T[_], &tampon->T[_ + 1], sizeof(req_t));
        tampon->cpt--;
        sem_post(nvide);
        sem_post(mutex);
        return true;
    } else {
        sem_post(mutex);
        return false;
    }
}

void Calcul(int pid) {
#pragma region INIT
    // find semaphores
    nvide = sem_open(SEM_NVIDE_FNAME, 0);
    mutex = sem_open(SEM_MUTEX_FNAME, 0);
    if (nvide == SEM_FAILED || mutex == SEM_FAILED) {
        printf("sem_open()/Calcul%d, %s\n", pid, strerror(errno));
        return;
    }
    // find message queue.
    if (msg_qids[pid] != msgget((key_t)(60 + pid), 0666)) {
        printf("\tERR: missing message queue for Calcul %d\n", pid);
        return;
    }
    // find shared mem block
    // find & attach shared mem block
    if (tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666)) {
        printf("\tERR: Couldn't find shared mem block for Calcul %d.\n", pid);
        return;
    }
    tampon = (tamp_t*)shmat(tampon_id, NULL, 0);
    if (!tampon) {
        printf("\tERR: attaching shared mem block failed for Calcul %d.\n", pid);
        return;
    }

    // Instruction file
    char filename[25];
    sprintf(filename, "./Instructions/Set%d.ins", pid);
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("\tERR: Couldn't find instruction file %d.\n", pid);
    }

    req_t request = { .pid = pid, .type = 1, .s1 = 0, .s2 = 0, .s3 = 0 };
    mesg_buffer message = {.type = (long)(100+pid), .body = {.type = false, .text = "\0"} };
#pragma endregion

    bool running = true;
    while (running) {
        fscanf(f, "%d %d %d %d", &request.type, &request.s1, &request.s2, &request.s3);
        // printf("__[Calcul %d]::(%d, %d, %d, %d)__\n", request.pid, request.type, request.s1, request.s2, request.s3);
        switch (request.type) {
            case 2:
                req_send(&request);
                // src_pid 0 = Gerant
                while(msgrcv(msg_qids[0], &message, sizeof(message.body), (long)(100+pid), IPC_NOWAIT) <= 0 || message.body.type == false || strcmp(message.body.text, msg_aloc_success)){};
                // reset
                message.body.text[0] = '\0';
                message.body.type = false;
                break;
            case 3:
                sprintf(message.body.text, msg_lib_format, request.s1, request.s2, request.s3);
                msgsnd(msg_qids[pid], &message, sizeof(message.body), 0);
                while(msgrcv(msg_qids[0], &message, sizeof(message.body), (long)(100+pid), IPC_NOWAIT) <= 0 || message.body.type == false || strcmp(message.body.text, msg_lib_success)){};
                //  reset
                message.body.text[0] = '\0';
                message.body.type = false;
                break;
            case 4:
                req_send(&request);
                running = false;
                break;
            default:
                break;
        }
        sleep(1);
    }

    shmdt(tampon);
}

void Gerant() {
#pragma region INIT
    // find semaphores
    nvide = sem_open(SEM_NVIDE_FNAME, 0);
    mutex = sem_open(SEM_MUTEX_FNAME, 0);

    if (nvide == SEM_FAILED || mutex == SEM_FAILED) {
        printf("sem_open()/Gerant, %s\n", strerror(errno));
        return;
    }
    // find message queue.
    for (int p = 0; p < NUM_PROC; p++) {
        if (msg_qids[p] != msgget((key_t)(60 + p), 0666)) {
            printf("\tERR: missing message queue for Gerant\n");
            return;
        }
    }
    // find & attach shared mem block
    if (tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666)) {
        printf("\tERR: Couldn't find shared mem block for Gerant.\n");
        return;
    }
    tampon = (tamp_t*)shmat(tampon_id, NULL, 0);
    if (!tampon) {
        printf("\tERR: attaching shared mem block failed for Gerant.\n");
        return;
    }
    int NbProc = NUM_PROC;

    req_t request = { .pid = 0, .type = 1, .s1 = 0, .s2 = 0, .s3 = 0 };
    mesg_buffer msg_buf; msg_buf.body.text[0] = '\0';
    int Dispo[3] = { 100, 100, 100 };
    req_t Dem[NUM_PROC] = { request, request, request, request, request };
    alloc_t Alloc[NUM_PROC] = { {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0} };
    procInf_t Stat[NUM_PROC] = { {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0} };
#pragma endregion

    while (NbProc) {
        if (req_receive(&request)) {
            printf("[_PID: %d, request: %d_]\n", request.pid, request.type);
            switch(request.type){
                case 2:
                    Dem[request.pid-1] = request;
                    Stat[request.pid-1].bloq = true;
                    break;
                case 4:
                    NbProc--;
                    break;
                default:
                    break;
            }
            // reset
            request.pid = 0;
            request.type = 1;
            request.s1 = 0;
            request.s2 = 0;
            request.s3 = 0;
        }

        for(int p=1; p <= NUM_PROC; p++){
            msgrcv(msg_qids[p], &msg_buf, sizeof(msg_buf.body), p+100, IPC_NOWAIT);
            if(strlen(msg_buf.body.text)){
                printf("Gerant(): message from P0%d, %s\n", p, msg_buf.body.text);
                //reset
                msg_buf.body.text[0] = '\0';
                msg_buf.body.type = false;
            }
        }

        while(satisfy(Dispo, Dem, Alloc, Stat) == ALC_SUCCESS){}
        // count wait time
        for(int p=1; p <= NUM_PROC; p++){
            if(Stat[p].bloq)
                Stat[p].tmpAtt++;
        }
    }
    shmdt(tampon);
}

int prog_init() {
    // semaphores

    sem_unlink(SEM_NVIDE_FNAME);
    sem_unlink(SEM_MUTEX_FNAME);

    nvide = sem_open(SEM_NVIDE_FNAME, O_CREAT, 0666, 3);
    mutex = sem_open(SEM_MUTEX_FNAME, O_CREAT, 0666, 1);

    if (nvide == SEM_FAILED || mutex == SEM_FAILED) {
        printf("sem_open(), %s\n", strerror(errno));
        return 1;
    }

    // message queues
    printf("creating message queues:\n");
    for (int _ = 0; _ <= NUM_PROC; _++) {
        msg_qids[_] = msgget((key_t)(60 + _), 0666 | IPC_CREAT);
        struct msqid_ds buf;
        msgctl(msg_qids[_], IPC_STAT, &buf);
        buf.msg_qbytes = 5 * sizeof(mesg_buffer);
        msgctl(msg_qids[_], IPC_SET, &buf);
        printf("\tProcess %d, msg_qid: %d\n", _, msg_qids[_]);
    }

    // shared memory
    tampon_id = shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666 | IPC_CREAT);
    return 0;
}

int prog_destroy() {
    sem_close(nvide);
    sem_close(mutex);

    sem_unlink(SEM_NVIDE_FNAME);
    sem_unlink(SEM_MUTEX_FNAME);

    printf("destroying message queues:\n");
    for (int _ = 0; _ <= NUM_PROC; _++) {
        msgctl(msg_qids[_], IPC_RMID, NULL);
        printf("\t%d. msg_qid %d destroyed.\n", _, msg_qids[_]);
    }

    shmctl(tampon_id, IPC_RMID, NULL);
    return 0;
}

int main(int argc, char* argv[]) {
    if (prog_init())
        printf("prog_init();\n");

    tampon = (tamp_t*)shmat(tampon_id, NULL, 0);
    if (!tampon) {
        printf("\tERR: attaching shared mem block failed for main.\n");
        return EXIT_FAILURE;
    }
    tampon->cpt = 0;

    if (argc == 2 && !strcmp(argv[1], "clean")) {
        if (prog_destroy())
            printf("prog_destroy();\n");
        return 0;
    }

    if (fork() == 0) {
        Gerant();
        printf("Gerant closed\n");
        exit(0);
    }
    sleep(1);
    for (int i = 1; i <= NUM_PROC; i++) {
        if (fork() == 0) {
            Calcul(i);
            printf("Calcul %d closed\n", i);
            exit(0);
        }
    }

    for (int i = 0; i <= NUM_PROC; i++) {
        wait(NULL);
    }

    shmdt(tampon);
    if (prog_destroy())
        printf("prog_destroy();\n");

    printf("\n main closed.\n");
    return EXIT_SUCCESS;
}
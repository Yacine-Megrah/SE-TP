#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
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

#pragma region STRUCTS
typedef struct
{
    int pid, type, s1, s2, s3;
} req_t;

typedef struct
{
    bool bloq;
    int tmpAtt;
} procInf_t;

typedef struct
{
    int s1, s2, s3;
} alloc_t;

typedef struct
{
    req_t T[TAMP_SIZE];
    int cpt;
} tamp_t;

typedef struct
{
    bool type;
    char text[MSG_STR_SIZE];
} message_t;

typedef struct
{
    long type;
    message_t body;
} mesg_buffer;
#pragma endregion

sem_t mutex, nvide;
int tampon_id;
tamp_t *tampon;

int msg_qids[NUM_PROC];
const char msg_lib_format[] = "liber(%d, %d, %d)";
const char msg_lib_success[] = "resource liberated.";
const char msg_lib_fail[] = "failed to liberate.";

void print_status(int Dispo[], req_t Dem[], procInf_t Stat[], alloc_t Alloc[])
{
    int p;
    printf("  Dispo:\n");
    printf("| %8d | %8d | %8d |\n", Dispo[0], Dispo[1], Dispo[2]);
    printf("----------------------------------\n");
    printf("  Demandes:\n| %-8s | %-8s | %-8s | %-8s | %-8s |\n", "pid", "type", "size1", "size2", "size3");
    for (p = 0; p < NUM_PROC; p++)
    {
        printf("| %8d | %8d | %8d | %8d | %8d |\n", p+1, Dem[p].type, Dem[p].s1, Dem[p].s2, Dem[p].s3);
    }
    printf("-----------------------------------------------------\n");
    printf("  Status:\n| %-8s | %-8s | %-8s |\n", "pid", "blocked", "tmp_att");
    for (p = 0; p < NUM_PROC; p++)
    {
        printf("| %8d | %-8s | %8d |\n", p+1, Stat[p].bloq ? "true" : "false", Stat[p].tmpAtt);
    }
    printf("---------------------------------\n");
    printf("  Allocations:\n| %-8s | %-8s | %-8s | %-8s |\n", "pid", "size1", "size2", "size3");
    for (p = 0; p < NUM_PROC; p++)
    {
        printf("| %8d | %8d | %8d | %8d |\n", p+1, Alloc[p].s1, Alloc[p].s2, Alloc[p].s3);
    }
    printf("-----------------------------------------------\n");
}

ssize_t message_receive(message_t *msg, int src_pid)
{
    mesg_buffer mesg_buf;
    ssize_t ret = msgrcv(msg_qids[src_pid], &mesg_buf, sizeof(message_t), (long)(src_pid + 100), 0);
    if(ret)memcpy(msg, &mesg_buf.body, sizeof(message_t));
    return ret;
}

int message_send(message_t *msg, int src_pid)
{
    mesg_buffer msg_buf;
    memcpy(&msg_buf.body, msg, sizeof(message_t));
    msg_buf.type = (long)(src_pid + 100);
    msgsnd(msg_qids[src_pid], &msg_buf, sizeof(message_t), 0);
    return 0;
}

// append
void req_send(req_t *request)
{
    sem_wait(&nvide);
        sem_wait(&mutex);
            //memcpy(&tampon->T[tampon->cpt], request, sizeof(req_t));
            tampon->cpt++;
            printf("\t cpt = %d .\n", tampon->cpt);
        sem_post(&mutex);
}

// pull
bool req_receive(req_t *request)
{
    sem_wait(&mutex);
    if(tampon->cpt > 0){
        //memcpy(request, &tampon->T[0], sizeof(req_t));
        //for (int _ = 0; _ < tampon->cpt-1; _++)
        //    memcpy(&tampon->T[_], &tampon->T[_ + 1], sizeof(req_t));
        tampon->cpt--;
        sem_post(&nvide);
        printf("\t cpt = %d .\n", tampon->cpt);
        sem_post(&mutex);
        return true;
    }else{
        sem_post(&mutex);
        return false;
    }
    
}

void Calcul(int pid)
{
    // find message queue.
    if (msg_qids[pid] != msgget((key_t)(60 + pid), 0666))
    {
        printf("\tERR: missing message queue for Calcul %d\n", pid);
        return;
    }
    // find shared mem block
    // find & attach shared mem block
    if (tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666))
    {
        printf("\tERR: Couldn't find shared mem block for Calcul %d.\n", pid);
        return;
    }
    tampon = (tamp_t *)shmat(tampon_id, NULL, 0);
    if (!tampon)
    {
        printf("\tERR: attaching shared mem block failed for Calcul %d.\n", pid);
        return;
    }

    // Instruction file
    char filename[25];
    sprintf(filename, "Set%d.ins", pid);
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("\tERR: Couldn't find instruction file %d.\n", pid);
    }

    req_t request = {.pid = pid, .type = 1, .s1 = 0, .s2 = 0, .s3 = 0};
    message_t message = {.type = false , .text = "\0"};

    bool running = true;
    while(running){
        fscanf(f, "%d %d %d %d", &request.type, &request.s1, &request.s2, &request.s3);
        printf("__[Calcul %d]::(%d, %d, %d, %d)__\n", request.pid,  request.type, request.s1, request.s2, request.s3);
        switch(request.type){
            case 1:
                sleep(0.1f);
                break;
            case 2:
                req_send(&request);
                // src_pid 0 = Gerant
                //while(message_receive(&message, 0) <= 0 || message.type == false || strcmp(message.text, msg_lib_success));
                // reset
                strcpy(message.text,"\0"); message.type = false;
                break;
            case 3:
                sprintf(message.text, msg_lib_format, request.s1, request.s2, request.s3);
                message_send(&message, pid); 
                //while(message_receive(&message, 0) <= 0 || message.type == false || strcmp(message.text, msg_lib_success));
                // reset
                strcpy(message.text,"\0"); message.type = false;
                break;
            case 4:
                req_send(&request);
                running = false;
                break;
            default: break;
        }
        sleep(0.5f);
    }

    shmdt(tampon);
    printf("Calcul %d closed\n", pid);
}

void Gerant()
{
    // find message queue.
    if (msg_qids[0] != msgget((key_t)(60), 0666))
    {
        printf("\tERR: missing message queue for Gerant\n");
        return;
    }
    // find & attach shared mem block
    if (tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666))
    {
        printf("\tERR: Couldn't find shared mem block for Gerant.\n");
        return;
    }
    tampon = (tamp_t *)shmat(tampon_id, NULL, 0);
    if (!tampon)
    {
        printf("\tERR: attaching shared mem block failed for Gerant.\n");
        return;
    }
    int NbProc = NUM_PROC;

    req_t request = {.pid = 0, .type = 1, .s1 = 0, .s2 = 0, .s3 = 0};
    mesg_buffer msg;
    int Dispo[3] = {100, 100, 100};
    req_t Dem[NUM_PROC] = {request, request, request, request, request};
    alloc_t Alloc[NUM_PROC] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    procInf_t Stat[NUM_PROC] = {{false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}};

    bool found;
    while (NbProc)
    {
        req_receive(&request);
        printf("[_PID: %d, request: %d_]\n", request.pid, request.type);
        if(request.type == 4)NbProc--;
        // reset
        request.pid = 0; request.type = 1; request.s1 = 0; request.s2 = 0; request.s3 = 0;
        
        for (int _ = 1; _ <= NUM_PROC; _++)
        {
            if (msgrcv(msg_qids[_], &msg, sizeof(message_t), (long)(100 + _), 0))
                printf("\tPID %d: message \'%s\'\n", _, msg.body.text);
        }
    }

    shmdt(tampon);
    printf("Gerant closed\n");
}

int prog_init()
{
    // semaphores
    if(sem_init(&mutex, 0, 1) || sem_init(&nvide, 0, TAMP_SIZE)){
        printf("sem_init(), %s\n", strerror(errno));
        return 1;
    }

    // message queues
    printf("creating message queues:\n");
    for (int _ = 0; _ <= NUM_PROC; _++)
    {
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

int prog_destroy()
{
    sem_destroy(&nvide);
    sem_destroy(&mutex);

    printf("destroying message queues:\n");
    for (int _ = 0; _ <= NUM_PROC; _++)
    {
        msgctl(msg_qids[_], IPC_RMID, NULL);
        printf("\tmsg_qid %d destroyed.\n", _, msg_qids[_]);
    }

    shmctl(tampon_id, IPC_RMID, NULL);
    return 0;
}

int main(int argc, char *argv[])
{   
    if (prog_init())
        printf("prog_init();\n");
    
    tampon = (tamp_t *)shmat(tampon_id, NULL, 0);
    if (!tampon)
    {
        printf("\tERR: attaching shared mem block failed for main.\n");
        return EXIT_FAILURE;
    }
    tampon->cpt = 0;

    if (argc == 2 && !strcmp(argv[1], "clean")){
        if (prog_destroy())
        printf("prog_destroy();\n");
        return 0;
    }

    if (fork() == 0)
    {
        Gerant();
        exit(0);
    }
    sleep(1);
    for (int i = 1; i <= NUM_PROC; i++)
    {
        if (fork() == 0)
        {
            Calcul(i);
            exit(0);
        }
    }

    for (int i = 0; i <= NUM_PROC; i++)
    {
        wait(NULL);
    }

    if (prog_destroy())
        printf("prog_destroy();\n");

    printf("\n main closed.\n");
    return EXIT_SUCCESS;
}
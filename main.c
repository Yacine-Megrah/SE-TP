#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define NUM_PROC 5
#define TAMP_SIZE 3
#define TAMP_KEY 16

#pragma region STRUCTS
typedef struct {
    int pid, type, s1, s2, s3;
}req_t;

typedef struct {
    req_t T[TAMP_SIZE];
    int cpt;
} tamp_t;
#pragma endregion

sem_t mutex1, mutex, nvide;
int tampon_id;
tamp_t *tampon;

int msg_qids[NUM_PROC];

void req_receive(req_t *request){
        sem_wait(&mutex);
            memcpy(request, &tampon->T[0], sizeof(req_t));
            for(int _ = 0; _ < tampon->cpt; _++)
                memcpy(&tampon->T[_], &tampon->T[_+1], sizeof(req_t));
            sem_wait(&mutex1);    
                tampon->cpt--;
            sem_post(&mutex1);    
        sem_post(&mutex);
    sem_post(&nvide);
}

void req_send(req_t *request){
    sem_wait(&nvide);
        sem_wait(&mutex);
            memcpy(&tampon->T[tampon->cpt], request, sizeof(req_t));
            sem_wait(&mutex1);
                tampon->cpt++;
                printf("\ttampon->cpt: %d\n", tampon->cpt);
            sem_post(&mutex1);
        sem_post(&mutex);
}

void Calcul(int pid){
    // find message queue.
    if(msg_qids[pid] != msgget((key_t)(60+pid), 0666)){
        printf("\tERR: missing message queue for Calcul %d\n", pid);
        return;
    }
    // find shared mem block
    // find & attach shared mem block
    if(tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666)){
        printf("\tERR: Couldn't find shared mem block for Calcul %d.\n", pid);
        return;
    }
    tampon = (tamp_t*)shmat(tampon_id, NULL, 0);

    if(pid == 4){
        req_t request = {pid, 1, 0, 0, 0};
        req_send(&request);
        printf("\tCalcul 4 sent request of type %d\n", tampon->T[0].type);
    }
    
    shmdt(tampon);
    printf("Calcul %d closed\n", pid);
}

void Gerant(){
    // find message queue.
    if(msg_qids[0] != msgget((key_t)(60), 0666)){
        printf("\tERR: missing message queue for Gerant\n");
        return;
    }
    // find & attach shared mem block
    if(tampon_id != shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666)){
        printf("\tERR: Couldn't find shared mem block for Gerant.\n");
        return;
    }
    tampon = (tamp_t*)shmat(tampon_id, NULL, 0);

    req_t request;

    int trouve = 0;
    while(trouve==0){
        sem_wait(&mutex1);
            if(0 < tampon->cpt){
                printf("request found\n");
                trouve = 1;
                sem_post(&mutex1);
                req_receive(&request);
                printf("\trequest received from %d.\n", request.pid);
            }else sem_post(&mutex1);
    }

    shmdt(tampon);
    printf("Gerant closed\n");
}

int prog_init(){
    // semaphores
    sem_init(&mutex1, 1, 1);
    sem_init(&mutex , 1, 1);
    sem_init(&nvide , 1, TAMP_SIZE);
    // message queues
    printf("creating message queues:\n");
    for(int _ = 0; _ <= NUM_PROC ; _++){
        msg_qids[_] = msgget((key_t)(60+_), 0666 | IPC_CREAT);
        printf("\tProcess %d, msg_qid: %d\n", _, msg_qids[_]);
    }
    // shared memory
    tampon_id = shmget((key_t)TAMP_KEY, sizeof(tamp_t), 0666 | IPC_CREAT);
    return 0;
}

int prog_destroy(){
    sem_destroy(&nvide);
    sem_destroy(&mutex);
    sem_destroy(&mutex1);

    printf("destroying message queues:\n");
    for(int _ = 0; _ <= NUM_PROC ; _++){
        msgctl(msg_qids[_], IPC_RMID, NULL);
        printf("\tmsg_qid %d destroyed.\n", _, msg_qids[_]);
    }

    shmctl(tampon_id, IPC_RMID, NULL);
    return 0;
}

int main(){
    if(prog_init())printf("prog_init();\n");

    if(fork() == 0){
        Gerant();
        exit(0);
    }

    for(int i = 1; i<= NUM_PROC; i++){ 
        if(fork() == 0){
            Calcul(i);
            exit(0);
        }
    }

    for(int i = 0; i <= NUM_PROC; i++){ 
        wait(NULL);
    }

    if(prog_destroy())printf("prog_destroy();\n");

    printf("\n main closed.\n");
    return EXIT_SUCCESS;
}
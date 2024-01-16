#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>

#define SHM_SIZE  sizeof(tampon_t)
#define TAMP_SIZE 3

typedef struct {
    int counter;
} tampon_t;

sem_t mutex;
sem_t nvide;

void producer(tampon_t *shared_memory) {
    while (1) {
        // Produce: increase the counter
        sem_wait(&nvide);
            sem_wait(&mutex);
                shared_memory->counter++;
                printf("Produced. Counter: %d\n", shared_memory->counter);
            sem_post(&mutex);
    }
}

void consumer(tampon_t *shared_memory) {
    while (1) {

        // Consume: decrease the counter
        sem_wait(&mutex);
            if(shared_memory->counter){
                shared_memory->counter--;
                printf("Consumed. Counter: %d\n", shared_memory->counter);
                sem_post(&nvide);
            }
        sem_post(&mutex);

        sleep(1);  // Simulate some work
    }
}

int main() {
    // Create shared memory
    key_t key = ftok("/tmp", 'A');
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    tampon_t *shared_memory = (tampon_t *)shmat(shmid, NULL, 0);
    shared_memory->counter = 0;

    // Initialize semaphores
    if(sem_init(&mutex, 1, 1)){
        printf("%s", strerror(errno));
        system("pause");
    }
    sem_init(&nvide, 1, 3);

    // Fork producer and consumer processes
    pid_t producer_pid = fork();
    if (producer_pid == 0) {
        producer(shared_memory);
        exit(0);
    }

    pid_t consumer_pid = fork();
    if (consumer_pid == 0) {
        consumer(shared_memory);
        exit(0);
    }

    // Wait for child processes to finish
    wait(NULL);
    wait(NULL);

    // Cleanup: Destroy semaphores and detach shared memory
    sem_destroy(&mutex);
    sem_destroy(&nvide);
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

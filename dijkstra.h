#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#define CLE 123
int semid ;
struct sembuf operation[1] ;
char *path = "nom_de_fichier_existant" ;

int sem_create(key_t cle,int initval){
    union semun {
    int val ;
    struct semid_ds *buf ;
    ushort *array ;
    } arg_ctl ;
    semid = semget(ftok("dijkstra.h",cle),1,IPC_CREAT|IPC_EXCL|0666) ;
    if (semid == -1) {
    semid = semget(ftok("dijkstra.h",cle),1, 0666) ;
    if (semid == -1) {
    perror("Erreur semget()") ;
    exit(1) ;
    }
    }
    arg_ctl.val = initval ;
    if (semctl(semid,0,SETVAL,arg_ctl) == -1) {
    perror("Erreur initialisation semaphore") ;
    exit(1) ;
    }
    return(semid) ;
}
void P(int semid){
    struct sembuf sempar ;
    sempar.sem_num = 0 ;
    sempar.sem_op = -1 ;
    sempar.sem_flg = SEM_UNDO ;
    if (semop(semid, &sempar, 1) == -1)
    perror("Erreur operation P") ;
}
void V(int semid){
    struct sembuf sempar ;
    sempar.sem_num = 0 ;
    sempar.sem_op = 1 ;
    sempar.sem_flg = SEM_UNDO ;
    if (semop(semid, &sempar, 1) == -1)
    perror("Erreur operation V") ;
}
void sem_delete(int semid){
    if (semctl(semid,0,IPC_RMID,0) == -1)
    perror("Erreur dans destruction semaphore") ;
}
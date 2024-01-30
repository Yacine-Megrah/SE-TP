#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>

#include "dijkstra.h"

#define MAX_MESSAGE_SIZE 100

#define SHM_Requete_SIZE (sizeof(Requete) * 3)
#define KEYRequete 1234

#define SHM_SIZE_RequeteData (sizeof(RequeteData))
#define KEYRequeteData 12345                     

typedef struct{
	long mtype;
	char mtext[MAX_MESSAGE_SIZE];
} msgbuf;

typedef struct{
	int id;
    int type;
    int Dem1;
    int Dem2;
    int Dem3;
} Requete;

Requete *TRequetes;

typedef struct {
    int rqstt;
    int rqstq;
    int NumRqt;
} RequeteData;

RequeteData *Rqtdata;

int nv,np,mutex,mutex2;
int Flibre[5],Freponse;

typedef struct{
	bool blocked;
	int vict;
	time_t time;
}Infos;

typedef struct{
	int Dem1,Dem2,Dem3;
}Demande;

typedef struct{
	int N1,N2,N3;
}Allocation;

void Gerant() {
	int N1=30,N2=30,N3=30;
    Demande Dem[5];
	Infos Info[5];
    Allocation Alloc[5];
	msgbuf msg;
    float a=.5,b=.5;
    int cpt = 0;
    Requete curReqt;
	int counter=0;
	int minQ , minQid , Q;
	time_t nowTime;
	bool found;
    printf("Gerant");
	for (int i = 0; i < 5; i++) Info[i].blocked = false;
    do{
		found = false;
		P(np);
		if (Rqtdata->NumRqt>0)
		{
			curReqt.id = TRequetes[Rqtdata->rqstt].id;
			curReqt.type = TRequetes[Rqtdata->rqstt].type;
			curReqt.Dem1 = TRequetes[Rqtdata->rqstt].Dem1;
			curReqt.Dem2 = TRequetes[Rqtdata->rqstt].Dem2;
			curReqt.Dem3 = TRequetes[Rqtdata->rqstt].Dem3;
			P(mutex2);
			Rqtdata->NumRqt--;
			V(mutex2);
			Rqtdata->rqstt = (Rqtdata->rqstt+1)%3;
			found = true;
		}
		V(nv);

		if (!found) {
			while(counter<5){
				if((msgrcv(Flibre[counter],&msg,MAX_MESSAGE_SIZE,1,IPC_NOWAIT|MSG_NOERROR)) != -1)
				{
					printf("texte du message recu: %s\n",msg.mtext);
					found = true;
				}
				counter = (counter+1)%5;
				if (found) break;
			}
			sscanf(msg.mtext, "%d %d %d %d %d", &curReqt.type, &curReqt.id, &curReqt.Dem1, &curReqt.Dem2, &curReqt.Dem3);
		}
		if (found){
			switch(curReqt.type){
			case 2:
				if (curReqt.Dem1 < N1 && curReqt.Dem2 < N2 && curReqt.Dem3 < N3) {
					N1 -= curReqt.Dem1;
					N2 -= curReqt.Dem2;
					N3 -= curReqt.Dem3;
					// change alloc 
					Alloc[curReqt.id].N1 += curReqt.Dem1;
					Alloc[curReqt.id].N2 += curReqt.Dem2;
					Alloc[curReqt.id].N3 += curReqt.Dem3;
					// unblock Dem 
					Info[curReqt.id].blocked = false;
					// 1 msg in respond to Pi
					msg.mtype = curReqt.id;
					strcpy(msg.mtext,"1");
					if(msgsnd(Freponse,&msg,strlen(msg.mtext),IPC_NOWAIT) == -1)
					{
						perror("impossible d'envoyer le msg") ;
						exit(-1) ;
					}
					printf("msg de type %ld envoye a %d",msg.mtype,Flibre[curReqt.id]) ;
					printf(" texte du msg: %s\n",msg.mtext) ;
				}else{
					// check if you have blocked processuce in Dem how can satiffie this one 
					minQ = 0;
					minQid = -1;
					nowTime = time(NULL);
					for(int i=0;i<5;i++){
						if(Info[i].blocked){
							if (Alloc[i].N1+N1 > curReqt.Dem1 && Alloc[i].N2+N2 > curReqt.Dem2 && Alloc[i].N3+N3 > curReqt.Dem3) {
								Q =  a*(nowTime-Info[i].time)+b*Info[i].vict;
								if(minQ>Q){
									minQ = Q;
									minQid = i;
								}
							}
						}
					}
					if(minQid!=-1){
						N1 -= curReqt.Dem1;
						N2 -= curReqt.Dem2;
						N3 -= curReqt.Dem3;
						if(N1<0){
							Alloc[minQid].N1 +=N1;
							Dem[minQid].Dem1 -=N1;
							N1=0;
						}
						if(N2<0){
							Alloc[minQid].N2 +=N2;
							Dem[minQid].Dem2 -=N2;
							N2=0;
						}
						if(N3<0){
							Alloc[minQid].N3 +=N3;
							Dem[minQid].Dem3 -=N3;
							N3=0;
						}
						Info[minQid].vict++;
						// change alloc
						Alloc[curReqt.id].N1 += curReqt.Dem1;
						Alloc[curReqt.id].N2 += curReqt.Dem2;
						Alloc[curReqt.id].N3 += curReqt.Dem3;
						// unblock Dem
						Info[curReqt.id].blocked = false;
						// 1 msg in respond to Pi
						msg.mtype = curReqt.id;
						strcpy(msg.mtext,"1");
						if(msgsnd(Freponse,&msg,strlen(msg.mtext),IPC_NOWAIT) == -1)
						{
							perror("impossible d'envoyer le msg") ;
							exit(-1) ;
						}
						printf("msg de type %ld envoye a %d",msg.mtype,Freponse) ;
						printf(" texte du msg: %s\n",msg.mtext) ;
					}
					else {
						// 0 msg in respond to Pi
						msg.mtype = curReqt.id;
						strcpy(msg.mtext,"0");
						if(msgsnd(Freponse,&msg,strlen(msg.mtext),IPC_NOWAIT) == -1)
						{
							perror("impossible d'envoyer le msg") ;
							exit(-1) ;
						}
						printf("msg de type %ld envoye a %d",msg.mtype,Freponse) ;
						printf(" texte du msg: %s\n",msg.mtext) ;
						// add demand to Dem
						Info[curReqt.id].blocked = true;
						Dem[curReqt.id].Dem1 = curReqt.Dem1;
						Dem[curReqt.id].Dem2 = curReqt.Dem2;
						Dem[curReqt.id].Dem3 = curReqt.Dem3;
						Info[curReqt.id].vict = 0;
						Info[curReqt.id].time = time(NULL);
					}
				}
				break;
			case 3:
				msg.mtype = 2;
				// cahnge alloc 
				Alloc[curReqt.id].N1 -= curReqt.Dem1;
				Alloc[curReqt.id].N2 -= curReqt.Dem2;
				Alloc[curReqt.id].N3 -= curReqt.Dem3;
				// add in N
				N1 += curReqt.Dem1;
				N2 += curReqt.Dem2;
				N3 += curReqt.Dem3;
				// check if any porecessuce can be satisfied in dem
				for(int i=0 ;i<5;i++){
					if(Info[i].blocked){
						if (Dem[i].Dem1 < N1 && Dem[i].Dem2 < N2 && Dem[i].Dem3 < N3) {
							N1 -= Dem[i].Dem1;
							N2 -= Dem[i].Dem2;
							N3 -= Dem[i].Dem3;
							// change alloc
							Alloc[curReqt.id].N1 += Dem[i].Dem1;
							Alloc[curReqt.id].N2 += Dem[i].Dem2;
							Alloc[curReqt.id].N3 += Dem[i].Dem3;
							// unblock Dem
							Info[i].blocked = false;
							// 1 msg in respond to curReqt.id
							msg.mtype = curReqt.id;
							strcpy(msg.mtext,"1");
							if(msgsnd(Freponse,&msg,strlen(msg.mtext),IPC_NOWAIT) == -1)
							{
								perror("impossible d'envoyer le msg") ;
								exit(-1) ;
							}
							printf("msg de type %ld envoye a %d",msg.mtype,Freponse) ;
							printf(" texte du msg: %s\n",msg.mtext) ;
						}
					}
				}
				printf("Libre\n");
				break;
			case 4:
				cpt++;
				printf("end\n");
				break;
			}
		}
    }while (cpt<5);
	if (shmdt(TRequetes) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	exit(0);
}

void Calcule(int id) {
	FILE *F;
    int inst[5];
    char Name[20],Strinst[20];
	msgbuf msg;
    sprintf(Name, "P%d.txt", id);
    F = fopen(Name, "r");
    printf("%s \n", Name);
	//shmget
    while (fscanf(F, "%d %d %d %d",&inst[0],&inst[1],&inst[2],&inst[3]) != EOF) {
        switch(inst[0]){
        	case 1:
        	    printf("simple inst P%d\n",id);
        		sleep(1);
        		break;
        	case 2:
        		printf("demende P%d\n",id);
        		
        		P(nv);
        		P(mutex);
        		// add rqst
				TRequetes[Rqtdata->rqstq].id=id;
				TRequetes[Rqtdata->rqstq].type=2;
				TRequetes[Rqtdata->rqstq].Dem1=inst[1];
				TRequetes[Rqtdata->rqstq].Dem2=inst[2];
				TRequetes[Rqtdata->rqstq].Dem3=inst[3];
        		Rqtdata->NumRqt++;
				Rqtdata->rqstq = (Rqtdata->rqstq+1)%3;
        		//
        		V(mutex); 
        		V(np);
				do{
					if((msgrcv(Freponse,&msg,MAX_MESSAGE_SIZE,id,0)) != -1)
					{
						printf("texte du message recu: %s\n",msg.mtext);
					}
				}while (strcmp(msg.mtext,"0"));
        		break;
        	case 3:
        		// add in file msg libre de i
				sprintf(Strinst, "3 %d %d %d %d",id,inst[1],inst[2],inst[3]);
				msg.mtype = 1;
        		strcpy(msg.mtext, Strinst);
				if(msgsnd(Flibre[id],&msg,strlen(msg.mtext),0) == -1)
				{
					perror("impossible d'envoyer le msg") ;
					exit(-1) ;
				}
				printf("msg de type %ld envoye a %d",msg.mtype,Flibre[id]) ;
				printf(" texte du msg: %s\n",msg.mtext) ;
        		printf("liberation P%d\n",id);
        		break;
        	case 4:
        		P(nv);
        		P(mutex);
        		// add rqst
        		sprintf(Strinst, "4 %d 0 0 0",id);
        		sscanf(Strinst, "%d %d %d %d %d", &TRequetes[Rqtdata->rqstq].id, &TRequetes[Rqtdata->rqstq].type, &TRequetes[Rqtdata->rqstq].Dem1, &TRequetes[Rqtdata->rqstq].Dem2, &TRequetes[Rqtdata->rqstq].Dem3);
        		P(mutex2);
				Rqtdata->NumRqt++;
				V(mutex2);
				Rqtdata->rqstq = (Rqtdata->rqstq+1)%3;
        		//
        		V(mutex); 
        		V(np);
        		printf("end P%d\n",id);
        		break;
        }
    }
    fclose(F);
	if (shmdt(TRequetes) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
    exit(0);
}

int main() {
    int i,id;
    int status;
	char path[50];
	int shmid,shmidata;

	// init semaphore
	nv = sem_create(1,3) ;
	np = sem_create(2,0) ;
	mutex = sem_create(3,1) ;
	mutex2 = sem_create(4,1) ;
	// creation des files de message
	for (int i = 0; i < 5; i++)
	{
		if (( Flibre[i] = msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL)) == -1 ) {
			perror ("Erreur msgget()") ;
			exit(1) ;
		}
	}
	if (( Freponse = msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL)) == -1 ) {
		perror ("Erreur msgget()") ;
		exit(1) ;
	}	
	// Create tempon
    if ((shmid = shmget(KEYRequete, SHM_Requete_SIZE, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    if ((TRequetes = (Requete *)shmat(shmid, NULL, 0)) == (Requete *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    // Initialize tempon
    for (int i = 0; i < 3; ++i) {
        TRequetes[i].id = i + 1;
        TRequetes[i].type = 0;
        TRequetes[i].Dem1 = 0;
        TRequetes[i].Dem2 = 0;
        TRequetes[i].Dem3 = 0;
    }

    // Create a shared memory segment
    if ((shmidata = shmget(KEYRequeteData, SHM_SIZE_RequeteData, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the address space of the process
    if ((Rqtdata = (RequeteData *)shmat(shmidata, NULL, 0)) == (RequeteData *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Initialize the shared memory
    Rqtdata->rqstt = 0;
    Rqtdata->rqstq = 0;
    Rqtdata->NumRqt = 0;
    id = fork();
    if(id==0)Gerant();

    for (i = 1; i <= 5; i++) {
		printf("creation fils %d\n",i);
		id = fork();
		if(id==0)Calcule(i);
    }

    for (i = 0; i < 6; i++) wait(&status);
	sem_delete(nv);
	sem_delete(np);
	sem_delete(mutex);
	sem_delete(mutex2);
	// detach remove tempon
	if (shmdt(TRequetes) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		perror("shmctl");
		exit(EXIT_FAILURE);
	}
	if (shmdt(Rqtdata) == -1) {
		perror("shmdt");
		exit(EXIT_FAILURE);
	}
	if (shmctl(shmidata, IPC_RMID, NULL) == -1) {
		perror("shmctl");
		exit(EXIT_FAILURE);
	}
    return 0;
}

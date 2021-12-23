# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <pthread.h>
# include <signal.h>
# include <sys/types.h>
# include <unistd.h>
# include <string.h>
# include <stdbool.h>
# include <limits.h>

/* Declaraciones globales */

/*Semaforos y variables condicion*/
pthread_mutex_t mutexLog;
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexAscensor;
pthread_mutex_t mutexMaquinas;

pthread_cond_t condAscensor;
pthread_cond_t condClientes;

int contadorClientes;
int contadorAscensor;
int ascensorFuncionando;			// 0 no funciona 1, si

/* Lista de 20 clientes */
struct cliente{
	int id;
	int atendido;					//0 no atendido, 1 atendiendo , 2 atendido
	int tipo;						//0 no vip, 1 vip
	int ascensor;
	pthread_t hiloCliente
}clientes[20];

/* Lista de recepcionistas */
pthread_t recepcionistas[3];


/* Lista de MaquinasCheckIn */

pthread_t maquinasCheck[5];

/* Fichero de log */
FILE *logFile;


/* Declaracion de las funciones */

void nuevoCliente(int sig);



int main(int argc, char* argv[]){
	struct sigaction ss

	ss.sa_handler= nuevoCliente;

 		if (-1==sigaction(SIGUSR1,&ss,NULL)){
 			perror("Fallo sigaction SIGUSR1");
 			return 1;
 		}

 		if (-1==sigaction(SIGUSR2,&ss,NULL)){
 			perror("Fallo sigaction SIGUSR2");
 			return 1;
 		}

 		ss.sa_handler = terminar;

 		if (-1==sigaction(SIGINT,&ss,NULL)){
 			perror("Fallo sigaction SIGINT");
 			return 1;
 		}

 	/* Inicializar recursos */

 	pthread_mutex_int(&mutexLog,NULL);
 	pthread_mutex_int(&mutexAscensor,NULL);
 	pthread_mutex_int(&mutexMaquinas,NULL);
 	pthread_mutex_int(&mutexColaClientes,NULL);

 	contadorClientes = 0;





}

void nuevoCliente(int sig){

}





void writeLogMessage(char *id, char *msg) { 
// Calculamos la hora actual
time_t now = time(0);
struct tm *tlocal = localtime(&now);
char stnow[25];
strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal); // Escribimos en el log
logFile = fopen(logFileName, "a");
fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
fclose(logFile);
}

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

#define maximoClientes 20

/*Semaforos y variables condicion*/
pthread_mutex_t mutexLog;
pthread_mutex_t mutexColaClientes;
pthread_mutex_t mutexAscensor;
pthread_mutex_t mutexMaquinas;

pthread_cond_t condAscensor;

int contadorIDClientes;
int contadorClientes;				// Va de a 20
int contadorAscensor;
int ascensorFuncionando;			// 0 no funciona 1, si

/* Lista de 20 clientes */
struct cliente{
	int id;
	int atendido;					//0 no atendido, 1 siendo atendido , 2 atendido
	int tipo;						//0 no vip, 1 vip
	int ascensor;
	int checking;
};
struct cliente clientes[20];

/* Lista de MaquinasCheckIn */

int maquinasCheck[5];

/* Fichero de log */
FILE *logFile;


/* Declaracion de las funciones */
void nuevoCliente(int sig);
void terminar(int sig);
void *accionesCliente(void *arg);
void *accionesRecepcionista(void *arg);


int main(int argc, char* argv[]){
	
	int vip = 1;
	int noVip = 0;
	int i;
	
	/*enmascaramos las señales,SIGUSR1 cliente no vip, SIGUSR2 cliente vip, SIGINT terminar el programa */
	signal(SIGUSR1, nuevoCliente);
	signal(SIGUSR2, nuevoCliente);
	signal(SIGINT, finalizar);

 	/* Inicializar recursos */
 	pthread_mutex_init(&mutexLog,NULL);
 	pthread_mutex_init(&mutexAscensor,NULL);
 	pthread_mutex_init(&mutexMaquinas,NULL);
 	pthread_mutex_init(&mutexColaClientes,NULL);

 	pthread_cond_init(&condAscensor,NULL);
 	
 	contadorClientes = -1;
 	contadorIDClientes = -1;
  	
  	/*Lista de clientes*/
  	for(i = 0; i < 20; i++){
  		clientes[i].id = 0;
  		clientes[i].atendido = 0;
  		clientes[i].tipo = 0;
  		clientes[i].ascensor = 0;
  		clientes[i].checking = 0;
  	}

  	/*Lista de recepcionistas*/
  	pthread_t recepcionista1, recepcionista2, recepcionista3;
  	

  	/*Máquinas de checkIn*/
  	for(i = 0; i < 5; i++){
  		maquinasCheck[i] = 0;
  	}

  	/*Fichero log*/
  	logFile = fopen("registroTiempos.log", "w");

  	/*Variables relativas al ascensor*/
	ascensorFuncionando = 0;
	contadorAscensor = 0;

	/*Crear 3 hilos recepcionistas*/
	pthread_create(&recepcionista1,NULL,accionesRecepcionista,&noVip);
	pthread_create(&recepcionista2,NULL,accionesRecepcionista,&noVip);
	pthread_create(&recepcionista3,NULL,accionesRecepcionista,&vip);

	while(1){
		
		
	}
	return 0;
}

void nuevoCliente(int sig){

	signal(SIGUSR1, nuevoCliente);
	signal(SIGUSR2, nuevoCliente);
	
	pthread_t hiloCliente;
	pthread_mutex_lock(&mutexColaClientes);

	if(contadorClientes < maximoClientes ){

		contadorClientes++;
		contadorIDClientes++;
		clientes[contadorClientes].id = contadorIDClientes;
		
		if(sig == SIGUSR1){
			clientes[contadorClientes].tipo = 0;			//Tipo no VIP
		}else if(sig == SIGUSR2){
			clientes[contadorClientes].tipo = 1;			//Tipo VIP
		}

		pthread_create(&hiloCliente,NULL,accionesCliente,&contadorIDClientes);
	}
	pthread_mutex_unlock(&mutexColaClientes);

}

void *accionesCliente(void *arg){

}
void *accionesRecepcionista(void *arg){

}
void terminar(int sig){
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);
}


/*void writeLogMessage(char *id, char *msg) { 
// Calculamos la hora actual
time_t now = time(0);
struct tm *tlocal = localtime(&now);
char stnow[25];
strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal); // Escribimos en el log
logFile = fopen(logFileName, "a");
fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
fclose(logFile);
}*/

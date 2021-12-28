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
int calculaAleatorios(int min, int max);



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

int clienteMayorTiempoEsperando(int tipoRecepcionista){
	int posicion = -1;
	int contador = 0;

	//Busco clientes de mi tipo
	pthread_mutex_lock(&mutexColaClientes);
	while (posicion == -1 && contador < maximoClientes) {
		if(clientes[contador].tipo == tipoRecepcionista && clientes[contador].id!=0 && clientes[contador].atendido == 0){
			posicion = clientes[contador].id;
		}
		contador++;

}
	//Busco clientes que estén sin atender (me da igual el tipo)
	while (posicion == -1 && contador < maximoClientes) {
 		if(clientes[contador].id!=0 && clientes[contador].atendido == 0){
			posicion = clientes[contador].id;

		}
		contador++;
}
	pthread_mutex_unlock(&mutexColaClientes);

	return posicion;
}




void *accionesRecepcionista(void *arg){

    int tipoRecepcionista = *(int *) arg;
    int recepcionistaOcupado = 0; 			//Ocupado = 1, no ocupado = 0	
    int posicionCliente;
    int aleatorio = -1;
    int contadorClientesAtendidos = 0;
    


    while(fin != 2){

    posicionCliente = clienteMayorTiempoEsperando(tipoRecepcionista);

    if(recepcionistaOcupado == 0){

    	pthread_mutex_lock(&mutexColaClientes);
    	clientes[posicionCliente].atendido = 1;
    	pthread_mutex_unlock(&mutexColaClientes);

    	recepcionistaOcupado = 1;
    	aleatorio = calculaAleatorios(0,100);
    	if(aleatorio <=80){
    		printf("El cliente tiene todo en regla");
    		sleep((calculaAleatorios(1,4)));
    		pthread_mutex_lock(&mutexColaClientes);
    		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el paciente
    		pthread_mutex_unlock(&mutexColaClientes);

    	} else if(aleatorio<= 90){
      		printf("El cliente no se ha identificado correctamente");
     		sleep((calculaAleatorios(2,6)));
   			pthread_mutex_lock(&mutexColaClientes);
     		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el paciente
    		pthread_mutex_unlock(&mutexColaClientes);

    	}else{
       		printf("El cliente no tiene el pasaporte vacunal");   
       		sleep((calculaAleatorios(6,10)));	
       		pthread_mutex_lock(&mutexColaClientes);
    		clientes[posicionCliente].atendido = 5;		//el paciente se tiene que ir, metemos dentro de la funcion finalizar que si un paciente.atendido= 5, se acabe su hilo???
    		pthread_mutex_unlock(&mutexColaClientes);

    	}

    	contadorClientesAtendidos++;

    	if(contadorClientesAtendidos == 5 && tipoRecepcionista != 1){		//Preguntar, recepcionista tipo 0 = novip y tipo 1 = vip???
    		printf("El recepcionista va a descansar 5 segundos");
    		sleep(5);
    		contadorClientesAtendidos = 0;
    	}

    	recepcionistaOcupado = 0;
}
	sleep(1);

}
}
void terminar(int sig){
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);
}


int calculaAleatorios(int min, int max){
    return rand() % (max - min + 1) + min;
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

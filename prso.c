# include <stdio.h>
# include <stdlib.h>
# include <time.h>
# include <pthread.h>
# include <signal.h>
#include <sys/syscall.h>
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
int fin;

/* Lista de 20 clientes */
struct cliente{
	int id;
	int atendido;					//0 no atendido, 1 siendo atendido , 2 atendido
	int tipo;						//0 no vip, 1 vip
	int ascensor;
	int checking;
	pthread_t thCliente;
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
int clienteMayorTiempoEsperando(int tipoRecepcionista);
void eliminarCliente(int id);
int comprobarAtendido(int ID);
pid_t gettid(void);

int main(int argc, char* argv[]){
	
	int vip = 1;
	int noVip = 0;
	int i;
	
	/*enmascaramos las señales,SIGUSR1 cliente no vip, SIGUSR2 cliente vip, SIGINT terminar el programa */
	signal(SIGUSR1, nuevoCliente);
	signal(SIGUSR2, nuevoCliente);
	signal(SIGINT, terminar);

 	/* Inicializar recursos */
 	pthread_mutex_init(&mutexLog,NULL);
 	pthread_mutex_init(&mutexAscensor,NULL);
 	pthread_mutex_init(&mutexMaquinas,NULL);
 	pthread_mutex_init(&mutexColaClientes,NULL);

 	pthread_cond_init(&condAscensor,NULL);
 	
 	contadorClientes = 0;
 	contadorIDClientes = 0;
 	fin=0;
  	
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
		if(fin==1){
			printf("prueba fin =1\n");
		}
		
	}
	return 0;
}

void nuevoCliente(int sig){

	signal(SIGUSR1, nuevoCliente);
	signal(SIGUSR2, nuevoCliente);
	
	pthread_mutex_lock(&mutexColaClientes);
	
	if(contadorClientes < maximoClientes){
		
		contadorIDClientes++;
		clientes[contadorClientes].id = contadorIDClientes;
		
		if(sig == SIGUSR1){
			clientes[contadorClientes].tipo = 0;			//Tipo no VIP
		}else if(sig == SIGUSR2){
			clientes[contadorClientes].tipo = 1;			//Tipo VIP
		}

		pthread_create(&clientes[contadorClientes].thCliente,NULL,accionesCliente,&contadorClientes);
		contadorClientes++;
		
	}else{
		printf("El hotel Sokovia está lleno.\n");
	}
	pthread_mutex_unlock(&mutexColaClientes);

}

int comprobarAtendido(int id){
	int atendido;
    int i = 0;
    int posicion = -1;

    pthread_mutex_lock(&mutexColaClientes);
    while (posicion == -1)
    {
        if(clientes[i].id == id){
            posicion = i;
            atendido = clientes[posicion].atendido;
        }
        i++;
    }
    pthread_mutex_unlock(&mutexColaClientes);

    return atendido;
}

void *accionesCliente(void *arg){

	int posicion = *(int *) arg;
	int atendido;
	int aleatorio;
	//	1. Guardar en el log la hora de entrada.
	//	2. Guardar en el log el tipo de cliente.
	srand(gettid());

	aleatorio = calculaAleatorios(1,100);
	if(aleatorio <= 10){										//10% de los clientes decide ir automaticamente a las maquinas de checking
		printf("El cliente con ID %d decide ir automaticamente a las maquinas de checking\n",clientes[posicion].id);
	}else{

		while(1){
			atendido = comprobarAtendido(clientes[posicion].id);
			if(atendido == 0){		//PUNTO 4 90% restante se mantienen esperando						 
					aleatorio = calculaAleatorios(1,100);
					if(aleatorio <= 20){	//Se cansan de esperar y se van a las maquinas de checking
						printf("El cliente con ID %d se cansa de esperar y decide ir a las maquinas de checking\n",clientes[posicion].id);
					}else if(aleatorio > 20 &&(aleatorio <= 30)){	//Se cansa de esperar y abandona el hotel
							printf("El cliente con ID %d se ha cansado de esperar y abandona el hotel\n",clientes[posicion].id);
							eliminarCliente(clientes[posicion].id);
							pthread_exit(NULL);
					}else{
						aleatorio = calculaAleatorios(1,100);
						if(aleatorio <=5){		//Se va al baño y abanadona el hotel
							printf("El cliente con ID %d se ha ido al baño y abandona el hotel\n",clientes[posicion].id);
							eliminarCliente(clientes[posicion].id);
							pthread_exit(NULL);
						}
					}
					
					sleep(3);	//Si no ha sido atendido duerme 3 segundos y vuelve a hacer todas las comprobaciones.
				
			}else if(atendido == 1){		//PUNTO 5
				sleep(2);
			}else if(atendido == 2){		//PUNTO 6
				aleatorio = calculaAleatorios(1,100);
				if(aleatorio <=30){			// El cliente espera al ascensor
					pthread_mutex_lock(&mutexColaClientes);
					clientes[posicion].ascensor = 1;
					pthread_mutex_unlock(&mutexColaClientes);
					printf("El paciente con ID %d está preparado para coger el ascensor\n",clientes[posicion].id);
					if(fin != 1){
						while(ascensorFuncionando == 1 &&(contadorAscensor == 6)){			//Mientras el ascensor esté lleno o este funcionando se queda esperando.
							sleep(3);
						}
					}else{
						printf("El cliente con ID %d  no espera al ascensor porque se deja de atender a clientes.\n",clientes[posicion].id);
						eliminarCliente(clientes[posicion].id);
						pthread_exit(NULL);			
					}			
				}else{
					printf("El cliente con ID %d ha sido atendido, no espera al ascensor y se va a su habitación\n",clientes[posicion].id);
					eliminarCliente(clientes[posicion].id);
					pthread_exit(NULL);					
				}

			}

		}
	}
}

int clienteMayorTiempoEsperando(int tipoRecepcionista){
	int posicion = -1;
	int i = 0;

	//Primero se buscan clientes del mismo tipo que el recepcionista
	pthread_mutex_lock(&mutexColaClientes);
	while (posicion == -1 && i < maximoClientes) {
		if(clientes[i].tipo == tipoRecepcionista && clientes[i].id!=0 && clientes[i].atendido == 0){
			posicion = clientes[i].id;
		}
		i++;
	}
	
	pthread_mutex_unlock(&mutexColaClientes);

	return posicion;
}
/*FALTAN LOGS*/
void *accionesRecepcionista(void *arg){

	int tipoRecepcionista = *(int *) arg;		//tipo del recepcionista 0=no vip 1=vip;
	int recepcionistaOcupadoNoVip = 0;		//Ocupado = 1, no ocupado = 0
	int recepcionistaOcupadoVip = 0;  		//Ocupado = 1, no ocupado = 0	
	int posicionCliente;
	int aleatorio = -1;
	int contadorClientesAtendidos = 0;
	
	srand(gettid());
    
	while(fin != 2){
	    	posicionCliente = clienteMayorTiempoEsperando(tipoRecepcionista);
	    	/*acciones recepcionistas no vip*/
	    	if((tipoRecepcionista==0)&&(recepcionistaOcupadoNoVip==0)&&(contadorClientes!=0)&&(posicionCliente!=-1)){ 
	    		printf("entra el cliente %d en la cola no vip.\n",clientes[posicionCliente].id);
	    		recepcionistaOcupadoNoVip=1;
	    		contadorClientesAtendidos++;
	    		
	    		pthread_mutex_lock(&mutexColaClientes);
	    		clientes[posicionCliente].atendido = 1;		//el cliente está siendo atendido
	    		pthread_mutex_unlock(&mutexColaClientes);
	    		
	    		aleatorio = calculaAleatorios(0,100);	//calculamos el aleatorio
	    		
	    		if(aleatorio <=80){ //80% todo en regla.
		    		printf("El cliente tiene todo en regla");
		    		sleep((calculaAleatorios(1,4)));
		    		pthread_mutex_lock(&mutexColaClientes);
		    		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		    		pthread_mutex_unlock(&mutexColaClientes);

		    	}else if(aleatorio<= 90){	//10%mal identificados.
		      		printf("El cliente no se ha identificado correctamente");
		     		sleep((calculaAleatorios(2,6)));
		   		pthread_mutex_lock(&mutexColaClientes);
		     		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		    		pthread_mutex_unlock(&mutexColaClientes);

		    	}else{	//10% no tienen el pasaporte vacunal.
		       		printf("El cliente no tiene el pasaporte vacunal");   
		       		sleep((calculaAleatorios(6,10)));	
		       		eliminarCliente(clientes[posicionCliente].id);

		    	}
		    	if(contadorClientesAtendidos==5){		
		    		printf("El recepcionista va a descansar 5 segundos");
		    		sleep(5);
		    		contadorClientesAtendidos = 0;
		    	
	    		}
	    		recepcionistaOcupadoNoVip=0;
	    		
	    		
	    	}
	    	/*acciones recepcionistas vip*/
	    	if((tipoRecepcionista==1)&&(recepcionistaOcupadoVip==0)&&(contadorClientes!=0)&&(posicionCliente!=-1)){ 
	    		printf("entra el cliente %d en la cola vip.\n",clientes[posicionCliente].id);
	    		recepcionistaOcupadoVip=1;
	    		
	    		pthread_mutex_lock(&mutexColaClientes);
	    		clientes[posicionCliente].atendido = 1;		//el cliente está siendo atendido
	    		pthread_mutex_unlock(&mutexColaClientes);
	    		
	    		aleatorio = calculaAleatorios(0,100);	//calculamos el aleatorio
	    		
	    		if(aleatorio <=80){	//80% todo en regla.
		    		printf("El cliente tiene todo en regla");
		    		sleep((calculaAleatorios(1,4)));
		    		pthread_mutex_lock(&mutexColaClientes);
		    		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		    		pthread_mutex_unlock(&mutexColaClientes);

		    	}else if(aleatorio<= 90){	//10%mal identificados.
		      		printf("El cliente no se ha identificado correctamente");
		     		sleep((calculaAleatorios(2,6)));
		   		pthread_mutex_lock(&mutexColaClientes);
		     		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		    		pthread_mutex_unlock(&mutexColaClientes);

		    	}else{	//10% no tienen el pasaporte vacunal.
		       		printf("El cliente no tiene el pasaporte vacunal");   
		       		sleep((calculaAleatorios(6,10)));	
		       		eliminarCliente(clientes[posicionCliente].id);

		    	}
		    	recepcionistaOcupadoVip=0;
	    	}
	}
	sleep(1);	
}
void eliminarCliente(int id){
	int posicion=-1;
	int i=0;
	pthread_mutex_lock(&mutexColaClientes);
	/*buscamos al cliente a eliminar comprobando la lista de clientes y el argumento id.*/
	while(posicion==-1){
		if(id==clientes[i].id){
			posicion=i;
		}
	}
	/*se mueven a todos los clientes que esten por detras del que se va a eliminar un puesto hacia delante*/
	for(i=posicion;i<maximoClientes-1;i++){
		clientes[i].id=clientes[i+1].id;
		clientes[i].atendido=clientes[i+1].atendido;
		clientes[i].tipo=clientes[i+1].tipo;
		clientes[i].ascensor=clientes[i+1].ascensor;
		clientes[i].checking=clientes[i+1].checking;
	}
	/*se le da valor 0 (se deja el espacio libre en la ultima posicion de la lista*/
	clientes[maximoClientes-1].id=0;
	clientes[maximoClientes-1].atendido=0;
	clientes[maximoClientes-1].tipo=0;
	clientes[maximoClientes-1].ascensor=0;
	clientes[maximoClientes-1].checking=0;
	
	contadorClientes--;
	
	pthread_mutex_unlock(&mutexColaClientes);
}

void terminar(int sig){
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);
}


int calculaAleatorios(int min, int max){
    return rand() % (max - min + 1) + min;
}

/*Funcion para utilizar srand().*/
pid_t gettid(void){
    return syscall(__NR_gettid);
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

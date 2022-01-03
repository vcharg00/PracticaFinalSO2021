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
	int atendido;		//0 no atendido, 1 siendo atendido , 2 atendido
	int tipo;		//0 no vip, 1 vip
	int ascensor;
	int checking;
	pthread_t thCliente;
} clientes[20];


/* Lista de MaquinasCheckIn */

int maquinasCheck[5];

/* Fichero de log */
FILE *logFile;


/* Declaracion de las funciones */
void nuevoCliente(int sig);
void terminar(int sig);

int calculaAleatorios(int min, int max);
int clienteMayorTiempoEsperando(int tipoRecepcionista);
int comprobarAtendido(int id);
int maquinasLibres();
void eliminarCliente(int id);
pid_t gettid(void);

void *accionesCliente(void *arg);
void *accionesRecepcionista(void *arg);
void *ascensor(void *arg);

void writeLogMessage(int ClienteRecepcionistaMaquina, int id, char *msg);

int main(int argc, char* argv[]){
	
	int vip = 2;	//vip
	int noVip1 = 0; //no vip
	int noVip2 = 1;	//no vip
	int i;

	//Mensajes para los logs
	char msg[130];

	//Sobreescribir logs
	remove("registroTiempos.log");
	
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
	pthread_create(&recepcionista1,NULL,accionesRecepcionista,&noVip1);
	pthread_create(&recepcionista2,NULL,accionesRecepcionista,&noVip2);
	pthread_create(&recepcionista3,NULL,accionesRecepcionista,&vip);
	
	printf("pid al que enviar las señales: %d\n",getpid());

	while(1){
		if(fin==1){	//con fin=1 se espera a que se vacie el ascensor y los clientes esperando.
			if((contadorClientes==0)&&(ascensorFuncionando==0)){
				fin=2;		//con fin=2 se termina todo el programa.
				printf("Se ha cerrado el hotel.\n");
				sprintf(msg, "Se ha cerrado el hotel.");
				writeLogMessage(2, 0, msg);
				sleep(5);
				return 0;
			}
		}
	}
}
void nuevoCliente(int sig){

	signal(SIGUSR1, nuevoCliente);
	signal(SIGUSR2, nuevoCliente);
	int posicion=0;
	pthread_mutex_lock(&mutexColaClientes);
	
	if(contadorClientes<maximoClientes){
		
		contadorIDClientes++;
		clientes[contadorClientes].id = contadorIDClientes;
		
		if(sig == SIGUSR1){
			clientes[contadorClientes].tipo = 0;		//Tipo no VIP
		}else if(sig == SIGUSR2){
			clientes[contadorClientes].tipo = 1;		//Tipo VIP
		}
		
		pthread_create(&clientes[posicion].thCliente,NULL,accionesCliente,&posicion);
		posicion=contadorClientes;
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
    	while (posicion == -1){
		if(clientes[i].id == id){
		    posicion = i;
		    atendido = clientes[posicion].atendido;
		}
		i++;
    	}
    	pthread_mutex_unlock(&mutexColaClientes);

    	return atendido;
}

int maquinasLibres(){
	int posicion = -1;
	int i = 0;

	pthread_mutex_lock(&mutexMaquinas);
	while(posicion==-1 &&(i<5)){
		if(maquinasCheck[i]==0){
			posicion = i;
		}
		i++;
	}
	pthread_mutex_unlock(&mutexMaquinas);
	return posicion;
}

void *accionesCliente(void *arg){

	int maquinaLibre;
	int posicion = *(int *)arg;
	int id=clientes[posicion].id;
	int tipo=clientes[posicion].tipo;
	int atendido;
	int aleatorio;
	int condMaquinas=0;
	int clientesAscensor[6];
	int i;

	char msgCliente[100];
	//	1. Guardar en el log la hora de entrada.
	//	2. Guardar en el log el tipo de cliente.
	srand(gettid());
	
	printf("El cliente %d de tipo %d ha entrado en el hotel.\n",id,tipo);
	sprintf(msgCliente, "El cliente %d de tipo %d ha entrado en el hotel.\n",id,tipo);
	writeLogMessage(0, id, msgCliente);
	sleep(3);

	aleatorio = calculaAleatorios(1,100);
	if(aleatorio <= 1){	//10% de los clientes decide ir automaticamente a las maquinas de checking
		while(condMaquinas!=1){
			printf("El cliente con id %d decide ir automaticamente a las maquinas de checking\n",id);
			maquinaLibre = maquinasLibres();		//guardo en esta variable la posicion de la maquina 
			if(maquinaLibre != -1){			//si la maquina esta libre, es decir cualquier posicion distinta de -1...				
				printf("El cliente con id %d entra en la maquina %d para atenderse a si mismo\n",id,maquinaLibre);
				pthread_mutex_lock(&mutexMaquinas);
				maquinasCheck[maquinaLibre] = 1;	//Pongo la maquina como ocupada 
				pthread_mutex_unlock(&mutexMaquinas);
				
				pthread_mutex_lock(&mutexColaClientes);
				clientes[posicion].atendido = 1;	//el cliente se atiende a si mismo, y simula situacion durmiendo 6 segs
				clientes[posicion].checking = 1;
				pthread_mutex_unlock(&mutexColaClientes);
				sleep(6);
				printf("El cliente con id %d se ha atendido a si mismo en la maquina %d y ha tardado 6 seg\n",id,maquinaLibre);
				sprintf(msgCliente, "El cliente con id %d se ha atendido a si mismo en la maquina %d y ha tardado 6 seg\n",id,maquinaLibre);
				writeLogMessage(0, id, msgCliente);
				pthread_mutex_lock(&mutexColaClientes);
				clientes[posicion].atendido = 2;
				pthread_mutex_unlock(&mutexColaClientes);
				condMaquinas=1;

			}else{	//si no hay maquinas libres
				sleep(3);
				aleatorio = calculaAleatorios(1,100);
				if(aleatorio <= 50){
					printf("El cliente con Id %d se cansa de esperar en la zona de maquinas y decide ir a la recepcion normal\n",id);
					condMaquinas=1;
					
				}else{
					condMaquinas=0;		//se mantiene dentro del bucle
				}

			}
		}

	}else{
		srand(gettid());
		
		atendido = comprobarAtendido(id);
		while(atendido==0){	//PUNTO 4 90% restante se mantienen esperando				 
			aleatorio = calculaAleatorios(1,100);
			if(aleatorio <= 20){	//Se cansan de esperar y se van a las maquinas de checking
				printf("El cliente con id %d se cansa de esperar y decide ir a las maquinas de checking\n",id);
			}else if(aleatorio > 20 &&(aleatorio <= 30)){	//Se cansa de esperar y abandona el hotel
					printf("El cliente con id %d se ha cansado de esperar y abandona el hotel\n",id);
					writeLogMessage(0, id, "El cliente con id se ha ido al baño y abandona el hotel");
					eliminarCliente(id);
					pthread_exit(NULL);
			}else{
				aleatorio = calculaAleatorios(1,100);
				if(aleatorio <=5){		//Se va al baño y abanadona el hotel
					printf("El cliente con id %d se ha ido al baño y abandona el hotel\n",id);
					writeLogMessage(0, id, "El cliente con id se ha ido al baño y abandona el hotel");
					eliminarCliente(id);
					pthread_exit(NULL);
				}
			}
			atendido = comprobarAtendido(id);
			sleep(3);	//Si no ha sido atendido duerme 3 segundos y vuelve a hacer todas las comprobaciones.
			
		}
	}
	atendido = comprobarAtendido(id);
	if(atendido == 1){	//PUNTO 5
		sleep(2);
	}else if(atendido == 2){		//PUNTO 6
		aleatorio = calculaAleatorios(1,100);
		if(aleatorio <=30){			// El cliente espera al ascensor
			if(fin != 1){
				pthread_mutex_lock(&mutexColaClientes);
				clientes[posicion].ascensor = 1;
				pthread_mutex_unlock(&mutexColaClientes);
				printf("El cliente con id %d está preparado para coger el ascensor\n",id);
				writeLogMessage(0, id, "El cliente con id está preparado para coger el ascensor");
				while(ascensorFuncionando!=0){	//Mientras el ascensor esté lleno o este funcionando se queda esperando.	
					printf("El cliente con id %d está esperando al acensor porque está lleno\n",id);
					writeLogMessage(0, id, "El cliente con id está esperando al acensor porque está lleno");	
					sleep(3);
				}
				
				pthread_mutex_lock(&mutexAscensor);
				clientesAscensor[contadorAscensor]=id;	
				contadorAscensor++;
				
				pthread_cond_signal(&condAscensor);
				pthread_cond_wait(&condAscensor,&mutexAscensor); //si todo va bien aqui hay 5 hilos esperando +1 que es el que envia el signal (6);
				
				if(clientesAscensor[6]==id){
					printf("los clientes van a salir del ascensor empezando por el ultimo que entró\n");
				}
				//ejemplo sale el hilo con id 4
				for(i=5;i>=0;i--){
					if(clientesAscensor[i]==id){
						if(i==0){
							printf("el cliente %d es el ultimo en salir y libera el ascensor\n",clientesAscensor[i]);
							ascensorFuncionando=0;
							contadorAscensor=0;
							eliminarCliente(clientesAscensor[i]);
							pthread_exit(NULL);
							
						}else{
							printf("Sale el cliente %d del ascensor y se marcha.\n",clientesAscensor[i]);
							writeLogMessage(0, clientesAscensor[i], "Sale el cliente del ascensor y se marcha.");	
							eliminarCliente(clientesAscensor[i]);
							pthread_exit(NULL);
						}
					}
				}
				pthread_mutex_unlock(&mutexAscensor);
				
				
			}else{
				printf("El cliente con id %d  no espera al ascensor porque se deja de atender a clientes.\n",id);
				eliminarCliente(id);
				pthread_exit(NULL);			
			}			
		}else{
			printf("El cliente con id %d ha sido atendido, no espera al ascensor y se va a su habitación\n",id);
			writeLogMessage(0, id, "El cliente con id ha sido atendido, no espera al ascensor y se va a su habitación");	
			eliminarCliente(id);
			pthread_exit(NULL);					
		}

	}

}
void *ascensor(void *arg){
	
	while(fin!=2){
		pthread_mutex_lock(&mutexAscensor);
		while(contadorAscensor<6){
			printf("el ascensor todavia no se puede usar porque no está lleno\n");
			pthread_cond_wait(&condAscensor,&mutexAscensor);
		}
		printf("hay 6 clientes en el ascensor el ascensor procede a cerrar sus puertas y a subir\n");
		sleep(calculaAleatorios(3,6));
		ascensorFuncionando=1;
		pthread_cond_signal(&condAscensor);
		pthread_mutex_unlock(&mutexAscensor);
		
	}
}
int clienteMayorTiempoEsperando(int tipoRecepcionista){
	int posicion = -1;
	int i = 0;

	//Primero se buscan clientes del mismo tipo que el recepcionista
	pthread_mutex_lock(&mutexColaClientes);
	while (posicion == -1 && i < maximoClientes) {
		if((tipoRecepcionista==2)&&(clientes[i].tipo==1)){ 	//si el recepcionista es vip y el cliente tambien entra aquí.
			if((clientes[i].id!=0) && (clientes[i].atendido == 0) && (clientes[i].checking==0)){
				posicion = i;
			}
		}else if(((clientes[i].tipo==0)&&(tipoRecepcionista==0))||((clientes[i].tipo==0)&&(tipoRecepcionista==1))){ //si el recepcionista es no vip y el cliente tambien, entra aquí.
			if((clientes[i].id!=0) && (clientes[i].atendido == 0) && (clientes[i].checking==0)){
				posicion = i;
			}
		}
		i++;
	}
	
	pthread_mutex_unlock(&mutexColaClientes);

	return posicion;
}

void *accionesRecepcionista(void *arg){

	int tipoRecepcionista = *(int *) arg;		//tipo del recepcionista 0, 1=novip, 2=vip;
	int recepcionistaOcupado = 0;		//Ocupado = 1, no ocupado = 0	
	int posicionCliente;
	int aleatorio;
	int contadorClientesAtendidos = 0;
	
	char msgRecepcionista[130];

	if(tipoRecepcionista==0){
		printf("recepcionista %d atiende a clientes no vip.\n",tipoRecepcionista);
	}else if(tipoRecepcionista==1){
		printf("recepcionista %d atiende a clientes no vip.\n",tipoRecepcionista);
	}else if(tipoRecepcionista==2){
		printf("recepcionista %d atiende a clientes vip.\n",tipoRecepcionista);
	}
	
	srand(gettid());
    
	while(fin != 2){
	    	posicionCliente = clienteMayorTiempoEsperando(tipoRecepcionista);
	    	int id=clientes[posicionCliente].id;
	    	int checking=clientes[posicionCliente].checking;
	    	int tipo=clientes[posicionCliente].tipo;
	    	
	    	if((contadorClientes!=0)&&(recepcionistaOcupado==0)&&(posicionCliente!=-1)&&(checking==0)){
	    		pthread_mutex_lock(&mutexColaClientes);
	    		clientes[posicionCliente].atendido=1;
	    		pthread_mutex_unlock(&mutexColaClientes);
	    		
	    		recepcionistaOcupado=1;
	    		
	    		if(tipoRecepcionista==0){
				printf("el cliente %d es atendido en la cola no vip por el recepcionista %d.\n",id,tipoRecepcionista);
			}else if(tipoRecepcionista==1){
				printf("el cliente %d es atendido en la cola no vip por el recepcionista %d.\n",id,tipoRecepcionista);
			}else if(tipoRecepcionista==2){
				printf("el cliente %d es atendido en la cola vip por el recepcionista %d.\n",id,tipoRecepcionista);
			}
			
			if((tipoRecepcionista==0)||(tipoRecepcionista==1)){
				contadorClientesAtendidos++;
			}
			
			aleatorio=calculaAleatorios(0,100);
			
			if(aleatorio <=80){ //80% todo en regla.
	    			printf("El cliente %d tiene todo en regla\n",id);
	    			sleep((calculaAleatorios(1,4)));
	    			
	    			printf("El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
	    			sprintf(msgRecepcionista, "El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
				writeLogMessage(1, 0, msgRecepcionista);
	    			sleep(1);
		    		pthread_mutex_lock(&mutexColaClientes);
		    		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		    		pthread_mutex_unlock(&mutexColaClientes);
		    		
		    	}else if(aleatorio<= 90){	//10%mal identificados.
		      		printf("El cliente %d no se ha identificado correctamente le atienden mas despacio\n",id);
		     		sleep((calculaAleatorios(2,6)));
		     		
		     		printf("El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
		     		sprintf(msgRecepcionista, "El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
				writeLogMessage(1, 0, msgRecepcionista);
		     		sleep(1);
		   		pthread_mutex_lock(&mutexColaClientes);
		     		clientes[posicionCliente].atendido = 2;		//Acaba de ser atendido el cliente
		     		sprintf(msgRecepcionista, "El recepcionista %d ha termiado la atencion.\n",tipoRecepcionista);
				writeLogMessage(1, 0, msgRecepcionista);
		    		pthread_mutex_unlock(&mutexColaClientes);

		    	}else{	//10% no tienen el pasaporte vacunal.
		       		printf("El cliente %d ha sido atendido por el recepcionista %d, pero no tiene el pasaporte vacunal asique se marcha\n",id,tipoRecepcionista);  
		       		sprintf(msgRecepcionista, "El cliente %d ha sido atendido por el recepcionista %d, pero no tiene el pasaporte vacunal asique se marcha\n",id,tipoRecepcionista);
					writeLogMessage(1, 0, msgRecepcionista);
		       		sleep((calculaAleatorios(6,10)));	
		       		eliminarCliente(id);
		       		pthread_exit(NULL);

		    	}
		    	if(contadorClientesAtendidos==5){	//al llegar a 5 el recepcionista descansa
		    		printf("El recepcionista %d va a descansar 5 segundos\n",tipoRecepcionista);
		    		sprintf(msgRecepcionista, "El recepcionista %d inicia su descanso\n",tipoRecepcionista);
				writeLogMessage(1, 0, msgRecepcionista);
		    		sleep(5);
		    		sprintf(msgRecepcionista, "El recepcionista %d termina su descanso\n",tipoRecepcionista);
				writeLogMessage(1, 0, msgRecepcionista);
		    		contadorClientesAtendidos = 0;
		    	}
		    	sleep(1);
		    	recepcionistaOcupado=0;
			    		
		}else{
			sleep(1);	//vuelve a comprobar si hay clientes que atender
		}
	    	
	}
	
	pthread_exit(NULL); //mata a los recepcionistas

}

void writeLogMessage (int ClienteRecepcionistaMaquina, int id, char *msg) {
    pthread_mutex_lock(&mutexLog);
	// Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[25];
	strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
	char fuenteLog[20];
	switch (ClienteRecepcionistaMaquina) {
		case 0:
			sprintf(fuenteLog, "%s %d", "Cliente", id);
			break;
		case 1:
			sprintf(fuenteLog, "%s %d", "Recepcionista", id);
			break;
		case 2:
			sprintf(fuenteLog, "Maquina");
			break;
	}
	// Escribimos en el log
	logFile = fopen("registroTiempos.log", "a");
	fprintf(logFile, "[ %s ]: [%s]: %s\n", stnow, fuenteLog, msg);
	fclose(logFile);    
    pthread_mutex_unlock(&mutexLog);
}

void eliminarCliente(int id){
	pthread_mutex_lock(&mutexColaClientes);
	int posicion=-1;
	int i=0;
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
	fin=1;
	printf("\nse va a dejar de atender a nuevos clientes, solo se atenderá a aquellos que estén en cola o esperando al ascensor.\n");
}


int calculaAleatorios(int min, int max){
    return rand() % (max - min + 1) + min;
}

/*Funcion para utilizar srand().*/
pid_t gettid(void){
    return syscall(__NR_gettid);
}

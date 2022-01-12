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
pthread_cond_t condClientes;

/*Otras variables globales*/
int contadorIDClientes;
int contadorClientes;			// Va de a 20.
int contadorAscensor;			//cuenta el numero de clientes que hay en el ascensor.
int ascensorFuncionando;		// 0 no funciona 1, si.
int fin;

/* Lista de 20 clientes */
struct cliente{
	int id;
	int atendido;		//0 no atendido, 1 siendo atendido , 2 atendido, 3 eliminar a clientes.
	int tipo;		//0 no vip, 1 vip.
	int ascensor;		//0 no usa el ascensor , 1 usa el ascensor.
	int checking;		//0 no utiliza las maquinas de checking, 1 utiliza las maquinas.
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
void cambiarAtendido(int id, int nuevoAtendido);
void cambiarChecking(int id, int nuevoChecking);
int maquinasLibres();
void eliminarCliente(int id);
pid_t gettid(void);

void *accionesCliente(void *arg);
void *accionesRecepcionista(void *arg);
void *ascensor(void *arg);

void writeLogMessage(int clienteRecepcionistaMaquina, int id, char *msg);
/*Funcion principal del programa*/
int main(int argc, char* argv[]){
	
	int vip = 2;	//vip
	int noVip1 = 0; //no vip
	int noVip2 = 1;	//no vip
	int i;

	/*Mensajes para los logs*/
	char msg[130];

	/*Sobreescribir logs*/
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
 	pthread_cond_init(&condClientes,NULL);
 	
 	contadorClientes = 0;
 	contadorIDClientes = 0;
 	fin=0;
  	
  	/*Lista de clientes*/
  	for(i = 0; i < maximoClientes; i++){
  		clientes[i].id = 0;
  		clientes[i].atendido = 0;
  		clientes[i].tipo = 0;
  		clientes[i].ascensor = 0;
  		clientes[i].checking = 0;
  	}

  	/*hilos de recepcionistas y ascensor*/
  	pthread_t recepcionista1, recepcionista2, recepcionista3 , ascensorHotel;
  	

  	/*Máquinas de checkIn*/
  	for(i = 0; i < 5; i++){
  		maquinasCheck[i] = 0;
  	}

  	/*Fichero log*/
  	logFile = fopen("registroTiempos.log", "w");

  	/*Variables relativas al ascensor*/
	ascensorFuncionando = 0;
	contadorAscensor = -1;

	/*Crear 3 hilos recepcionistas y 1 de ascensor*/
	pthread_create(&recepcionista1,NULL,accionesRecepcionista,&noVip1);
	pthread_create(&recepcionista2,NULL,accionesRecepcionista,&noVip2);
	pthread_create(&recepcionista3,NULL,accionesRecepcionista,&vip);
	pthread_create(&ascensorHotel,NULL,ascensor,NULL);
	
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
/*Funcion que se encarga de añadir nuevos clientes.*/
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
/*Funcion que comprueba el valor de atendido*/
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
/*Funcion que cambia la variable atendido de los clientes a 1,2,3*/
void cambiarAtendido(int id, int nuevoAtendido){
	int posicion=-1;
	int i=0;
	
	pthread_mutex_lock(&mutexColaClientes);
    	while (posicion == -1){
		if(clientes[i].id == id){
		    posicion = i;
		    clientes[posicion].atendido=nuevoAtendido;
		}
		i++;
    	}
    	pthread_mutex_unlock(&mutexColaClientes);
}
/*Funcion que cambia la variable checking de los clientes a 1 o a 0.*/
void cambiarChecking(int id, int nuevoChecking){
	int posicion=-1;
	int i=0;

	pthread_mutex_lock(&mutexColaClientes);
	while (posicion == -1){
		if(clientes[i].id == id){
		    posicion = i;
		    clientes[posicion].checking=nuevoChecking;
		}
		i++;
	}
	pthread_mutex_unlock(&mutexColaClientes);
}
/*Funcion que comprueba si hay maquinas libres.*/
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
/*Funcion que se encarga de realizar todas las funciones relacionadas con el cliente.*/
void *accionesCliente(void *arg){

	int maquinaLibre;
	int posicion = *(int *)arg;
	int id=clientes[posicion].id;
	int tipo=clientes[posicion].tipo;
	int atendido;
	int aleatorio;
	int condMaquinas=0;	//variable que mantiene al cliente en el bucle de las maquinas o no.
	int clientesAscensor[6];
	int i;
	int entrarMaquina=0;	//si es 0 no has entrado a maquinas y puedes hacerlo, si es 1 has entrado pero no puedes volver a entrar si es 2 entras directamente.

	char msgCliente[100];
	char msgMaquina[100];
	
	printf("El cliente %d de tipo %d ha entrado en el hotel.\n",id,tipo);
	sprintf(msgCliente, "El cliente %d de tipo %d ha entrado en el hotel.\n",id,tipo);
	writeLogMessage(0, id, msgCliente);
	sleep(3);
	srand(gettid());
	while(1){
		atendido = comprobarAtendido(id);
		if(atendido==0){
			aleatorio = calculaAleatorios(1,100);
			if((aleatorio <= 10)&&(entrarMaquina==0)||(entrarMaquina==2)){	//10% de los clientes decide ir automaticamente a las maquinas de checking
				cambiarChecking(id,1);
				while(condMaquinas!=1){
					printf("El cliente con id %d está listo para entrar a las maquinas de checking.\n",id);
					maquinaLibre = maquinasLibres();		//guardo en esta variable la posicion de la maquina 
					if(maquinaLibre != -1){			//si la maquina esta libre, es decir cualquier posicion distinta de -1...				
						printf("El cliente con id %d entra en la maquina %d para atenderse a si mismo\n",id,maquinaLibre);
						sprintf(msgMaquina, "El cliente con id %d entra en la maquina %d para atenderse a si mismo\n",id,maquinaLibre);
						writeLogMessage(2, 0, msgMaquina);
						pthread_mutex_lock(&mutexMaquinas);
						maquinasCheck[maquinaLibre] = 1;	//Pongo la maquina como ocupada 
						pthread_mutex_unlock(&mutexMaquinas);
						cambiarAtendido(id,1);
						
						/*el cliente se atiende a si mismo, y simula situacion durmiendo 6 segs*/
						
						sleep(6);
						printf("El cliente con id %d se ha atendido a si mismo en la maquina %d y ha tardado 6 seg\n",id,maquinaLibre);
						pthread_mutex_lock(&mutexMaquinas);
						maquinasCheck[maquinaLibre] = 0;	//Pongo la maquina como libre 
						pthread_mutex_unlock(&mutexMaquinas);
						
						sprintf(msgMaquina, "El cliente con id %d se ha atendido a si mismo en la maquina %d y ha tardado 6 seg\n",id,maquinaLibre);
						writeLogMessage(2, 0, msgMaquina);
						cambiarAtendido(id,2);
						condMaquinas=1;
						entrarMaquina=1;
					
					}else{	//si no hay maquinas libres
						sleep(3);
						aleatorio = calculaAleatorios(1,100);
						if(aleatorio <= 50){
							printf("El cliente con Id %d se cansa de esperar en la zona de maquinas y decide ir a la recepcion normal.\n",id);
							sprintf(msgMaquina, "El cliente con Id %d se cansa de esperar en la zona de maquinas y decide ir a la recepcion normal.\n",id);
							writeLogMessage(2, 0, msgMaquina);
							condMaquinas=1;
							entrarMaquina=1;
							cambiarChecking(id,0);
							
						}else{
							condMaquinas=0;		//se mantiene dentro del bucle
						}
					}
				}

			}else{	//PUNTO 4 90% restante se mantienen esperando			 
				aleatorio = calculaAleatorios(1,100);
				if(aleatorio <= 50){	//Se cansan de esperar y se van a las maquinas de checking
					printf("El cliente con id %d se cansa de esperar y decide ir a las maquinas de checking.\n",id);
					sprintf(msgCliente, "El cliente con id %d se cansa de esperar y decide ir a las maquinas de checking.\n",id);
					writeLogMessage(0, id, msgCliente);
					entrarMaquina=2;
					condMaquinas=0;
					cambiarChecking(id,1);
					sleep(1);
				}else if(aleatorio > 50 &&(aleatorio <= 60)){	//Se cansa de esperar y abandona el hotel
						printf("El cliente con id %d se ha cansado de esperar y abandona el hotel\n",id);
						sprintf(msgCliente, "El cliente con id %d se ha cansado de esperar y abandona el hotel\n",id);
						writeLogMessage(0, id, msgCliente);
						eliminarCliente(id);
						pthread_exit(NULL);
				}else{
					aleatorio = calculaAleatorios(1,100);
					if(aleatorio <=5){		//Se va al baño y abanadona el hotel
						printf("El cliente con id %d se ha ido al baño y abandona el hotel\n",id);
						sprintf(msgCliente, "El cliente con id %d se ha ido al baño y abandona el hotel\n",id);
						writeLogMessage(0, id, msgCliente);
						eliminarCliente(id);
						pthread_exit(NULL);
					}
				}
				cambiarChecking(id,0);
				sleep(3);	//Si no ha sido atendido duerme 3 segundos y vuelve a hacer todas las comprobaciones.
			}
		}else if(atendido == 1){	//PUNTO 5
			sleep(2);
		}else if(atendido == 2){		//PUNTO 6
			aleatorio = calculaAleatorios(1,100);
			if(aleatorio <=30){			// El cliente espera al ascensor
				if(fin != 1){
					pthread_mutex_lock(&mutexColaClientes);
					clientes[posicion].ascensor = 1;
					pthread_mutex_unlock(&mutexColaClientes);
					printf("El cliente con id %d está preparado para coger el ascensor\n",id);
					sprintf(msgCliente, "El cliente con id %d está preparado para coger el ascensor\n",id);
					writeLogMessage(0, id, msgCliente);
					pthread_mutex_lock(&mutexAscensor);
					while(ascensorFuncionando!=0){	//Mientras el ascensor esté lleno o este funcionando se queda esperando.					
						pthread_mutex_unlock(&mutexAscensor);
						printf("El cliente con id %d está esperando al acensor porque está lleno\n",id);
						sprintf(msgCliente, "El cliente con id %d está esperando al acensor porque está lleno\n",id);
						writeLogMessage(0, id, msgCliente);
						sleep(3);
						pthread_mutex_lock(&mutexAscensor);
					}
					pthread_mutex_unlock(&mutexAscensor);
					
					pthread_mutex_lock(&mutexAscensor);
					contadorAscensor++;
					clientesAscensor[contadorAscensor]=id;	
					
					if(contadorAscensor==5){
						pthread_cond_signal(&condAscensor);
					}else{
						printf("el ascensor todavia no se puede usar porque no está lleno\n");
					}
					pthread_cond_wait(&condClientes,&mutexAscensor);
					
					//ejemplo sale el hilo con id 4
					for(i=5;i>=0;i--){
						if(clientesAscensor[i]==id){
							if(i==0){
								sleep(3);
								printf("el cliente %d es el ultimo en salir y libera el ascensor\n",clientesAscensor[i]);
								ascensorFuncionando=0;
								contadorAscensor=-1;
								cambiarAtendido(id,3);
							}else{
								printf("Sale el cliente %d del ascensor y se marcha.\n",clientesAscensor[i]);
								sprintf(msgCliente, "Sale el cliente %d del ascensor y se marcha.\n",clientesAscensor[i]);
								writeLogMessage(0, id, msgCliente);	
								cambiarAtendido(id,3);
							}
						}
						sleep(1);	
					}
					pthread_mutex_unlock(&mutexAscensor);
				}else{
					printf("El cliente con id %d no espera al ascensor porque se deja de atender a clientes.\n",id);
					sprintf(msgCliente, "El cliente con id %d no espera al ascensor porque se deja de atender a clientes.\n",id);
					writeLogMessage(0, id, msgCliente);	
					cambiarAtendido(id,3);		
				}			
			}else{
				printf("El cliente con id %d ha sido atendido, no espera al ascensor y se va a su habitación\n",id);
				sprintf(msgCliente, "El cliente con id %d ha sido atendido, no espera al ascensor y se va a su habitación\n",id);
				writeLogMessage(0, id, msgCliente);	
				cambiarAtendido(id,3);					
			}

		}else if(atendido == 3){	//si atendido es igual a 3 eliminamos al cliente
			printf("El cliente %d abandona el hotel.\n",id);
			sprintf(msgCliente, "El cliente %d abandona el hotel.\n",id);
			writeLogMessage(0, id, msgCliente);	
			eliminarCliente(id);
			pthread_exit(NULL);
		}
	}
}
void *ascensor(void *arg){
	int i;
	while(fin!=2){
		pthread_mutex_lock(&mutexAscensor);
		pthread_cond_wait(&condAscensor,&mutexAscensor);
		ascensorFuncionando=1;
		printf("\nhay 6 clientes en el ascensor el ascensor procede a cerrar sus puertas y a subir\n");
		sleep(calculaAleatorios(3,6));
		printf("los clientes van a salir del ascensor empezando por el ultimo que entró\n");
		for(i=0;i<=contadorAscensor;i++){
			pthread_cond_signal(&condClientes);
		}
		pthread_mutex_unlock(&mutexAscensor);
	}
	pthread_exit(NULL);
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
/*Funcion que se encarga de realizar las funciones relacionadas con el recepcionista.*/
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
	    		cambiarAtendido(id,1);
	    		
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
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
	    			sleep(1);
	    			
		    		cambiarAtendido(id,2);	//Acaba de ser atendido el cliente
		    		
		    	}else if(aleatorio<= 90){	//10%mal identificados.
		      		printf("El cliente %d no se ha identificado correctamente le atienden mas despacio\n",id);
		      		sprintf(msgRecepcionista, "El cliente %d no se ha identificado correctamente le atienden mas despacio\n",id);
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
		     		sleep((calculaAleatorios(2,6)));
		     		
		     		printf("El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
		     		sprintf(msgRecepcionista, "El cliente %d ha sido atendido por el recepcionista %d\n",id,tipoRecepcionista);
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
		     		sleep(1);
		   		
		     		cambiarAtendido(id,2);	//Acaba de ser atendido el cliente
		     		sprintf(msgRecepcionista, "El recepcionista %d ha termiado la atencion.\n",tipoRecepcionista);
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
		    		
		    	}else{	//10% no tienen el pasaporte vacunal.
		       		printf("El cliente %d ha sido atendido por el recepcionista %d, pero no tiene el pasaporte vacunal asique se marcha\n",id,tipoRecepcionista);  
		       		sprintf(msgRecepcionista, "El cliente %d ha sido atendido por el recepcionista %d, pero no tiene el pasaporte vacunal asique se marcha\n",id,tipoRecepcionista);
					writeLogMessage(1, id, msgRecepcionista);
		       		sleep((calculaAleatorios(6,10)));	
		       		cambiarAtendido(id,3);
		    	}
		    	if(contadorClientesAtendidos==5){	//al llegar a 5 el recepcionista descansa
		    		printf("El recepcionista %d va a descansar 5 segundos\n",tipoRecepcionista);
		    		sprintf(msgRecepcionista, "El recepcionista %d va a descansar 5 segundos\n",tipoRecepcionista);
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
		    		sleep(5);
		    		sprintf(msgRecepcionista, "El recepcionista %d termina su descanso",tipoRecepcionista);
				writeLogMessage(1, tipoRecepcionista, msgRecepcionista);
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
/*Funcion que se encarga de escribir en el log.*/
void writeLogMessage (int clienteRecepcionistaMaquina, int id, char *msg) {
    pthread_mutex_lock(&mutexLog);
	// Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[25];
	strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
	char fuenteLog[20];
	switch (clienteRecepcionistaMaquina) {
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
/*Funcion que se encarga de eliminar los clientes.*/
void eliminarCliente(int id){
	pthread_mutex_lock(&mutexColaClientes);
	int posicion=-1;
	int i=0;
	/*buscamos al cliente a eliminar comprobando la lista de clientes y el argumento id.*/
	while((posicion==-1)&&(i<maximoClientes)){
		if(id==clientes[i].id){
			posicion=i;
		}
		i++;	
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
/*Funcion que recibe la señal de terminar el programa.*/
void terminar(int sig){
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	char msgMaquina[160];
	fin=1;
	pthread_cond_signal(&condAscensor);
	printf("\nse va a dejar de atender a nuevos clientes, solo se atenderá a aquellos que estén en cola o esperando al ascensor.\n");
	sprintf(msgMaquina, "\nse va a dejar de atender a nuevos clientes, solo se atenderá a aquellos que estén en cola o esperando al ascensor.\n");
	writeLogMessage(2, 0, msgMaquina);
}
/*Funcion que calcula aleatorios.*/
int calculaAleatorios(int min, int max){
	
    return rand() % (max - min + 1) + min;
}
/*Funcion para utilizar srand().*/
pid_t gettid(void){
    return syscall(__NR_gettid);
}

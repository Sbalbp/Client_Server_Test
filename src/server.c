#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#define NUM_USERS 4

// Usuarios y sus passwords
char usuarios[NUM_USERS][100] = {{"Sbalbp"},{"user1"},{"user2"},{"user3"}};
char passwords[NUM_USERS][100] = {{"pass1"},{"121212"},{"MyPass"},{"123456"}};

char directorio[256];
char *ips[NUM_USERS];
int *flags;
key_t key;
int shmid;

// Direccion IP del cliente
char *direccion;

void *SigCatcher(int n){
  wait3(NULL,WNOHANG,NULL);
  printf("Sesion cerrada\n");
}

void hijo(int);

void split(char *cad, char token, int pos, char devolver[]);

void error(const char *msg){
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){

	bzero(directorio,256);
    sprintf(directorio,".");

    // Memoria compartida
    int flag;

    key = ftok("server.c",'R');
    shmid = shmget(key, 1024, 0644 | IPC_CREAT);

    /* Attach to Shared Memory */
    flags = shmat(shmid, (void *)0, 0);
    if(flags == (int *)(-1))
		perror("shmat");

    for(flag=0;flag<NUM_USERS;flag++)
    	flags[flag]=0;


    char reto[8];

    // Atrapa-zombies
    signal(SIGCHLD,(__sighandler_t)SigCatcher);

    int sockfd, newsockfd, portno, pid;

    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no se ha proporcionado puerto\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    	error("ERROR en la apertura del socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR en binding");

    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0)
            error("ERROR en aceptacion");

        pid = fork();

        if (pid < 0)
            error("ERROR en fork");
        if (pid == 0)  {
	    	// Obtenemos la direccion IP del cliente
	    	direccion = (char*)inet_ntoa(cli_addr.sin_addr);

            close(sockfd);
            hijo(newsockfd);
            exit(0);
        }
        else{
	    	close(newsockfd);
	 	}
    }
    close(sockfd);
    return 0;
}

void split(char *cad, char token, int pos, char devolver[]){
	int i,j,k;

	i = j = k = 0;

	while (i <= pos){
		k = 0;
		bzero(devolver,256);

		for(;cad[j]==token;j++){}

		for(;j<strlen(cad) && cad[j]!=token;j++,k++){
			devolver[k] = cad[j];
		}
		i++;
	}
}

// Funcion encargada dar servicio a un cliente concreto. Existe una instancia de esta funcion por cada cliente conectado
void hijo (int sock){
   	int i, n, itr, identificado = 1, file, leido;
   	int uid, it, mensaje = 0;
   	unsigned int size, total;
   	char buffer[256], bufferR[256], bufferW[256], reto[8], secreto[256], usuario[256], password[256], msg[256], arg[256];
   	unsigned char out[16], outRec[16];
   	struct stat st;

   	// Iniciar semilla
   	srand (time(NULL));

   // Creacion del reto aleatorio
	for(itr=0;itr<8;itr++)
		reto[itr] = rand()%100+1;

   	// Recibir user
   	bzero(usuario,256);
   	n = read(sock,usuario,255);

   	// Buscar user y pass, formando secreto
   	for(itr=0; itr<NUM_USERS; itr++){
   		if(strcmp(usuario,usuarios[itr]) == 0){
			uid = itr;

			sprintf(secreto,"%s%s%s",usuarios[itr],passwords[itr],reto);

			// codificacion del secreto
    		// Habria que usar algun algoritmo como MD5
    		for(i=0; i<16; i++){
    			out[i]=reto[i%8]+usuarios[itr][i%strlen(usuarios[itr])]+passwords[itr][i%strlen(passwords[itr])];
    		}
		}
   	}

   	// Enviar reto
   	n = write(sock,reto,8);
   	// Recibir secreto
   	n = read(sock,outRec,255);

   	// Comprobar secretos
   	for (itr=0; itr<16; itr++) {
		if(out[itr] != outRec[itr]){
			identificado = 0;
		}
   	}


   	if(identificado == 1 && flags[uid]==0){
		// Bloquear sesion
		flags[uid]=1;
		n = write(sock,"IDENTIFICADO",12);

		// bucle
   		do{

			mensaje = 0;

			unsigned int tam_com;

   			bzero(buffer,256);
			read(sock,(unsigned char*) &tam_com,sizeof(unsigned int));
			bzero(buffer,256);
			n = read(sock,buffer,tam_com);

   			if (n < 0) error("ERROR de lectura del socket");
   			printf("Mensaje de %s: %s\n",usuario,buffer);

			bzero(msg,256);
			split(buffer,' ',0,msg);

			if(strcmp(msg,"fetch")==0){
				mensaje = 1;
				bzero(msg,256);
				split(buffer,' ',1,msg);

				char ruta[256];
				bzero(ruta,256);
				sprintf(ruta,"%s/%s",directorio,msg);
				strcpy(msg,ruta);

				// Obtener tamaño del fichero
				size = 0;
				stat(msg, &st);
		        size = st.st_size;

				bzero(bufferW,256);
				n = write(sock,(unsigned char*) &size,sizeof(unsigned int));

				if(size > 0){
					// Apertura del fichero
					file = open(msg,O_RDONLY|O_NOCTTY|O_CREAT,0644);

					bzero(bufferW,256);
					// Mientras no se acabe el fichero, se envia informacion
		        	while( (leido = read(file,bufferW,256))!=0){
						n = write(sock,bufferW,leido);
						bzero(bufferW,256);
					}

					close(file);
				}

			}

			if(strcmp(msg,"store")==0){
				it = 0;
				mensaje = 1;

				// Obtener tamaño del fichero
				read(sock,(unsigned char*) &size,sizeof(unsigned int));

				if(size != 0){
					bzero(msg,256);
					split(buffer,' ',1,msg);

					char ruta[256];
					bzero(ruta,256);
					sprintf(ruta,"%s/%s",directorio,msg);
					strcpy(msg,ruta);

					file = open(msg,O_RDWR|O_NOCTTY|O_CREAT,0644);
					// Mientras no se haya leido todo el fichero
					while(it < size){
						// Leemos lo que se envio
						bzero(bufferR,256);
						leido = read(sock,bufferR,256);
						write(file,bufferR,leido);

						it+=leido;
					}
					close(file);
				}
			}

			if(strcmp(msg,"check_users")==0){
				unsigned char longt;
				mensaje = 1;

				total = 0;
				for(it=0;it<NUM_USERS;it++)
					total+=flags[it];

				bzero(bufferW,256);
				n = write(sock,(unsigned char*) &total,sizeof(unsigned int));

				for(it=0; it<NUM_USERS; it++){
					if(flags[it] == 1){
						longt = strlen(usuarios[it]);

						// Envio del nombre
						n = write(sock,(unsigned char*) &longt,sizeof(unsigned int));
						n = write(sock,usuarios[it],longt);

						longt = strlen(direccion);

						// Envio de la IP
						n = write(sock,(unsigned char*) &longt,sizeof(unsigned int));
						n = write(sock,direccion,longt);
					}
				}
			}

			if(strcmp(msg,"check_content")==0){
				mensaje = 1;
				char comando[256];

				bzero(comando,256);
				sprintf(comando,"ls -B --block-size=1024 -l %s | awk '{print $9\"  -  \"$5\" KB\"}' | grep '[1-9]' > temporalData",directorio);
				system(comando);

				// Obtener tamaño del fichero
				size = 0;
				stat("temporalData", &st);
		        	size = st.st_size;

				bzero(bufferW,256);
				n = write(sock,(unsigned char*) &size,sizeof(unsigned int));

				if(size > 0){
					// Apertura del fichero
					file = open("temporalData",O_RDONLY|O_NOCTTY|O_CREAT,0644);

					bzero(bufferW,256);
					// Mientras no se acabe el fichero, se envia informacion
		        	while( (leido = read(file,bufferW,256))!=0){
						n = write(sock,bufferW,leido);
						bzero(bufferW,256);
					}

					close(file);
				}

				system("rm temporalData");
			}

			if(strcmp(msg,"browse")==0){
				mensaje = 1;

				bzero(msg,256);
				split(buffer,' ',1,msg);

				sprintf(directorio,"%s/%s",directorio,msg);
			}

			if(strcmp(msg,"reset")==0){
				mensaje = 1;

				bzero(directorio,256);
				sprintf(directorio,".");
			}

			if(strcmp(msg,"help")==0){
				mensaje = 1;
			}


   		}while(strcmp(buffer,"logout")!=0);

		// Liberar la sesion
		flags[uid]=0;
   	}
   	else{
		if(flags[uid]==0){
			n = write(sock,"NO IDENTIFICADO",15);
		}
		else{
			n = write(sock,"YA IDENTIFICADO",15);
		}
	}
}

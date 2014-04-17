#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>

void error(const char *msg){
    perror(msg);
    exit(0);
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

int main(int argc, char *argv[]){
    int i, sockfd, portno, n, file, itPer;
    int mensaje = 0;
    unsigned int size, leido, it, total;
    unsigned char out[16];
    char* direccion;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    struct stat st;

    char bufferW[256], bufferR[256], user[256], password[256], secreto[256], fichero[256], msg[256];

    if (argc < 3) {
       fprintf(stderr,"sintaxis: \"%s <hostname> <puerto>\"\n", argv[0]);
       exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    	error("ERROR en la apertura del socket");

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr,"ERROR, no existe el host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR conectando");

    // Obtenemos la direccion IP del servidor
    direccion = (char*)inet_ntoa(serv_addr.sin_addr);

    // Introducimos usuario y password
    printf("---------------------------------------------------------\n");
    printf("|          INTRODUZCA SU NOMBRE DE USUARIO              |\n");
    printf("---------------------------------------------------------\n> ");
    fgets(user,256,stdin);
    user[strlen(user)-1]='\0';
    printf("---------------------------------------------------------\n");
    printf("|               INTRODUZCA SU CONTRASEÑA                |\n ");
    printf("---------------------------------------------------------\n> ");
    fgets(password,256,stdin);
    password[strlen(password)-1]='\0';

    // Envio del usuario
    n = write(sockfd,user,strlen(user));
    // recepcion del reto
    bzero(bufferR,256);
    n = read(sockfd,bufferR,255);
    // creacion del secreto
    sprintf(secreto,"%s%s%s",user,password,bufferR);

    // codificacion del secreto
    // Habria que usar algun algoritmo como MD5
    for(i=0; i<16; i++){
    	out[i]=bufferR[i%8]+user[i%strlen(user)]+password[i%strlen(password)];
    }

    // envio del secreto
    n = write(sockfd,out,strlen(out));
    // recepcion de confirmacion
    bzero(bufferR,256);
    n = read(sockfd,bufferR,255);

    if(strcmp(bufferR,"IDENTIFICADO") == 0){
		printf("\n--> Conectado con exito\n\n");

    	// bucle
    	do{
			mensaje = 0;

    		printf("--> Teclee un mensaje: \n\n %s @ %s > ",user,direccion);
    		bzero(bufferW,256);
        	bzero(bufferR,256);

			unsigned int tam_com;

	    	fgets(bufferW,256,stdin);
			bufferW[strlen(bufferW)-1]='\0';
			tam_com = strlen(bufferW);
			write(sockfd,(unsigned char*) &tam_com,sizeof(unsigned int));
			n = write(sockfd,bufferW,strlen(bufferW));

			bzero(msg,256);
			split(bufferW,' ',0,msg);


	    	if (n < 0)
	    	    error("ERROR de escritura en socket");

			if(strcmp(msg,"fetch")==0){
				mensaje = 1;
				it = 0;
				itPer = 0;

				bzero(msg,256);
				split(bufferW,' ',1,msg);

				// Obtener tamaño del fichero
				read(sockfd,(unsigned char*) &size,sizeof(unsigned int));

				if(size != 0){
					file = open(msg,O_RDWR|O_NOCTTY|O_CREAT|O_TRUNC,0644);
					// Mientras no se haya leido todo el fichero
					while(it < size){
						// Leemos lo que se envio
						bzero(bufferR,256);
						leido = read(sockfd,bufferR,256);
						write(file,bufferR,leido);

						it+=leido;
						itPer++;
						if(itPer%300 == 0)
							printf("--> fetching: %f%%\n",100.0*(float)it/(float)size);
					}
					printf("\n--> fetch finalizado\n\n");
					close(file);
				}
				else{
					printf("\n--> Error: Nada que traer\n\n");
				}
			}

			if(strcmp(msg,"store")==0){
				mensaje = 1;
				it = 0;
				itPer = 0;
				bzero(msg,256);
				split(bufferW,' ',1,msg);

				// Obtener tamaño del fichero
				size = 0;
				stat(msg, &st);
		        	size = st.st_size;

				bzero(bufferW,256);
				n = write(sockfd,(unsigned char*) &size,sizeof(unsigned int));

				if(size > 0){
					// Apertura del fichero
					file = open(msg,O_RDONLY|O_NOCTTY|O_CREAT,0644);

					bzero(bufferW,256);
					// Mientras no se acabe el fichero, se envia informacion
		        	while( (leido = read(file,bufferW,256))!=0){
						n = write(sockfd,bufferW,leido);
						bzero(bufferW,256);

						it+=leido;
						itPer++;
						if(itPer%300 == 0)
							printf("--> storing: %f%%\n",100.0*(float)it/(float)size);
					}

					printf("\n--> store finalizado\n\n");
					close(file);
				}
				else{
					printf("\n--> Nada que enviar\n\n");
				}
			}

			if(strcmp(msg,"check_users")==0){
				unsigned char longt;
				mensaje = 1;

				read(sockfd,(unsigned char*) &total,sizeof(unsigned int));

				printf("\n---------------------------------------------------------\n");
				printf("|                  USUARIOS CONECTADOS                  |\n");
				printf("---------------------------------------------------------\n");

				for(it=0; it<total; it++){
					// Leer nombre
					bzero(bufferR,256);
					read(sockfd,(unsigned char*) &longt,sizeof(unsigned int));
					read(sockfd,bufferR,longt);
					printf("|  %s",bufferR);

					// Leer IP
					bzero(bufferR,256);
					read(sockfd,(unsigned char*) &longt,sizeof(unsigned int));
					read(sockfd,bufferR,longt);
					printf(" en %s\t\t\t\t\t|\n",bufferR);
					printf("---------------------------------------------------------\n");
				}
				printf("\n");
			}

			if(strcmp(msg,"check_content")==0){
				mensaje = 1;
				it = 0;
				itPer = 0;

				// Obtener tamaño del fichero
				read(sockfd,(unsigned char*) &size,sizeof(unsigned int));

				if(size != 0){
					file = open("temporalData",O_RDWR|O_NOCTTY|O_CREAT|O_TRUNC,0644);
					// Mientras no se haya leido todo el fichero
					while(it < size){
						// Leemos lo que se envio
						bzero(bufferR,256);
						leido = read(sockfd,bufferR,256);
						write(file,bufferR,leido);

						it+=leido;
						itPer++;
					}
					close(file);

					printf("\n---------------------------------------------------------\n");
					printf("|            CONTENIDO DEL DIRECTORIO REMOTO            |\n");
					printf("---------------------------------------------------------\n");
					system("cat temporalData");
					printf("---------------------------------------------------------\n\n");
					system("rm temporalData");
				}
				else{
					printf("Error: Nada que traer\n");
				}
			}

			if(strcmp(msg,"browse")==0 || strcmp(msg,"reset")==0){
				mensaje = 1;
				printf("\n--> Directorio modificado\n\n");
			}

			if(strcmp(msg,"help")==0){
				mensaje = 1;
				printf("\n---------------------------------------------------------\n");
				printf("|	                  AYUDA                         |\n");
				printf("---------------------------------------------------------\n");
				printf("| Comandos:                                             |\n");
				printf("|                                                       |\n");
				printf("| 1) logout - Cierra la sesion.	                        |\n");
				printf("|                                                       |\n");
				printf("| 2) store <fichero> - Almacena	el archivo 'fichero'    |\n");
				printf("| 	del directorio local en el directorio remoto.   |\n");
				printf("|                                                       |\n");
				printf("| 3) fetch <fichero> - Almacena	el archivo 'fichero'    |\n");
				printf("| 	del directorio remoto en el directorio local.   |\n");
				printf("|                                                       |\n");
				printf("| 4) check_users - Muestra qué usuarios están conectados|\n");
				printf("| 	y desde dónde están conectados (IP).            |\n");
				printf("|                                                       |\n");
				printf("| 5) check_content - Muestra el contenido del directorio|\n");
				printf("| 	remoto.                                         |\n");
				printf("|                                                       |\n");
				printf("| 6) browse <directorio> - Cambia el directorio remoto  |\n");
				printf("| 	a 'directorio', alcanzable desde el directorio  |\n");
				printf("| 	remoto actual.                                  |\n");
				printf("| 7) reset - Reinicia el directorio remoto al           |\n");
				printf("| 	directorio inicial.                             |\n");
				printf("|                                                       |\n");
				printf("| 8) local_shell - Entra en modo shell para ejecutar    |\n");
				printf("| 	comandos del sistema.                           |\n");
				printf("---------------------------------------------------------\n\n");
			}

			if(strcmp(msg,"local_shell")==0){
				char msg_shell[256];

				do{
					printf("\n--> Teclee un mensaje: \n\n %s @ %s / local_shell > ",user,direccion);
					bzero(msg_shell,256);
					fgets(msg_shell,256,stdin);

					if(strcmp(msg_shell,"exit_shell\n")!=0){
						printf("\n");
						system(msg_shell);
					}
				}while(strcmp(msg_shell,"exit_shell\n")!=0);
				printf("\n");
			}

			if(mensaje == 0){
				if(strcmp(msg,"logout")!=0)
	    			printf("\n--> Mensaje no válido\n\n");
			}


	 	}while(strcmp(bufferW,"logout")!=0);
    }

    if(strcmp(bufferR,"NO IDENTIFICADO") == 0){
		printf("Conexion denegada: Usuario o contraseña erroneos\n");
    }

    if(strcmp(bufferR,"YA IDENTIFICADO") == 0){
    	printf("Conexion denegada: Ya esta conectado\n");
    }

    close(sockfd);
    return 0;
}

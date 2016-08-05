#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define VERSION 23
#define BUFFER_SIZE 	8096
#define ERROR	    	42
#define LOG         	44
#define PROHIBIDO   	403
#define NOENCONTRADO	404

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{0,0} };

/* Función de la Version 2 para el fichero log que se generara  */
/****************************************************************/
void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
	int fd ;
	char logbuffer[BUFFER_SIZE*2];
	
	switch (log_message_type) {
		case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case PROHIBIDO:
			// Enviar como respuesta 403 Forbidden
			(void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
			(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",message, additional_info);
			break;
		case NOENCONTRADO:
			// Enviar como respuesta 404 Not Found
			(void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
			(void)sprintf(logbuffer,"NOT FOUND: %s:%s",message, additional_info);
			break;
		case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",message, additional_info, socket_fd); break;
	}

	if((fd = open("web_sstt.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(log_message_type == ERROR || log_message_type == NOENCONTRADO || log_message_type == PROHIBIDO) exit(3);
}

/* Función ejecutada por el proceso hijo para procesar la conexion web */
/****************************************************************/
void servidor_web(int fd)
{
	debug(LOG,"request","Ha llegado una peticion",fd); // Version 2

	/* Variables */
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;

	/* Definir buffer para leer las peticiones */
	static char buffer[BUFFER_SIZE+1]; 

	/* Leer la petición HTTP */
	ret =read(fd,buffer,BUFFER_SIZE); 

	/* Comprobación de errores de lectura*/
	if(ret == 0 || ret == -1) {
		debug(PROHIBIDO,"failed to read browser request","",fd);
	}

	/* Si la lectura tiene datos válidos terminar el buffer con un \0 */
	if(ret > 0 && ret < BUFFER_SIZE)
		buffer[ret]=0;		/* terminar el buffer */
	else buffer[0]=0;

	/* Imprimir línea a línea la petición HTTP en pantalla */
	printf("Recibido: %s \n",buffer);
	/* Realizar un "parsing" del HTTP request */
	for(i=0;i<ret;i++)
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	
	/* Si no es un GET rechazar porque solo se permite esta operacion */
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		debug(PROHIBIDO,"solo se permite la operación GET",buffer,fd);
	}
	
	/* Rellenamos con 0's el resto del buffer */
	for(i=4;i<BUFFER_SIZE;i++) { 
		if(buffer[i] == ' ') {
			buffer[i] = 0;
			break;
		}
	}

	/* Comprobación del uso de rutas de directorio padre no permitidas */
	for(j=0;j<i-1;j++) 	
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			debug(PROHIBIDO,"Directorio padre (..) ruta incompatible",buffer,fd);
		}

	/* Si no se especifica el index.html añadirlo por defecto a la petición */
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) 
		(void)strcpy(buffer,"GET /index.html");

	/* Verificar si se soporta el tipo de fichero de la petición. Ver tabla "extensiones" y responder con el codigo de error en caso de que no*/
	buflen=strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) debug(PROHIBIDO,"Tipo de fichero no soportado",buffer,fd);

	/* Abrir el contenido el fichero especificado en la URL*/
	file_fd = open(&buffer[5],O_RDONLY);

	/* Crear una respuesta y enviar los datos en bloques de 8KB */
	if (file_fd == -1) {
		debug(NOENCONTRADO,"No se puede abrir el archivo correctamente",&buffer[5],fd);
		long fileError_fd = open("404.html",O_RDONLY);
		(void)sprintf(buffer,"HTTP/1.0 404 Not Found \r\nContent-Type:\r\n\r\n");
		write(fd,buffer,strlen(buffer));
		while ((ret = read(fileError_fd, buffer, BUFFER_SIZE)) > 0)
	                write(fd,buffer,ret);
	}
	else {
		printf("Fichero abierto correctamente \n");
       		(void)sprintf(buffer,"HTTP/1.0 200 OK\r\nSet-Cookie: name=Luis-Cookie; expires=Sat, 03 May 2025 17:44:22 GMT\r\nContent-Type: %s\r\n\r\n", fstr);
        	write(fd,buffer,strlen(buffer));
		
		/* Enviar el resto del contenido */
	        while ( (ret = read(file_fd, buffer, BUFFER_SIZE)) > 0)
	                write(fd,buffer,ret);
	}
	
	printf("Se ha procesado todo correctamente, hacer nuevas peticiones \n ");
        sleep(1);       /* esperar un poco a que se vacie el buffer */
        exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr; 	/* static = inicializa a ceros */
	static struct sockaddr_in serv_addr; 	/* static = inicializa a ceros */

	if( argc < 3  || argc > 3 ) {
                printf("Uso: web-sstt numero-puerto directorio-web\n");
                printf("No se soporta: URLs que incluyan \"..\", Java, Javascript, CGI\n");
                exit(0);
        }
	
        if(chdir(argv[2]) == -1)
        {
                printf("No se puede cambiar al directorio %s\n",argv[2]);
                exit(4);
        }

	/* Hacer un demonio que no se pueda parar y que no genere zombies (no wait()) */
	if(fork() != 0)
		return 0; 

	(void)signal(SIGCLD, SIG_IGN); /* ignora muertes de procesos hijo */
	(void)signal(SIGHUP, SIG_IGN); /* ignora cuelges de terminal */

	debug(LOG,"web server starting...", argv[1] ,getpid()); // Version 2
	
	//for(i=0;i<32;i++)
		//(void)close(i);		/* cerrar ficheros abiertos */
	//(void)setpgrp();		/* romprer grupo de procesos */
	printf("INFO: iniciando Servidor \n");
	
	
	/* Establecer el socket con el puerto pasado como parámetro */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0){
		debug(ERROR, "system call","socket",0);
		exit(1);
	}
		
	port = atoi(argv[1]);

	/* Comprobación de puertos inválidos */
	if(port < 0 || port >60000){
		debug(ERROR,"Puerto invalido, prueba un puerto de 1 a 60000",argv[1],0);
		exit(1);
	}

	/* Asignaciónde los valores al socket de puerto y de direcciones */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	
	/* Hacer un bind */
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0);
		//printf("Error al hacer bind sobre el socket: %d \n",errno);

	/* Hacer un listen sobre el socket */
	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0);
		//printf("ERROR al hacer el listen \n");

	while(1) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0);
			exit(1);
		}
		else {
			if(pid == 0) { 	/* hijo */
				(void)close(listenfd);
				servidor_web(socketfd); 
			} else { 	/* padre */
				(void)close(socketfd);
			}
		}
	}
}

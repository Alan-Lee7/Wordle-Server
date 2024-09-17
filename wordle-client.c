/* hw4-client.c */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

int main(int argc, char ** argv )
{
 int sd;
    struct addrinfo hints, *res, *p;
    int status;
    const char *hostname = "127.0.0.1"; //Local IP
    const char *port = "8190"; // Port number as a string

    // Prepare the hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET for IPv4, AF_INET6 for IPv6, AF_UNSPEC for both
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Get address information
    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    // Loop through all the results and connect to the first one
    for (p = res; p != NULL; p = p->ai_next) {
        if ((sd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (connect(sd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sd);
            perror("connect");
            continue;
        }
        break; // Successfully connected
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        freeaddrinfo(res); // Free the linked list
        return EXIT_FAILURE;
    }

    freeaddrinfo(res); // Free the linked list

    printf("CLIENT: connecting to server...\n");


  /* The implementation of the application protocol is below... */
int extra = 0, length = 0;
while ( 1 )    /* TO DO: fix the memory leaks! */
{
  int flag = 0;
  char * buffer = calloc( 255, sizeof( char ) );
  if ( fgets( buffer, 255, stdin ) == NULL ) break;
  // if ( strlen( buffer ) != 6 ) { printf( "CLIENT: invalid -- try again\n" ); continue; }
  length = strlen(buffer) - 1;
  *(buffer + length) ='\0';   /* get rid of the '\n' */

  printf( "CLIENT: Sending to server: %s\n", buffer );
  int n = write( sd, buffer, strlen( buffer ) );    /* or use send()/recv() */
  if ( n == -1 ) { perror( "write() failed" ); return EXIT_FAILURE; }

  for (int i = 0; i < (length + extra) / 5; i++) {

    n = read( sd, buffer, 9 );    /* BLOCKING */
    //printf("%d\n", n);
    if ( n == -1 )
    {
      perror( "read() failed" );
      return EXIT_FAILURE;
    }
    else if ( n == 0 )
    {
      printf( "CLIENT: rcvd no data; TCP server socket was closed\n" );
      flag = 1;
      break;
    }
    else /* n > 0 */
    {
      switch ( *buffer )
      {
        case 'N': printf( "CLIENT: invalid guess -- try again" ); break;
        case 'Y': printf( "CLIENT: response: %s", buffer + 3); break;
      }

      short guesses = ntohs( *(short *)(buffer + 1) );
      printf( " -- %d guess%s remaining\n", guesses, guesses == 1 ? "" : "es" );
      if ( guesses == 0 )
      {
          flag = 1;
          break;
          printf( "CLIENT: you lost!\n" );
      }
      int total = 0;
      for (int i = 3; i < 8; i++)
      {
          if (!(*(buffer + i) < 65 || *(buffer + i) > 90))
          {
              total++;
          }
      }
      if (total == 5)
      {
          printf( "CLIENT: you won!\n" );
          flag = 1;
          break;
      }
    }
  }
  extra = (length + extra) % 5;
  if (flag) {break;}
}


  printf( "CLIENT: disconnecting...\n" );

  close( sd );

  return EXIT_SUCCESS;
}
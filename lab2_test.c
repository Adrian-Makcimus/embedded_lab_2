/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);
void *writeCounter(void *);
void *readCounter(void *);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
int count; int valid = 0;


int main()
{


pthread_t thread1, thread2;
pthread_create( &thread1, NULL, &writeCounter, NULL);
pthread_create( &thread2, NULL, &readCounter, NULL);
pthread_join( thread1, NULL);
pthread_join( thread2, NULL);



  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, 8, 0);
  }

  return NULL;
}


void *writeCounter(void *ignore) {
 int i;
 for (i = 0 ; i < 10 ; i++) {
 pthread_mutex_lock(&mutex1);
 while (valid) pthread_cond_wait(&cond1, &mutex1);
 count = i; valid = 1;
 pthread_cond_signal(&cond1);
 pthread_mutex_unlock(&mutex1);
}
return NULL; }
void *readCounter(void *ignore) {
 int done = 0;
 do {
 pthread_mutex_lock(&mutex1);
 while (!valid) pthread_cond_wait(&cond1, &mutex1);
 printf("%d\n", count);
 valid = 0; done = count == 9;
 pthread_cond_signal(&cond1);
 pthread_mutex_unlock(&mutex1);
 } while (!done);
return NULL; }



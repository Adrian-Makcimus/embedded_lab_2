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


const char usb2ns_ascii[57] = {0 , 0 , 0 , 0 ,'a','b','c','d','e','f','g','h','i','j','k',
                              'l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
                              '1','2','3','4','5','6','7','8','9','0', 0 , 0 , 0 ,'\t',' ',
                              '-','=','[',']','\\',0 ,';','\'','`',',','.','/' };
const char usb2s_ascii[57] =  {0 , 0 , 0 , 0 ,'A','B','C','D','E','F','G','H','I','J','K',
                              'L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
                              '!','@','#','$','%','^','&','*','(',')', 0 , 0 , 0 ,'\t',' ',
                              '_','+','{','}','|', 0 ,':','\"','~','<','>','\?' };


pthread_t network_thread, network_thread2;
void *network_thread_f(void *);
void *network_thread_type(void*);
pthread_mutex_t disp_msg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond0 = PTHREAD_COND_INITIALIZER;


int valid = 0;
int done = 0; //1 if true 


  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];
  int input_row = 22;
  int input_col = 0;
  int message_row = 9;
  int message_col = 0;
  char sendBuf[BUFFER_SIZE];


int main()
{



  memset(sendBuf, ' ', BUFFER_SIZE);

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('_', 21, col); 
  }

  fbputs("Hello CSEE 4840 World!", 4, 10);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, &network_thread_f, NULL);
  pthread_create(&network_thread2, NULL,&network_thread_type, NULL);

 

  
  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);
  pthread_join(network_thread,NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
 do{

  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */

  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf(" %s", recvBuf);   
   // fbputs(recvBuf, 8, 0);
  }
  
 //now that we want to access  
  pthread_mutex_lock(&disp_msg_mutex);
  //valid is zero initally
  while(valid){ pthread_cond_wait(&cond0, &disp_msg_mutex);}
  fbputs(recvBuf,message_row,0);
 //this should not wait in any case
  valid = 1;  
  message_row++;
  pthread_cond_signal(&cond1);
  pthread_mutex_unlock(&disp_msg_mutex); } while(!done);
 

 return NULL;

}



void *network_thread_type(void *ignored)
{
while(!done) {
	   libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *) &packet, sizeof(packet),
                              &transferred, 0);

  if (transferred == sizeof(packet)) {
      sprintf(keystate, "%08x %02x %02x", packet.modifiers, packet.keycode[0],
              packet.keycode[1]);
      printf("%s\n", keystate);
      //fbputs(keystate, 12, 0);
      if (packet.keycode[0] != 0 && packet.keycode[0] != 42 && packet.keycode[0] != 40) {
      if (packet.modifiers == 0x02 || packet.modifiers == 0x20){
        fbputchar(usb2s_ascii[packet.keycode[0]], input_row, input_col);
        int idx = (input_row - 22)*64+input_col;
        sendBuf[idx] = usb2s_ascii[packet.keycode[0]];
      }
      else{
        fbputchar(usb2ns_ascii[packet.keycode[0]], input_row, input_col);
        int idx = (input_row - 22)*64+input_col;
        sendBuf[idx] = usb2ns_ascii[packet.keycode[0]];
      }
      input_col++;
      if (input_col == 64){
        input_col = 0;
        input_row++;
      }
      }
      else if (packet.keycode[0] == 0){
        fbputchar(12, input_row,input_col);
      }
      else if (packet.keycode[0] == 42){
        fbputchar(' ', input_row, input_col);
        sendBuf[input_row*64+input_col] = 0;
        input_col--;
        if (input_col < 0) {
          input_col = 63;
          input_row--;
          fbputchar(' ', input_row, input_col);
        }
      }
      else if (packet.keycode[0] == 40) { //enter

	pthread_mutex_lock(&disp_msg_mutex);
	while(!valid) pthread_cond_wait(&cond1, &disp_msg_mutex);

	valid = 0;

        clear_framebuff(22, 0);
        input_row = 22;
        input_col =0;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if(message_row == 21) {
               fb_scroll(BUFFER_SIZE);
               message_row = 19;
            }
            if (sendBuf[i] != ' ') {
              printf("%c\n",sendBuf[i]);
           }
           fbputchar(sendBuf[i], message_row, message_col);
           message_col++;
            if (message_col == 64){
                message_col = 0;
                message_row++;
            }

        }
	
		write(sockfd, &sendBuf, BUFFER_SIZE-1);
	//	pthread_cond_signal(&cond0);
	        pthread_mutex_unlock(&disp_msg_mutex);
     } // end enter


      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
        done == 1;
      } 
    }//if transfered block
	
	} //end of while(!done)

	return NULL;
}

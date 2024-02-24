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

pthread_mutex_t disp_msg_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 
int valid = 0;
int done = 0; //1 if true 

pthread_t network_thread; // Allowcate space for network thread
void *network_thread_f(void *);
void *network_thread_type(void*); 

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];
  int input_row = 17;
  int input_col = 0;

  int received_row = 1;
  int received_col = 0;
   

  char sent_msg[BUFFER_SIZE];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('_',16, col); 
    fbputchar('*', 23, col);
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
  //Sets up thread and starts runnning it in parallel
  // in this case run a function called network_thread_f (see below)
  pthread_create(&network_thread, NULL, network_thread_f, NULL);
  pthread_creat(&network_thread, NULL, network_thread_type, NULL);



  /* Look for and handle keypresses */
  // this probably needs to be its own thread function (see recording near 50min
  // 
/*
  for (;;) {
    } // end of for(;;) loop
*/

   if(done == 1){
    /* Terminate the network thread */
     pthread_cancel(network_thread);

     /* Wait for the network thread to finish */
     pthread_join(network_thread, NULL);
     return 0;
  } 

 
}


//this will happen first since the server does display this first 
// this should should be valid first 
void *network_thread_f(void *ignored)
{
  pthread_mutex_lock(&disp_msg_mutex);
  //valid is  0initally
  while(valid) pthread_cond_wait(&cond0, &disp_msg_mutex); //relinquish control of mutex until cond1 is signaled
  valid = 1;
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    fbputs(recvBuf, received_row, 0); //draw given string recvBuf
  }
  pthread_cond_signal(&cond1);
  pthread_mutex_unlock(&disp_msg_mutex);
  received_row++;
  return NULL;
}





void *network_thread_type(void *ignored)
{ 
    libusb_interrupt_transfer(keyboard, endpoint_address,
                              (unsigned char *) &packet, sizeof(packet),
                              &transferred, 0);

  if (transferred == sizeof(packet)) {
      sprintf(keystate, "%08x %02x %02x", packet.modifiers, packet.keycode[0],
              packet.keycode[1]);
      printf("%s\n", keystate);
      fbputs(keystate, 12, 0);

        // single check for enter 
     if (packet.keycode[0] == 0x28) {

        while(!valid) pthread_cond_wait(&cond1, &disp_msg_mutex);
        fbputs(sent_msg,received_row,0);
        valid = 1;
        pthread_cond_signal(&cond1);
        ptread_mutex_unlock(&disp_msg_mutex);

        sent_msg[BUFFER_SIZE] = "\0";
        continue;
     }



      if (packet.keycode[0] != 0 && packet.keycode[0] != 42) { //button AND not delete
        if (packet.modifiers == 0x02 || packet.modifiers == 0x20){ //shift
         fbputchar(usb2s_ascii[packet.keycode[0]], input_row, input_col);
         strcat(sent_msg,usb2s_ascii[packet.keycode[0]]);
        }
        else{
         fbputchar(usb2ns_ascii[packet.keycode[0]], input_row, input_col);
         strcat(sent_msg,usb2ns_ascii[packet.keycode[0]]);
           }
      input_col++;
      if (input_col == 64){//wrap around 
        input_col = 0;
        input_row++;
      }
      } //end button and not delete if
      else if (packet.keycode[0] == 0){ //no event (displace space)
        fbputchar(12, input_row,input_col);
      }
      else if (packet.keycode[0] == 42){ // delete
        size_t length = strlen(sent_msg);
        sent_msg(length-1) = "\0";


        fbputchar(' ', input_row, input_col);
        input_col--;
        if (input_col < 0) {
          input_col = 63;
          input_row--;
          fbputchar(' ', input_row, input_col);
        }
      } //end delete if 


     if (packet.keycode[0] == 0x29) { /* ESC pressed? */
      	done = 1;
       } // for break statment
     else{
        done = 0;
     }     
    }//end of if (transferred == sizeof(packet)) 
//return 1???
} // end of network_thread_type

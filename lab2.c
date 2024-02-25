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


char usb2ns_ascii[57] = {0 , 0 , 0 , 0 ,'a','b','c','d','e','f','g','h','i','j','k',
                              'l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
                              '1','2','3','4','5','6','7','8','9','0', 0 , 0 , 0 ,'\t',' ',
                              '-','=','[',']','\\',0 ,';','\'','`',',','.','/' };
char usb2s_ascii[57] =  {0 , 0 , 0 , 0 ,'A','B','C','D','E','F','G','H','I','J','K',
                              'L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
                              '!','@','#','$','%','^','&','*','(',')', 0 , 0 , 0 ,'\t',' ',
                              '_','+','{','}','|', 0 ,':','\"','~','<','>','\?' };


void backspace_buffer(char *buf, int size, int idx) {
  char *buf2 = malloc(size);
  for (int i = 0; i < idx; i++) {
    buf2[i] = buf[i];
    printf("%c,",buf[i]);
  }
  for (int i = idx; i < size-1; i++) {
    buf2[i] = buf[i+1];
    printf("%c,",buf[i]);
  }
  buf2[size-1] = '\0';
  memcpy(buf, buf2, size);
  free(buf2);
}

void insert_buffer() {
}



pthread_t network_thread;
void *network_thread_f(void *);

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];
  int input_row = 22;
  int input_col = 0;
  int message_row = 9;
  int message_col = 0;
  char *sendBuf = malloc(BUFFER_SIZE);
 
  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  clear_framebuff(0,0);

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
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  memset(sendBuf, '\0', BUFFER_SIZE);

  uint8_t old_keys[] = {0, 0, 0, 0, 0, 0};
  int keyidx = 0;
  int changed = 0;

  /* Look for and handle keypresses */
  for (;;) {
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);

  if (transferred == sizeof(packet)) {

      sprintf(keystate, "%08x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      printf("%s\n", keystate);

      for (int i = 0; i < 6; i++) {
	if (old_keys[i] != packet.keycode[i]) {
           keyidx = i;
           changed = 1;
           old_keys[i] = packet.keycode[i];
        }
      }

      if (changed && packet.keycode[keyidx] != 0 && packet.keycode[keyidx] != 42 && packet.keycode[keyidx] != 40 && 
          packet.keycode[keyidx] != 79 && packet.keycode[keyidx] != 80 && packet.keycode[keyidx] != 41) { //regular key
        if (packet.modifiers == 0x02 || packet.modifiers == 0x20){ //shift
          fbputinvertchar(usb2s_ascii[packet.keycode[keyidx]], input_row, input_col);
          int idx = (input_row - 22)*64+input_col;
          sendBuf[idx] = usb2s_ascii[packet.keycode[keyidx]];
          printf("%c%d\n", sendBuf[idx], idx);
        }
        else{ //no shift
          fbputchar(usb2ns_ascii[packet.keycode[keyidx]], input_row, input_col);
          int idx = (input_row - 22)*64+input_col;
          sendBuf[idx] = usb2ns_ascii[packet.keycode[keyidx]];
	  printf("%c%d%d\n", sendBuf[idx], idx, sizeof(sendBuf[idx]));
        }
        input_col++;
        if (input_col == 64){
	  input_col = 0;
          input_row++;
        }
      }
      else if (changed && packet.keycode[keyidx] == 0){ //let go or no input
        int idx = (input_row - 22)*64+input_col;
        if (sendBuf[idx] == '\0'){
          fbputchar(12, input_row,input_col);
        }
        else {
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
      }
      else if (changed && packet.keycode[keyidx] == 42 && !(input_col == 0 && input_row == 22)){ //backspace
      	fbputchar(' ', input_row, input_col);
        input_col--;
        if (input_col < 0) {
          input_col = 63;
          input_row--;
          fbputchar(' ', input_row, input_col);
        }
        backspace_buffer(sendBuf, BUFFER_SIZE, (input_row-22)*64+input_col);
     }
     else if (changed && packet.keycode[keyidx] == 79 && !(input_col == 63 && input_row == 23 )) { //right arrow
        int left_input_row = input_row;
        int left_input_col = input_col+ 1;
        if (left_input_col == 64){
	  left_input_col = 0;
          left_input_row++;
        }
        if (sendBuf[(left_input_row-22)*64+left_input_col] != '\0') {
          fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col);
          input_row = left_input_row;
          input_col = left_input_col;
          int idx = (input_row - 22)*64+input_col;
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
        else if (sendBuf[(left_input_row-22)*64+left_input_col] == '\0' && sendBuf[(input_row-22)*64+input_col] != '\0') {
	  fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col);
          input_row = left_input_row;
          input_col = left_input_col;
          fbputchar(12, input_row, input_col);
        }
     }
     else if (changed && packet.keycode[keyidx] == 80  && !(input_col == 0 && input_row == 22)) { //left arrow
        if (sendBuf[(input_row-22)*64+input_col] == '\0') {
          fbputchar(' ', input_row, input_col);
        }
        else {
          fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col);
        }
        input_col--;
        if (input_col < 0) {
          input_col = 63;
          input_row--;
          int idx = (input_row - 22)*64+input_col;
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
     }
     else if (changed && packet.keycode[keyidx] == 40) { //enter
	clear_framebuff(22, 0);
        input_row = 22;
        input_col =0;
        for (int i = 0; i < BUFFER_SIZE; i++) {
	    if(message_row == 21) {
               fb_scroll(BUFFER_SIZE);
               message_row = 19;	
            }
            if (sendBuf[i] != ' ') {
              printf("%c%d\n",sendBuf[i], i);
           }
           if (sendBuf[i] == '\0') {
	      sendBuf[i] = ' ';
           }
           fbputchar(sendBuf[i], message_row, message_col);
	   message_col++;
	    if (message_col == 64){
		message_col = 0;
		message_row++;
	    }
            
        }
      memset(sendBuf, '\0', BUFFER_SIZE);
        
     }
     changed = 0;
     
      if (packet.keycode[keyidx] == 0x29) { /* ESC pressed? */
	break;
      }
    }
  }
  
  free(sendBuf);
  
  /* Terminate the network thread */
  pthread_cancel(network_thread);
  
  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

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



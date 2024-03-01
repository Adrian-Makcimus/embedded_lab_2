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
char usb2cl_ascii[57] =  {0 , 0 , 0 , 0 ,'A','B','C','D','E','F','G','H','I','J','K',
                              'L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
                              '1','2','3','4','5','6','7','8','9','0', 0 , 0 , 0 ,'\t',' ',
                              '-','=','[',']','\\',0 ,';','\'','`',',','.','/' };
char usb2scl_ascii[57] = {0 , 0 , 0 , 0 ,'a','b','c','d','e','f','g','h','i','j','k',
                              'l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
                              '!','@','#','$','%','^','&','*','(',')', 0 , 0 , 0 ,'\t',' ',
                              '_','+','{','}','|', 0 ,':','\"','~','<','>','\?' };



void empty_line(int row, int col) {
  for(int i=col; i < 63; i++) {
    fbputchar(' ', row, i, 255, 255, 255);
  }
} 


void backspace_buffer(char *buf, int size, int idx) {
  idx--;
  char *buf2 = malloc(size);
  for (int i = 0; i < idx; i++) {
    buf2[i] = buf[i];
  }
  for (int i = idx; i < size-1; i++) {
    buf2[i] = buf[i+1];
  }
  buf2[size-1] = '\0';
  memcpy(buf, buf2, size);
  free(buf2);
}

void insert_buffer(char *buf, int size, int idx, char c) {
  char *buf2 = malloc(size);
  for (int i = 0; i < idx; i++) {
    buf2[i] = buf[i];
  }
  buf2[idx] = c;
  for (int i = idx+1; i < size; i++) {
    buf2[i] = buf[i-1];
  }
  memcpy(buf, buf2, size);
  free(buf2);
}

void insert_key(char *buf, char c, int row, int col, int size) {
  if (buf[size - 1] != '\0') { //dont go past 128
    return;
  }
  int idx = (row - 22)*64 + col;
  if (buf[idx] != '\0') { //inserting
    insert_buffer(buf, size, idx, c);
    //print out
    empty_line(row, col);
    int out_row = row;
    int out_col = col;
    
    for (int i = idx; i < size; i ++) {
      if (buf[i] == '\0') {
	    fbputchar(' ', out_row, out_col, 255, 255, 255);
      }
      else {
        fbputchar(buf[i], out_row, out_col, 255, 255, 255);
      }
	  out_col++;
	  if (out_col == 64){
		out_col = 0;
		out_row++;
	  }
    }
    col++;
	  if (col == 64){
		col = 0;
		row++;
	  }

    fbputinvertchar(buf[idx+1],row, col);
  }
  else { //add at end
     fbputchar(c, row, col, 255, 255, 255);
     buf[idx] = c;
     if (idx == 127) {
       fbputinvertchar(buf[idx], row, col);
     }
     else {
       col++;
	   if (col == 64){
	     col = 0;
		 row++;
	   }
       fbputchar(12, row, col, 255, 255, 255);
     }
  }
}

void backspace(char *buf, int row, int col, int size) { 
  int idx = (row - 22)*64 + col;
  if (idx == 0) { //do nothing when at the beginning
    return;
  }
  if (idx == 127 && buf[idx] != '\0') { //backspace at 128 means just deleting the character and whitebox
    buf[idx] = '\0';
    fbputchar(12, row, col, 255, 255, 255);
    return;
  }
  if (buf[idx] == '\0') { //backspace at the end of what you are writing
    backspace_buffer(buf, BUFFER_SIZE, (row-22)*64+col);
    fbputchar(' ', row, col, 255, 255, 255);
    col--;
    if (col < 0) {
      col = 63;
      row--;
    }
    fbputchar(12, row, col, 255, 255, 255);
   }
  else { //backspace somewhere in the middle
    backspace_buffer(buf, BUFFER_SIZE, (row-22)*64+col);
    int out_row = row;
    int out_col = col;
    empty_line(out_row, out_col);
    
    for (int i = (out_row - 22)*64 + out_col; i < size; i ++) {
      if (buf[i] == '\0') {
	    fbputchar(' ', out_row, out_col, 255, 255, 255);
      }
      else {
        fbputchar(buf[i], out_row, out_col, 255, 255, 255);
      }
	  out_col++;
	  if (out_col == 64){
		out_col = 0;
		out_row++;
	  }
    }
    col--;
    if (col < 0) {
      col = 63;
      row--;
    }
    fbputinvertchar(buf[idx-1], row, col);
  }

}

void delete(char *buf, int row, int col, int size) {
    int idx = (row - 22)*64 + col;
    if (buf[idx] == '\0') {
      return;
    } 
    if (idx == size - 2 && buf[size - 1] == '\0') {
      return;
    }
    if (!(row == 23 && col == 63)) {
    col++;
	   if (col == 64){
	     col = 0;
             row++;
	   }
    } 
    backspace(buf, row, col, size);
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
  //int message_row = 9;
  //int message_col = 0;
  char *sendBuf = malloc(BUFFER_SIZE);
 
  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  clear_framebuff(0,0);

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('_', 21, col, 255, 255, 255); 
  }

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
  int capslock = 0;
  int numlock = 0;

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

      if (changed && ((packet.keycode[keyidx] >= 4 && packet.keycode[keyidx] <= 39) || (packet.keycode[keyidx] >= 43 &&
          packet.keycode[keyidx] <= 56))
          /*changed && packet.keycode[keyidx] != 0 && packet.keycode[keyidx] != 42 && packet.keycode[keyidx] != 40 && 
          packet.keycode[keyidx] != 79 && packet.keycode[keyidx] != 80 && packet.keycode[keyidx] != 41*/) { //regular key
        if (!capslock && (packet.modifiers == 0x02 || packet.modifiers == 0x20)){ //shift and no caps lock
          insert_key(sendBuf, usb2s_ascii[packet.keycode[keyidx]], input_row, input_col, BUFFER_SIZE);
        }
        else if(capslock && (packet.modifiers == 0x02 || packet.modifiers == 0x20)) { //caps lock and shift
          insert_key(sendBuf, usb2scl_ascii[packet.keycode[keyidx]], input_row, input_col, BUFFER_SIZE);
        }
        else if(capslock) { //caps lock
          insert_key(sendBuf, usb2cl_ascii[packet.keycode[keyidx]], input_row, input_col, BUFFER_SIZE);
        }
        else{ //no shift and no caps lock
          insert_key(sendBuf, usb2ns_ascii[packet.keycode[keyidx]], input_row, input_col, BUFFER_SIZE);
        }
        if (!(input_col == 63 && input_row == 23) && sendBuf[BUFFER_SIZE-1] == '\0') { //not able to insert at 128
          input_col++;
          if (input_col == 64){
            input_col = 0;
            input_row++;
          }
        }
      }
      else if (changed && packet.keycode[keyidx] == 0){ //let go or no input
        int idx = (input_row - 22)*64+input_col;
        if (sendBuf[idx] == '\0'){
          fbputchar(12, input_row,input_col, 255, 255, 255);
        }
        else {
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
      }
      else if (changed && packet.keycode[keyidx] == 42 && !(input_col == 0 && input_row == 22)){ //backspace
 	int idx = (input_row - 22)*64+input_col;
        char prev = sendBuf[idx];
        backspace(sendBuf, input_row, input_col, BUFFER_SIZE);
        if(!(idx == 127 && sendBuf[idx] == '\0' && prev != '\0')){ //if you backspace at 128, you should insert again at 128
          input_col--;
          if (input_col < 0) {
            input_col = 63;
            input_row--;
          }
        }
      }
     else if (changed && packet.keycode[keyidx] == 76){ //delete
        delete(sendBuf, input_row, input_col, BUFFER_SIZE);
    }
     else if (changed && packet.keycode[keyidx] == 79 && !(input_col == 63 && input_row == 23 )) { //right arrow
        int left_input_row = input_row;
        int left_input_col = input_col+ 1;
        if (left_input_col == 64){
        left_input_col = 0;
          left_input_row++;
        }
        if (sendBuf[(left_input_row-22)*64+left_input_col] != '\0') {
          fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col, 255, 255, 255);
          input_row = left_input_row;
          input_col = left_input_col;
          int idx = (input_row - 22)*64+input_col;
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
        else if (sendBuf[(left_input_row-22)*64+left_input_col] == '\0' && sendBuf[(input_row-22)*64+input_col] != '\0') {
	      fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col, 255, 255, 255);
          input_row = left_input_row;
          input_col = left_input_col;
          fbputchar(12, input_row, input_col, 255, 255, 255);
        }
     }
     else if (changed && packet.keycode[keyidx] == 80  && !(input_col == 0 && input_row == 22)) { //left arrow
        if (sendBuf[(input_row-22)*64+input_col] == '\0') {
          fbputchar(' ', input_row, input_col, 255, 255, 255);
        }
        else {
          fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col, 255, 255, 255);
        }
        input_col--;
        if (input_col < 0) {
          input_col = 63;
          input_row--;
        }
        int idx = (input_row - 22)*64+input_col;
        fbputinvertchar(sendBuf[idx], input_row, input_col);
     }
     else if (changed && packet.keycode[keyidx] == 81  && !(input_row == 23)) { //down arrow
        int idx = (input_row - 22)*64+input_col;
        if (sendBuf[idx+63] != '\0') {
          if(sendBuf[idx+64] == '\0') {
            fbputchar(12, input_row+1, input_col, 255, 255, 255);
          } 
          else {
            fbputinvertchar(sendBuf[idx+64], input_row+1, input_col);
          }
          fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col, 255, 255, 255);
          input_row++;
        }
     }
    else if (changed && packet.keycode[keyidx] == 82  && !(input_row == 22)) { //up arrow
        int idx = (input_row - 22)*64+input_col;
          fbputinvertchar(sendBuf[idx-64], input_row-1, input_col);
          if (sendBuf[idx] == '\0') {
            fbputchar(' ', input_row, input_col, 255, 255, 255);
          }
          else {
             fbputchar(sendBuf[(input_row-22)*64+input_col], input_row, input_col, 255, 255, 255);
          }
          input_row--;
     }
     else if (changed && packet.keycode[keyidx] == 40) { //enter
	clear_framebuff(22, 0);
        input_row = 22;
        input_col =0;
        send(sockfd, sendBuf, strlen(sendBuf), 0);  
        memset(sendBuf, '\0', BUFFER_SIZE); 
     }
     else if (changed && packet.keycode[keyidx] == 57) { //caps lock
       capslock = ~capslock;
     }
     else if (packet.keycode[keyidx] == 41) { /* ESC pressed? */
	break;
      }
     changed = 0;
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
  int message_row = 0;
  int message_col = 0;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    size_t len = 0;
    int joinleave = 0;
    int ip = 0;
        len = strlen(recvBuf);
        int row_scroll = ((len-1)/64) + 1;
        if(message_row == 20 && row_scroll == 2) {
	       fb_scroll(row_scroll-1);
               message_row -= row_scroll-1;	
        }
        if (len > 2 && recvBuf[0] == '#' && recvBuf[1] == '#') {
          joinleave = 1;
        }
        for (int i = 0; i < len; i++) {
	   if(message_row == 21 ) {
               fb_scroll(row_scroll);
               message_row -= row_scroll;	
           }
           if (recvBuf[i] == '\0') {
	      recvBuf[i] = ' ';
           }	
           if (i == 0 && recvBuf[i] == '<' && !ip) {
              ip = 1;
           }
           if (ip) {
               fbputchar(recvBuf[i], message_row, message_col, 50, 250, 50);
           }
           else if (joinleave){
		 fbputchar(recvBuf[i], message_row, message_col, 0, 0, 109);
           }
           else {
		 fbputchar(recvBuf[i], message_row, message_col, 255, 255, 255);
           }
           if (recvBuf[i] == '>' && ip) {
              ip =0;
           }
	   message_col++;
	   if (message_col == 64){
		message_col = 0;
		message_row++;
	   }     
        }
        if (message_col != 0) {
          message_col = 0;
          message_row++;
        }

  }

  return NULL;
}



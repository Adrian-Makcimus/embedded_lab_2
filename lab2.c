/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname   char *sendBuf = malloc(BUFFER_SIZE);(pcy2301)
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

void empty_line(int row, int col) {
  for(int i=col; i < 63; i++) {
    fbputchar(' ', row, i);
  }
} 


void backspace_buffer(char *buf, int size, int idx) {
  idx--;
  char *sendBuf = malloc(BUFFER_SIZE);  char *buf2 = malloc(size);
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
	    fbputchar(' ', out_row, out_col);
      }
      else {
        fbputchar(buf[i], out_row, out_col);
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
     fbputchar(c, row, col);
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
       fbputchar(12, row, col);
     }
  }
}

void backspace(char *buf, int row, int col, int size) { 
  int idx = (row - 22)*64 + col;
  if (idx == 0) { //do nothing when at the beginning
    return;
  }
  if (idx == 127 && buf[idx] != '\0') { //backspace at 128 means just deleting the character and whitebox
    backspace_buffer(buf, BUFFER_SIZE, idx);
    fbputchar(12, row, col);
    return;
  }
  if (buf[idx] == '\0') { //backspace at the end of what you are writing
    backspace_buffer(buf, BUFFER_SIZE, (row-22)*64+col);
    fbputchar(' ', row, col);
    col--;
    if (col < 0) {
      col = 63;
      row--;
    }
    fbputchar(12, row, col);
   }
  else { //backspace somewhere in the middle
    backspace_buffer(buf, BUFFER_SIZE, (row-22)*64+col);
    int out_row = row;
    int out_col = col;
    empty_line(out_row, out_col);
    
    for (int i = out_col; i < size; i ++) {
      if (buf[i] == '\0') {
	    fbputchar(' ', out_row, out_col);
      }
      else {
        fbputchar(buf[i], out_row, out_col);
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



pthread_t read_thread,edit_thread,write_thread;
pthread_mutex_t mut1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_wr = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_wait = PTHREAD_COND_INITIALIZER;



void *network_thread_fwrite(void *);
void *network_thread_fread(void *);
void *network_thread_fenter(void *);

int wait_send = 0;
int done = 0;


int err, col;
struct sockaddr_in serv_addr;

struct usb_keyboard_packet packet;
int transferred;
char keystate[12];
int input_row = 22;
int input_col = 0;
  //int message_row = 9;
  //int message_col = 0;
char *sendBuf;
 


//memset(sendBuf, '\0', BUFFER_SIZE);

uint8_t old_keys[] = {0, 0, 0, 0, 0, 0};
int keyidx = 0;
int changed = 0;

char recvBuf[BUFFER_SIZE];
int n;
int message_row = 9;
int message_col = 0;

size_t len = 0;
int roll_scroll = 0;
int edit = 0; 

int main()
{

char *sendBuf =malloc(BUFFER_SIZE);
memset(sendBuf, '\0', BUFFER_SIZE);
 
  /*     
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
 */

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  clear_framebuff(0,0);

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('_', 21, col); 
  }

 // fbputs("Hello CSEE 4840 World!", 4, 10);

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
  pthread_create(&read_thread, NULL, network_thread_fread, NULL);
  pthread_create(&edit_thread, NULL, network_thread_fenter, NULL);
//  pthread_create(&write_thread, NULL, network_thread_fwrite, NULL);

/*
  memset(sendBuf, '\0', BUFFER_SIZE);

  uint8_t old_keys[] = {0, 0, 0, 0, 0, 0};
  int keyidx = 0;
  int changed = 0;
*/


  
  /* Look for and handle keypresses */ 


  
  /* Terminate the network thread */
//  pthread_cancel(network_thread);
  
  /* Wait for the network thread to finish */
  pthread_join(read_thread, NULL);
 // pthread_join(write_thread,NULL);
  pthread_join(edit_thread,NULL);

  free(sendBuf);
  
  return 0;
}



void *network_thread_fread(void *ignored)
{
do{
 // char recvBuf[BUFFER_SIZE];
 // int n;
  message_row = 9;
  message_col = 0;
  /* Receive data */
 
// pthread_mutex_lock(&mut1);
 

// while(wait_send){ pthread_cond_wait(&cond_wr,&mut1);}
 
 
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    len = 0;
  //  	edit = 0;
  //	pthread_cond_signal(&cond_wait);
  //    	wait_send = 1;	*	


libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);

// if (transferred == sizeof(packet)) {
	//signal other thread to go and block here 
//	while(wait_send) { pthread_cond_wait(&cond_wr,&mut1);  };		
// }  // end of if tranfered = packet

	pthread_mutex_lock(&mut1);

	if(transferred == sizeof(packet)) { pthread_cond_signal(&cond_wait); }
	while(transferred == sizeof(packet)) {  pthread_cond_wait(&cond_wr, &mut1); }

//	if(transferred == sizeof(packet)) { pthread_cond_signal(&cond_wait); } 


        len = strlen(recvBuf);
       int  row_scroll = ((len-1)/64) + 1;
        for (int i = 0; i < len; i++) {
	    if(message_row == 21) {
               fb_scroll(row_scroll);
               message_row -= row_scroll;	
            }
            if (recvBuf[i] != ' ') {
              printf("%c%d\n",recvBuf[i], i);
           }
           if (recvBuf[i] == '\0') {
	      recvBuf[i] = ' ';
           }
           fbputchar(recvBuf[i], message_row, message_col);
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



  }//end of while (n=read())

// pthread_mutex_lock(&mut1);
 //while(wait_send){ pthread_cond_wait(&cond_wr,&mut1);}
   // wait_send = 1;
   // pthread_cond_signal(&cond_wait);
   
 pthread_mutex_unlock(&mut1);

} while(!done);
  return NULL;
} // end of fread



void *network_thread_fenter (void *ignored){
	while(!done) {

//libusb_interrupt_transfer(keyboard, endpoint_address,
//			      (unsigned char *) &packet, sizeof(packet),
//			      &transferred, 0);
//	pthread_mutex_lock(&mut1);
//	while(!wait_send) {pthread_cond_wait(&cond_wait, &mut1); };
  pthread_mutex_lock(&mut1);
  while(transferred != sizeof(packet)) { pthread_cond_wait(&cond_wait, &mut1) ;}

  if (transferred == sizeof(packet)) {
   
//	pthread_mutex_lock(&mut1);
//	while(wait_send) {pthread_cond_wait(&cond_wr, &mut1); };

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
          insert_key(sendBuf, usb2s_ascii[packet.keycode[keyidx]], input_row, input_col, BUFFER_SIZE);
        }
        else{ //no shift
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
          fbputchar(12, input_row,input_col);
        }
        else {
          fbputinvertchar(sendBuf[idx], input_row, input_col);
        }
      }
      else if (changed && packet.keycode[keyidx] == 42 && !(input_col == 0 && input_row == 22)){ //backspace
        backspace(sendBuf, input_row, input_col, BUFFER_SIZE);
        int idx = (input_row - 22)*64+input_col;
        if(!(idx == 127 && sendBuf[idx] == '\0')){ //if you backspace at 128, you should insert again at 128
          input_col--;
          if (input_col < 0) {
            input_col = 63;
            input_row--;
          }
        }
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
        }
        int idx = (input_row - 22)*64+input_col;
        fbputinvertchar(sendBuf[idx], input_row, input_col);
     }
     else if (changed && packet.keycode[keyidx] == 40) { //enter

  

	clear_framebuff(22, 0);
        input_row = 22;
        input_col =0;
        send(sockfd, sendBuf, strlen(sendBuf), 0);  
  
  	memset(sendBuf, '\0', BUFFER_SIZE); 

	
//	wait_send = 0;
//	edit = 1;
//     pthread_cond_signal(&cond_wait);
    
    
    
     }
     changed = 0;
     
      if (packet.keycode[keyidx] == 0x29) { /* ESC pressed? */
	done = 1;
      }
    }

	pthread_cond_signal(&cond_wr);	
	pthread_mutex_unlock(&mut1);


	} //end of while (!done)
	return NULL;
}// end of network_thread_fenter


/*

void *network_thread_fwrite(void * ignored){
 do{
	while(!wait_send) {pthread_cond_wait(&cond_wait, &mut1); }
	if (edit == 1){

	 clear_framebuff(22, 0);
        input_row = 22;
        input_col =0;
        send(sockfd, sendBuf, strlen(sendBuf), 0);  
        memset(sendBuf, '\0', BUFFER_SIZE);


	} //edit =1 
	else {

 	len = strlen(recvBuf);
        int row_scroll = ((len-1)/64) + 1;
        for (int i = 0; i < len; i++) {
	    if(message_row == 21) {
               fb_scroll(row_scroll);
               message_row -= row_scroll;	
            }
            if (recvBuf[i] != ' ') {
              printf("%c%d\n",recvBuf[i], i);
           }
           if (recvBuf[i] == '\0') {
	      recvBuf[i] = ' ';
           }
           fbputchar(recvBuf[i], message_row, message_col);
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

	}//end else for edit == 0

	pthread_cond_signal(&cond_wr);
	wait_send = 0;

	} while(!done);
 	return NULL; //thread termination

} // end of write thread


*/

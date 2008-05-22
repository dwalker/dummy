#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include "UserCmdTables.h"
#include "CmdCommon.h"
#include "LocalCmds.h"
#include "Display.h"
#include "matrix500.h"

// Declare our file descriptor input
int fdi = -1; // the keypad
// And our file descriptor output
int fdo = -1; // The default screen

FILE *outFile = 0;

bool terminateRequested = false;

int menu_state = MENU_START;
int savedOldSettings = -1;
struct termios T_new, T_old;

#define KEY 20091
int semid;

bool processKeyPress(char ch, char &brightness)
{
  bool retValue = false;
  
  switch (ch) {
  case 'B':
    if (brightness < 0x18)
      brightness++;
    setDisplayBrightness(brightness);
    break;
  case 'b':
    if (brightness > 0x11)
      brightness--;
    setDisplayBrightness(brightness);
    break;
  case 'c':
  case 'C':
    sendDisplayCmd(DISPLAY_CLEAR_STRING);
    break;
  case 'd':
  case 'D':
    showDisplayRam(128, 64);
    break;
  case 'l':
  case 'L':
    blinkDisplay();
    break;
  case 'u':
    setDisplayBrightness(0);
    break;
  case 'U':
    setDisplayBrightness(brightness);
    break;
  case 'x':
  case 'X':
    retValue = true;
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    fputc(ch, outFile);
    fflush(outFile);
    break;
  }
  return retValue;
}

void showHelpInformation(const char* progName, const char *optString)
{
	printf("%s is the local command server for the PTF 3207A\n", progName);
	printf("  GPS receiver\n\n");
	printf("Command options:\n");
	int i = 0;
	while (optString[i]) {
		bool terminateLine = false;
		if (optString[i] != ':') {
			printf(" %c", optString[i]);
			terminateLine = true;
		}

		switch(optString[i++]) {
		case 'd':
			printf(" [n]\tUse display #n as the display device.  If 'n' is not present\n\tthe default is display #1.");
			break;
		case 'D':
			printf(" n\tUse serial port #n as the display device.");
			break;
		case 'v':
			printf("\tPrint the version information.");
			break;
		case 'h':
			printf("\tShow this help information.");
			break;
		case 'k':
			printf("\tUse a hardware keyboard as the input device.");
			break;
		case 'K':
			printf(" n\tUse serial port #n as the input device.");
			break;
		case ':':	// Catch this but don't do anything
			break;
		default:
			printf("Uknown option, please pass error to developers.");
		}
		if (terminateLine)
			printf("\n");
	}
	
	exit(1);
}


bool processCommandOptions(int argc, char *argv[])
{
	int ch;
  bool parseError = false;
	const char *optString = "vhD:d::K:k";

  if (argc < 2) {
  	fprintf(stderr, "Error: Must provide options\n");
  	showHelpInformation(argv[0], optString);
  }

  while (!terminateRequested && !parseError && (ch = getopt(argc, argv, optString)) != -1) {
	  switch (ch) {
	  case 'h':
	  	showHelpInformation(argv[0], optString);
	  	break;
	  case 'd':
	    // If they have already opened an input then abort
	    if (fdo != -1) {
	      parseError = true;
	      break;
	    }
	    fdo = open("/dev/display0", O_WRONLY | O_NOCTTY | O_NDELAY);
	    if (fdo == -1) {
	      fprintf(stderr, "open /dev/display0 Failed, errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    setDisplayType(0);
	    break;
	  case 'D':
	    int outPort;
	    char outDriver[40];
	    // If they have already opened an output then abort
	    if (fdo != -1) {
	      parseError = true;
	      break;
	    }
	    // Read the comm port to use for output
	    outPort = atoi(optarg);
	    if (outPort < 0 || outPort > 4) {
	      fprintf(stderr, "Invalid port #, %d\n", outPort);
	      parseError = true;
	      break;
	    }
	    sprintf(outDriver, "/dev/ttyS%d", outPort);
	    fdo = open(outDriver, O_WRONLY | O_NOCTTY | O_NDELAY);
	    if (fdo == -1) {
	      fprintf(stderr, "open %s Failed, errno: %d\r\n", outDriver, errno);
	      parseError = true;
	      break;
	    }
	    if (tcgetattr(fdo, &T_new) != 0) { /*fetch tty state*/
	      fprintf(stderr, "tcgetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    if (savedOldSettings < 0) {
	      T_old = T_new;
	      savedOldSettings = fdo;
	    }
	    /*set   115200bps, n81, RTS/CTS flow control, 
	     ignore modem status lines, 
	     hang up on last close, 
	     and disable other flags*/
	    /*termios functions use to control asynchronous communications ports*/
	  
	    T_new.c_cflag = (B38400 | CS8 | CLOCAL | HUPCL | CRTSCTS);
	    T_new.c_oflag = 0;
	    T_new.c_iflag = 0;
	    T_new.c_lflag = 0;
	    if (tcsetattr(fdo, TCSANOW, &T_new) != 0) {
	      fprintf(stderr, "tcsetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    tcflow(fdo, TCOON);
	    setDisplayType(1);
	    break;
	  case 'r':
	    //int outPort;
	    //char outDriver[40];
	    // If they have already opened an output then abort
	    if (fdo != -1) {
	      parseError = true;
	      break;
	    }
	    // Read the comm port to use for output
	    outPort = atoi(optarg);
	    if (outPort < 0 || outPort > 4) {
	      fprintf(stderr, "Invalid port #, %d\n", outPort);
	      parseError = true;
	      break;
	    }
	    sprintf(outDriver, "/dev/ttyS%d", outPort);
	    fdo = open(outDriver, O_WRONLY | O_NOCTTY | O_NDELAY);
	    if (fdo == -1) {
	      fprintf(stderr, "open %s Failed, errno: %d\r\n", outDriver, errno);
	      parseError = true;
	      break;
	    }
	    if (tcgetattr(fdo, &T_new) != 0) { /*fetch tty state*/
	      fprintf(stderr, "tcgetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    if (savedOldSettings < 0) {
	      T_old = T_new;
	      savedOldSettings = fdo;
	    }
	    /*set   115200bps, n81, RTS/CTS flow control, 
	     ignore modem status lines, 
	     hang up on last close, 
	     and disable other flags*/
	    /*termios functions use to control asynchronous communications ports*/
	  
	    T_new.c_cflag = (B38400 | CS8 | CLOCAL | HUPCL | CRTSCTS);
	    T_new.c_oflag = 0;
	    T_new.c_iflag = 0;
	    T_new.c_lflag = 0;
	    if (tcsetattr(fdo, TCSANOW, &T_new) != 0) {
	      fprintf(stderr, "tcsetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    tcflow(fdo, TCOON);
	    setDisplayType(0);
	    break;
	  case 'k':
	    // If they have already opened an input then abort
	    if (fdi != -1) {
	      parseError = true;
	    }
	    fdi = open("/dev/keypad0", O_RDONLY | O_NOCTTY | O_NDELAY);
	    if (fdi == -1) {
	      fprintf(stderr, "open /dev/keypad0 Failed, errno: %d\r\n", errno);
	      parseError = true;
	    }
	    break;
	  case 'K':
	    int inPort;
	    char inDriver[40];
	    // If they have already opened an input then abort
	    if (fdi != -1) {
	      parseError = true;
	      break;
	    }
	    // Read the comm port to use for input
	    inPort = atoi(optarg);
	    if (inPort < 0 || inPort > 4) {
	      fprintf(stderr, "Invalid port #, %d\n", inPort);
	      parseError = true;
	      break;
	    }
	    sprintf(inDriver, "/dev/ttyS%d", inPort);
	    fdi = open(inDriver, O_RDONLY | O_NOCTTY | O_NDELAY);
	    if (fdi == -1) {
	      fprintf(stderr, "open %s Failed, errno: %d\r\n", inDriver, errno);
	      parseError = true;
	      break;
	    }
	    if (tcgetattr(fdi, &T_new) != 0) { /*fetch tty state*/
	      fprintf(stderr, "tcgetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    if (savedOldSettings < 0) {
	      T_old = T_new;
	      savedOldSettings = fdi;
	    }
	    /*set   115200bps, n81, RTS/CTS flow control, 
	     ignore modem status lines, 
	     hang up on last close, 
	     and disable other flags*/
	    /*termios functions use to control asynchronous communications ports*/
	  
	    T_new.c_cflag = (B38400 | CS8 | CREAD | CLOCAL | HUPCL | CRTSCTS);
	    T_new.c_oflag = 0;
	    T_new.c_iflag = 0;
	    T_new.c_lflag = 0;
	    if (tcsetattr(fdi, TCSANOW, &T_new) != 0) {
	      fprintf(stderr, "tcsetattr failed. errno: %d\r\n", errno);
	      parseError = true;
	      break;
	    }
	    break;
	  default:
	    parseError = true;
	    break;
	  }
  }
  return parseError;
}

int main(int args, char *argv[]) {
  key_t key;
  int shmid;
  char *segptr;
  struct itimerval itimer;
  short  sarray[2];
  struct sembuf operations[2];

  extern void sig_handler(int);
  
  if(signal(SIGINT, SIG_IGN) != SIG_IGN)
    signal(SIGINT, sig_handler);

  /* Install a signal handler to handle SIGALRM */
  if(signal(SIGALRM, SIG_IGN) != SIG_IGN)
    signal (SIGALRM, sig_handler);


  bool parseError = processCommandOptions(args, argv);
  
  semid = semget(KEY, 1, 0666 | IPC_CREAT | IPC_EXCL);

  if (semid < 0) {
    fprintf(stderr, "Unable to obtain semaphore.\n");
    exit(0);
  }

  sarray[0] = 0;
  if( semctl(semid, 0, SETVAL, sarray) < 0)
   {
      fprintf( stderr, "Cannot set semaphore value.\n");
   }
   else
   {
      fprintf(stderr, "Semaphore %d initialized.\n", KEY);
   }

  itimer.it_interval.tv_sec = 1;
  itimer.it_interval.tv_usec = 0;
  itimer.it_value.tv_sec = 1;
  itimer.it_value.tv_usec = 0;
  signal (SIGALRM, sig_handler);

  if (setitimer(ITIMER_REAL, &itimer, 0) == -1) {
    fprintf(stderr, "Unable to setup the timer\n");
  }
  
  if (fdi == -1) {
    fdi = fileno(stdin);
  }

  if (fdo == -1)
    outFile = stdout;
  else
    outFile = fdopen(fdo, "w");

  if (parseError || (fdi == -1) || (outFile == 0)) {
    fprintf(stderr, "Error in I/O specifications\n");
    if (outFile) fclose(outFile);
    else close(fdo);
    if (fdi != -1)
      close(fdi);
    exit(1);
  }

  
  fprintf(stderr, "Command parser started.\n");
  // Get to our shared memory for the menus; if it doesn't exist
  //  then we just bail....
  /* Create unique key via call to ftok() */
  key = ftok("/bin", 'C');

  /* Segment should already exists - try as a client */
  if ((shmid = shmget(key, 0, 0)) == -1) {
    perror("shmget");
    exit(1);
  }

  /* Attach (map) the shared memory segment into the current process */
  if ((segptr = (char *)shmat(shmid, 0, SHM_RDONLY)) == (char *)-1) {
    perror("shmat");
    exit(1);
  }

  menus = (struct menuEntry *)segptr;

  show_visayan_commands = false;

  char *sptr, *cmdPtr;
  char ch;
  int numRead;

  // Input is on fdi, output is on fdo & outFile....

  // Initialize the display
  sendDisplayCmd(DISPLAY_INIT_STRING);
  sendDisplayCmd(DISPLAY_SIZE_2);
  // Clear the main display
  sendDisplayCmd(DISPLAY_CLEAR_STRING);
  showSplash(127, 64);
  
  // Initialize the display
  fprintf(outFile, "\xcHowdy - this is a test\r\n");
  fprintf(outFile, "Howdy - this is another test");
  fprintf(outFile, "\r\nThis is the last test...");
  fflush(outFile);
  fcntl(fdi, F_SETFL, FNDELAY);

  displayBlank(3);

  // Show some information 
  char brightness = 0x18;
  
  while (!terminateRequested) {
    bool menuChange = false;
    numRead = read(fdi, &ch, 1);
    if (numRead  > 0) {
      // Put the screen back at full brightness - the mode sets
      //  the display blanking timeout period
      if (displayBlank(1)) {
        // The screen was previously blanked - eat this keypress
        continue;
      }

      terminateRequested = processKeyPress(ch, brightness);
    }
    
    /* Set the structure passed into the semop() to first wait   */
    /* for the first semval to equal 1, then decrement it to     */
    /* allow the next signal that the client writes to it.       */
    operations[0].sem_num = 0;  /* Operate on the first sem      */
    operations[0].sem_op = -1;  /* Decrement the semval by one   */
    operations[0].sem_flg = IPC_NOWAIT;  /* Don't Wait           */
    if (semop( semid, operations, 1 ) != -1)
    {
      // Call displayBlank() once/second
      displayBlank(0);
    }
  }
  fprintf(stderr, "\nLocal command parsing terminated\n");
  
  fcntl(fdi, F_SETFL, 0);

  //write(fdo, terminateMessage, strlen(terminateMessage));
  // Restore the old settings, if appropriate
  if (savedOldSettings >= 0) {
    if (tcsetattr(savedOldSettings, TCSANOW, &T_old) != 0) {
      fprintf(stderr, "tcsetattr failed. errno: %d\r\n", errno);
    }
  }
  close(fdo);
  close(fdi);
  
  /* Set the timer object value with zero, by using the macro
   * timerclear and
   * set the ITIMER_REAL by calling setitimer.
   * This essentially stops the timer.
   */
  timerclear(&itimer.it_value);
  setitimer (ITIMER_REAL, &itimer, 0);

   if( semctl( semid, 0, IPC_RMID, sarray ) < 0 ) {
    perror("semctl IPC_RMID failed"); exit(-1);
  }
}

void sig_handler(int signo)
{
  struct sembuf operations[2];

  switch (signo) {
    case SIGINT:
      terminateRequested = true;
      break;
    case SIGALRM:
      /* Set the structure passed into the semop() to first wait   */
      /* for the first semval to equal 1, then decrement it to     */
      /* allow the next signal that the client writes to it.       */
      operations[0].sem_num = 0;  /* Operate on the first sem      */
      operations[0].sem_op = 1;   /* Increment the semval by one    */
      operations[0].sem_flg = IPC_NOWAIT;  /* Don't Wait           */
      if (semop( semid, operations, 1 ) == -1)
      {
        fprintf(stderr, "Error writing to semaphore!\n");
      }
      break;
  }
}

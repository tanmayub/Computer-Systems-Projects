/*
 * file:        homework.c
 * description: Skeleton for homework 1
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, Jan. 2012
 * $Id: homework.c 500 2012-01-15 16:15:23Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "elf32.h"
#include "uprog.h"

/***********************************/
/* Declarations for code in misc.c */
/***********************************/

extern void init_memory(void);
extern void do_switch(void **location_for_old_sp, void *new_value);
extern void *setup_stack(void *stack, void *func);
extern int get_term_input(char *buf, size_t len);
extern void init_terms(void);

extern void  **vector;          /* system call vector */

/**************************************/
/* Declaring Global Variables for hw1 */
/**************************************/
struct elf32_ehdr hdr; /* ELF Header structure */
char args[10][128]; /* Arguments to hold for a function */

void *saved_stack_for_prog1; /* variable to save stack_pointer for program 1 i.e. q3prog1 */
void *saved_stack_for_prog2; /* variable to save stack pointer for program 2 i.e. q3prog2 */
void *saved_stack_for_main; /* variable to save stack pointer for main function i.e. required in case of uexit and first switch */
/***********************************************/
/********* Your code starts here ***************/
/***********************************************/

/*
 * Question 1.
 *
 * The micro-program q1prog.c has already been written, and uses the
 * 'print' micro-system-call (index 0 in the vector table) to print
 * out "Hello world".
 *
 * You'll need to write the (very simple) print() function below, and
 * then put a pointer to it in vector[0].
 *
 * Then you read the micro-program 'q1prog' into 4KB of memory starting 
 * at the address indicated in the program header, then execute it,
 * printing "Hello world".
 */
void *load_program_into_mem(char *program_name)
{
     int phcount = 0;
     /* 
      * Getting the file descriptor for program "q1prog" 
      * with appropriate error handling.
      */
     int fd = open(program_name, O_RDONLY);
     if(fd < 0)
     {
         printf("ERROR: Unable to load the binary file %s \n", program_name);
         return 0;
     }
     
     /* 
      * Reading the ELF Header for the program "q1prog"
      * into the ELF Header struct and getting the no
      * of program headers.
      */
     read(fd, &hdr, sizeof(hdr));
     int num_of_header = hdr.e_phnum;
    
     /*
      * Reading the program headers into the struct
      * defined for program headers.
      */ 
     struct elf32_phdr phdrs[num_of_header]; /* ELF Program Header Structure */
     lseek(fd, hdr.e_phoff, SEEK_SET);
     read(fd, &phdrs, sizeof(phdrs));

     /* 
      * Loop through the program headers and for the 
      * program header with p_type as PT_LOAD we load
      * the program into memory from p_offset with len
      * specified by p_filesz
      */
     void *buf;
     for(phcount = 0; phcount < hdr.e_phnum; phcount++)
     {
         if(phdrs[phcount].p_type == PT_LOAD)
         {
            /*
             * Allocating 4096 bytes of memmory at the p_vaddr using mmap
             * for mapping the program into memory.
             */
             buf = mmap(phdrs[phcount].p_vaddr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, fd, 0);
             if (buf == MAP_FAILED) {
                 perror("mmap failed");
             }
             /*
              * Load the program into the address returned by mmap.
              */
             lseek(fd, phdrs[phcount].p_offset, SEEK_SET);
             read(fd, buf, phdrs[phcount].p_filesz);
         }
     }
     return buf;
}
void print(char *line)
{
     /*
      * Calling standard library functions in C i.e. printf
     */
     printf("%s",line);
}

void q1(void)
{
     /*
      * Initializing the vector table with the print function at
      * location 0.
      */
     vector[0] = print;
     load_program_into_mem("q1prog");
   
     /*
      * Setting a pointer to a function called execute
      */ 
     void (*execute)();
     /*
      * Assigning the program entry present in the ELF Header
      * to the function pointer and calling the function to
      * load the print system call.
      */
     execute = hdr.e_entry;
     execute();
}


/*
 * Question 2.
 *
 * Add two more functions to the vector table:
 *   void readline(char *buf, int len) - read a line of input into 'buf'
 *   char *getarg(int i) - gets the i'th argument (see below)

 * Write a simple command line which prints a prompt and reads command
 * lines of the form 'cmd arg1 arg2 ...'. For each command line:
 *   - save arg1, arg2, ... in a location where they can be retrieved
 *     by 'getarg'
 *   - load and run the micro-program named by 'cmd'
 *   - if the command is "quit", then exit rather than running anything
 *
 * Note that this should be a general command line, allowing you to
 * execute arbitrary commands that you may not have written yet. You
 * are provided with a command that will work with this - 'q2prog',
 * which is a simple version of the 'grep' command.
 *
 * NOTE - your vector assignments have to mirror the ones in vector.s:
 *   0 = print
 *   1 = readline
 *   2 = getarg
 */
void readline(char *buf, int len) /* vector index = 1 */
{
    /*
     * Read a line from standard input i.e. commandline.
     * fgets adds the NULL character at the end of the
     * line.
     * It stops if end of stdin is reached, newline char
     * encountered or (len - 1) bytes are read. i.e. 
     * whichever first.
     */
     fgets(buf, len, stdin);
}

char *getarg(int i)		/* vector index = 2 */
{
    /*
     * [i + 1] as the first index stores the command.
     * Return 0 in case of null as check for 0 present
     * in q2prog. 
     */
    if(args[i + 1][0] == '\0')
        return 0;
    else
        return args[i + 1]; 
}

/*
 * split_into_tokens splits the input line which has been read
 * into tokens i.e. split on " " <space>.
*/
void split_into_tokens(char *buf)
{
     char *token;
     int arg_count = 0; // initialize the arg_count to 0
     const char space[2] = " ";
     /* Initializing the args character array with NULL */
     memset(args, '\0', sizeof(args));
     /* Split on the new line character*/
     token = strtok(buf, "\n");
     /* Get the first token in buf */
     token = strtok(token, space);
     while(token != NULL && arg_count < 10)
     {
         /* 
          * Copy the strings from the pointer pointing to the 
          * start of the token till it's length.
          */
         strcpy(args[arg_count], token);
         /* 
          * Keep on generating the tokens untill a NULL is 
          * encountered or the #arguments is more than 10. 
          */
         token = strtok(NULL, space);
         arg_count++;
     }
}
/*
 * Note - see c-programming.pdf for sample code to split a line into
 * separate tokens. 
 */
void q2(void)
{
    //int len = 128;
    //char buf[len];
    /* 
     * Intializing the vector table with print at 0,
     * readLine at index 1 and getarg at index 2
     */
    vector[0] = print;
    vector[1] = readline;
    vector[2] = getarg;

    while (1) {
        int len = 128;
        char buf[len];
        printf("> ");
	/* get a line */
         readline(buf, len);
	/* split it into words */
         split_into_tokens(buf);
	/* if zero words, continue */
         if(getarg(-1) == NULL)
             continue;
	/* if first word is "quit", break */
         if(!strcmp(getarg(-1), "quit"))
             break;
	/* make sure 'getarg' can find the remaining words */
         else
         {
             /* Check if the program can be loaded or else continue */
             if(load_program_into_mem(getarg(-1)))
             {
                 /*
                  * Setting a pointer to a function called execute
                  */
                  void (*execute)();
                 /*
                  * Assigning the program entry present in the ELF Header
                  * to the function pointer and calling the function to
                  * load the function call.
                  */

	          /* load and run the command */
                  execute = hdr.e_entry;
                  execute();
             }
         }
    }
    /*
     * Note that you should allow the user to load an arbitrary command,
     * and print an error if you can't find and load the command binary.
     */
}

/*
 * Question 3.
 *
 * Create two processes which switch back and forth.
 *
 * You will need to add another 3 functions to the table:
 *   void yield12(void) - save process 1, switch to process 2
 *   void yield21(void) - save process 2, switch to process 1
 *   void uexit(void) - return to original homework.c stack
 *
 * The code for this question will load 2 micro-programs, q3prog1 and
 * q3prog2, which are provided and merely consists of interleaved
 * calls to yield12() or yield21() and print(), finishing with uexit().
 *
 * Hints:
 * - Use setup_stack() to set up the stack for each process. It returns
 *   a stack pointer value which you can switch to.
 * - you need a global variable for each process to store its context
 *   (i.e. stack pointer)
 * - To start you use do_switch() to switch to the stack pointer for 
 *   process 1
 */

void yield12(void)		/* vector index = 3 */
{
      /* Switch to program2 i.e. "q3prog2" from program1 i.e. "q3prog1" */ 
      do_switch(&saved_stack_for_prog1, saved_stack_for_prog2);
}

void yield21(void)		/* vector index = 4 */
{
      /* Switch to program1 i.e. "q3prog1" from program2 i.e. "q3prog2" */ 
      do_switch(&saved_stack_for_prog2, saved_stack_for_prog1);
}

void uexit(void)		/* vector index = 5 */
{
      /*
       * Switch to main function from program2 i.e. "q3prog2" 
       * (doesn't matter as uexit can be called from q3prog1 as well)
       * but will simply set the stack pointer for main method
       * execution. 
       */
      do_switch(&saved_stack_for_prog2, saved_stack_for_main);
}

void q3(void)
{
    /* load q3prog1 and q3prog2 into their respective address spaces */
    /* initialize the stack correctly for each process */
    
    /* 
     * Intialize the vector table with all the 4 functions.
     */
      vector[0] = print; //required as q3prog1 and q3prog2 use the print syscall
      vector[3] = yield12;
      vector[4] = yield21;
      vector[5] = uexit;
  
    /* load q3prog1 and q3prog2 into their respective address spaces */
      void *prog1 = load_program_into_mem("q3prog1"); 
      void *prog2 = load_program_into_mem("q3prog2"); 
    /* 
     * Get the stack pointer for both the programs i.e.
     * located at 'program_address_space + 4096(page_size)  - 4(int bytes for SP)'
     */
      saved_stack_for_prog1 = prog1 + 4096 - 4;
      saved_stack_for_prog2 = prog2 + 4096 - 4;

    /* save the stacks for both the process in a separate variable */
      saved_stack_for_prog1 = setup_stack(saved_stack_for_prog1, prog1);
      saved_stack_for_prog2 = setup_stack(saved_stack_for_prog2, prog2);
 
    /* switch to program 1 from main */
      do_switch(&saved_stack_for_main, saved_stack_for_prog1);
}


/***********************************************/
/*********** Your code ends here ***************/
/***********************************************/

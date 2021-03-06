#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

#define CMDNM  0
#define SIGNAL 1
#define SYSTAT 2
#define EXIT   3
#define CD     4
#define PWD    5
#define HB     6

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: cmdnm
//
// DESCRIPTION:
//
// This function gets the command the started a process by accessing
// /proc/<pid>/comm.
//
////////////////////////////////////////////////////////////////////////////////

void * cmdnm_getCmdnm( void * input )
{
  char * buffer = (char *) input;
  FILE * fin = fopen( buffer , "r" );
  if( fin == NULL ) 
    pthread_exit( (void*)&fin );
  
  fscanf( fin , "%1024[^\n]", buffer );
  fclose( fin );
  
  pthread_exit( NULL );
}
   
int cmdnm( char* pid )
{
  FILE* fin = NULL;
  char buffer[1024] = "/proc/";
  int code;
  pthread_t thread;
  
  strncat( buffer , pid , 10 );
  strncat( buffer , "/comm" , 5 );

  code = pthread_create( &thread , NULL , cmdnm_getCmdnm , buffer );
  assert( code == 0 );
  code = pthread_join( thread , NULL );
  assert( code == 0 );

  printf( "Process %s started by: \'%s\'\n" , pid , buffer);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: send_signal
//
// DESCRIPTION:
//
// This function sends a signal to a process using the kill command. It checks
// if the arguments are in the proper ranges, switching them if not.
//
////////////////////////////////////////////////////////////////////////////////

int send_signal( char* arg1 , char* arg2 )
{
  int pid = atoi( arg2 );
  int sig = atoi( arg1 );
  int tmp = 0;
  if( sig > 32 )
  {
    tmp = pid;
    pid = sig;
    sig = tmp;
  }
  if ( kill(pid,sig) )
    fprintf( stderr , "Failed to send signal \'%d\' to \'%d\' \n" , sig , pid );
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: systat
//
// DESCRIPTION:
//
// This function gets some information about the system and displays it for the
// user in stdout. The specific information it provides is as follows:
//  -Linux version and system uptime
//  -Memory Usage: memtotal and memfree
//  -CPU Information: vendor id through cache size
//
////////////////////////////////////////////////////////////////////////////////

//this function will get version info
void * systat_getVersion( void * input )
{
  char * buffer = (char * ) input;
  char line[1024];
  
  FILE * fin = fopen( "/proc/version" , "r" );
  
  if( fin == NULL )
    pthread_exit( (void*) &fin );
  
  fgets( line , 1024 , fin );
  strncpy( buffer , "\n" , 1 );
  strncat( buffer , line , 1024 );
  fclose(fin);
  clearerr(fin);
  pthread_exit( NULL );
}

//this function will get system uptime and idle time
void * systat_getUptime( void * input )
{
  char * buffer = (char * ) input;
  double running,idle;
  FILE * fin = fopen( "/proc/uptime" , "r" );
  
  if( fin == NULL )
    pthread_exit( (void*) &fin );
    
  fscanf( fin , "%lg%lg" , &running , &idle );
  sprintf( buffer , "\nSystem uptime: %f seconds; idle time: %f seconds.\n"
            , running , idle );
            
  fclose( fin );
  clearerr( fin );
  pthread_exit( NULL );
}

//this function will get memfo
void * systat_getMemfo( void * input )
{
  char * buffer = (char *) input;
  FILE * fin = fopen( "/proc/meminfo" , "r" );
  char line[1024];
  
  strncpy( buffer , "\nMemory Usage:\n" , 16);
  
  if( fin == NULL )
    pthread_exit( (void*) &fin );
    
  fgets( line , 1024 , fin );
  strncat( buffer , line , 1024 );
  fgets( line , 1024 , fin );
  strncat( buffer , line , 1024 );
  fclose( fin );
  clearerr( fin );
  pthread_exit( NULL );
}

//this funciton will get cpuinfo
void * systat_getCpuinfo( void * input )
{
  //covert void input into string buffer
  char * buffer = (char *)input;
  char line[1024] = "";
  FILE * fin = fopen( "/proc/cpuinfo" , "r" );
  
  if( fin == NULL )
    pthread_exit( (void*) &fin );
  
  strncpy( buffer , "\nCPU Information:\n" , 26 );
  fgets( line , 1024 , fin );
  strncat( buffer , line , 1024 );
  
  int i;
  for( i = 0 ; i < 8 ; i++ )
  {
    fgets( line , 1024 , fin );
    strncat( buffer , line , 1024 );
  }
  
  fclose( fin );
  clearerr( fin );
    
  pthread_exit( NULL ); 
}

//This function will create threads for each stage of the systat process and 
//will return 0;
int systat()
{
  int  fails = 0;
  int  result_code;
  int  index;
  char buffer_version[1024] = "";
  char buffer_uptime[1024] = "";
  char buffer_memfo[2048] = "";
  char buffer_cpuinfo[10000] = "";
  char *buffer[4] = { buffer_version , buffer_uptime , 
                       buffer_memfo , buffer_cpuinfo };
  pthread_t thread[4];
  
  void * (*func[4])(void *) = { systat_getVersion,
                                systat_getUptime,
                                systat_getMemfo,
                                systat_getCpuinfo };
                             
  for( index = 0 ; index < 4 ; index++ )
  {
    //printf( "In systat, creating thread %d." , index );
    result_code = pthread_create( &thread[index] , NULL , func[index] , 
                                  buffer[index]);
    assert( 0 == result_code );
  }
  
  for( index = 0 ; index < 4 ; index++ )
  {
    result_code = pthread_join( thread[index] , NULL );
    //printf( "In systat, thread %d has completed.\n" , index );
    assert( 0 == result_code );
    printf( "%s" , buffer[index] );
  }
    
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: cd
//
// DESCRIPTION:
//
// This function implements the change directory intrinsic command.
//
////////////////////////////////////////////////////////////////////////////////

int cd( char *path )
{
  struct stat path_info;
  stat(path , &path_info);
  if( S_ISREG(path_info.st_mode) )
  {
    fprintf( stderr, "dsh: cd: %s: Not a directory.\n" , path );
    return -2;
  }
  if( chdir( path ) )
  {
    fprintf( stderr, "dsh: cd: %s: No such directory.\n" , path );
    return -1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: pwd
//
// DESCRIPTION:
//
// This function implements the print working directory intrinsic command.
//
////////////////////////////////////////////////////////////////////////////////

int pwd()
{
  char path[1024] = "";
  getcwd( path , 1024 );
  puts(path);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: hb
//
// DESCRIPTION:
//
// This function will print the current time every 'tinc' (s/ms) until 
// 'tend' (s/ms). Prettry boring
//
// INPUT: 
//
//  int tinc - the time increment
//  int tend - the end time
//  char* tval - the type of unit
//
////////////////////////////////////////////////////////////////////////////////

#define HB_BUFFER_SIZE 1024
struct hb_elapsed_t
{
  //input
  int tinc;
  int tend;
  //shared
  int complete;
  int buffer_pos;
  //output
  struct timespec time_buffer[HB_BUFFER_SIZE];
};

void * hb_ticker( void * input )
{
  //Define variables for thread
  struct timespec Time;
  struct hb_elapsed_t * hb_block = (struct hb_elapsed_t *) input;
  long msec_init;
  time_t sec_init;
  long elapsed = 0;
  long next = hb_block->tinc;
  int tinc = hb_block->tinc;
  int tend = hb_block->tend;
  
  hb_block->buffer_pos = 0;
  clock_gettime( CLOCK_REALTIME , &Time );
  msec_init = Time.tv_nsec/1000000;
  sec_init  = Time.tv_sec;
  
  hb_block->complete = 0;
  do
  {
    hb_block->time_buffer[ hb_block->buffer_pos % HB_BUFFER_SIZE ] = Time;
    hb_block->buffer_pos++;
    //printf( "here\n" );
    //keep checking time until it the right time
    if( elapsed < tend )
      while( elapsed < next )
      {
        clock_gettime( CLOCK_REALTIME , &Time );
        elapsed = 1000 * (Time.tv_sec - sec_init) +
                  (Time.tv_nsec/1000000 - msec_init);
      }
    else
    {
      hb_block->complete = 1;
      pthread_exit(NULL);
    }
    next += tinc;
  }
  while( 1 );
}

int hb( int tinc , int tend , char * tval )
{
  //Verify input, and setup for running with units of s or ms.
  int ms = 0;
  if( !strncmp(tval,"s",2) )
  {
    tinc *= 1000;
    tend *= 1000;
  }
  else if( !strncmp(tval,"ms",2) )
    ms = 1;
  else
  {
    fprintf(stderr,"Unknown units \'%s\', use either \'s\' or \'ms\'." , tval);
    return -1;
  } 
  
  //Define variables
  struct hb_elapsed_t hb_block;
  char buffer[26];
  char millis[5] = ".000";
  long msec;
  int  buffer_read_pos = 0;
  int  code;
  int  i;
  time_t sec;
  struct timespec Time;
  struct tm * time;
  pthread_t thread;
  
  hb_block.tinc = tinc;
  hb_block.tend = tend;
  hb_block.complete = 0;
  
  //make thread
  code = pthread_create( &thread , NULL , hb_ticker , (void *) &hb_block );
  assert( code == 0 );
  
  //While thread is still running
  while( !hb_block.complete ){
  while( buffer_read_pos < hb_block.buffer_pos )
  {
    Time = hb_block.time_buffer[ buffer_read_pos % HB_BUFFER_SIZE ];
    buffer_read_pos++;
    sec = Time.tv_sec;
    msec = Time.tv_nsec/1000000;
    
    //Build output string
    time = localtime( &sec );
    strftime( buffer , 26 , "%H:%M:%S" , time );
    if( ms )
    { 
      strncpy(millis,".000",5);
      for( i = 3 ; i > 0 ; i-- )
      {
        millis[i] += msec%10;
        msec /= 10;
      }
      strncat( buffer , millis , 5 );
    }
    puts(buffer);
  }}
  code = pthread_join( thread , NULL );
  assert( code == 0 );
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: Run
//
// DESCRIPTION:
//
// This function directs the program to run the intrinsic commands.
//
////////////////////////////////////////////////////////////////////////////////

int Run( int cmd_num , int args, char** arg_list )
{
  switch( cmd_num )
  {
    case CMDNM:
      if( args != 2 )
      {
        fprintf(stderr, "cmdnm requires 1 argument: <process_id>\n" );
        return -1;
      }
      return cmdnm( arg_list[1] );
    case SIGNAL:
      if( args != 3 )
      {
        fprintf( stderr,
                "signal requires 2 arguments: <signal_num> <process_id>\n" );
        return -1;
      }
      return send_signal( arg_list[1] , arg_list[2] );
    case SYSTAT:
      return systat();
    case EXIT:
      return 2;
    case CD:
      if( args != 2 )
      {
        fprintf(stderr,
               "cd requires 1 argument: <relative path> or <absolute_path>\n" );
        return -1;
      }
      return cd( arg_list[1] );
    case PWD:
      return pwd();
    case HB:
      if( args != 4 )
      {
        fprintf( stderr, "hb requires 3 arguments: <tinc> <tend> <tval>\n" );
        return -1;
      }
      return hb( atoi(arg_list[1]) , atoi(arg_list[2]) , arg_list[3] );
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION: New_Process
//
// DESCRIPTION:
//
// Creates a new process to run command given on the command line in the
// diagnostic shell.
//
////////////////////////////////////////////////////////////////////////////////

int New_Process( char** arg_list )
{
  pid_t child_pid;
  int status;

  //Duplicate current process to make child process
  child_pid = fork();
  if( child_pid == 0 )
  {
    //If child, try execute
    execvp( arg_list[0] , arg_list );
    //Exit if failed
    exit(-1);
  }
  else
  {
    //Struct that holds usage information
    struct rusage runtime_data;

    //Wait for child process to terminate and get usage information
    wait3( &status , 0 , &runtime_data );

    //If
    if( status == 65280 )
    {
      fprintf(stderr, "dsh: %s: command not found\n" , arg_list[0] );
      return -1;
    }

    printf( "Child_Pid: %d.\n" , (int)child_pid );
    printf( "User time: %d seconds, %d microseconds.\n" ,
      (int) runtime_data.ru_utime.tv_sec , (int) runtime_data.ru_utime.tv_usec);
    printf( "System time: %d second, %d microseconds.\n" ,
      (int) runtime_data.ru_stime.tv_sec , (int) runtime_data.ru_stime.tv_usec);
    printf( "Number of page faults: %d (soft) , %d (hard)\n" ,
      (int) runtime_data.ru_minflt , (int) runtime_data.ru_majflt );
    printf( "Number of page swaps: %d\n" , (int) runtime_data.ru_nswap );
  }
  return 0;
}

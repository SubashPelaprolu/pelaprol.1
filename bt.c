#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

struct queue_node {
  char * path;
  struct queue_node* next_node;
};

static char * err_label = NULL; //for perror
static char * root = NULL;  //top folder to traverse

//Program options
enum OPTION {O_FOLLOW=0, O_BYTES, O_FILETYPE, O_PERMISSIONS, O_LINKS, O_USERID, O_GRPID, O_MTIME};
static int options = 0; //mask for program options

//Queue pointers to first and last nodes
struct queue_node *first = NULL, *last = NULL;

static int queue_is_empty(){
  if(first == NULL){  /* if first is null */
    return 1; /* queue is empty */
  }else{
    return 0;
  }
}

/* Create a node for queue */
static struct queue_node * make_node(char * path){
  struct queue_node * qn = (struct queue_node *) malloc(sizeof(struct queue_node));
  if(qn == NULL){
    perror(err_label);
    exit(1);
  }
  qn->path = strdup(path);
  qn->next_node = NULL;
  return qn;
}

static void enqueue(char * path){
  struct queue_node * qn = make_node(path);
  if(queue_is_empty()){
    first = qn;
  }else{
    last->next_node = qn;
  }
  last = qn;
};

static char * dequeue(){
  char * path = NULL;

  if(queue_is_empty())
    return NULL;

  struct queue_node* qn = first;
  path = qn->path;
  first = first->next_node;
  free(qn);

  if(first == NULL)
    last = NULL;

  return path;
}

//Set the specified option in options
static void set_option(enum OPTION opt){
  options |= (1 << opt);  //raise bit number opt
}

static int is_set_option(enum OPTION opt){
  return (options & (1 << opt)); //check the bit at position opt
}

//get the name of a file, from path
static char* my_basename(const char * path){
  return strrchr(path, '/') + 1;  //find last / and go 1 character after it
}

//Return the unit of file size
static char get_size_unit(unsigned int size){
  int i=0;
  const char units[] = "bKMG?";
  while ((size > 1024) && (i < 5)) {
      size /= 1024;
      i++;
  }
  return units[i];
}

/* Create a string from modification time */
static char * make_timestamp(const time_t mtime){
  struct tm time;
  static char buf[20];

  localtime_r(&mtime, &time);
  strftime(buf, sizeof(buf), "%h %d, %Y", &time);
  return buf;
}

/* Get string of a group id */
static char * gid_to_name(const int gid){
  struct group *grp = getgrgid(gid);
  if(grp == NULL){
    perror(err_label);
    exit(1);
  }
  return grp->gr_name;
}

/* Get string of a user id */
static char * uid_to_name(const int uid){
  struct passwd * pwent = getpwuid(uid);
  if(pwent == NULL){
    perror(err_label);
    exit(1);
  }
  return pwent->pw_name;
}

/* Create the permission string from mode */
static char * mode_to_perms(const mode_t mode){
  static char buf[10];

  strncpy(buf, "---------", 10);

  if(mode & S_IRUSR) buf[0] = 'r';
  if(mode & S_IWUSR) buf[1] = 'w';
  if(mode & S_IXUSR) buf[2] = 'x';

  if(mode & S_IRGRP) buf[3] = 'r';
  if(mode & S_IWGRP) buf[4] = 'w';
  if(mode & S_IXGRP) buf[5] = 'x';

  if(mode & S_IROTH) buf[6] = 'r';
  if(mode & S_IWOTH) buf[7] = 'w';
  if(mode & S_IXOTH) buf[8] = 'x';

  return buf;
}

/* Match the mode to a type letter */
static char mode_to_type(const mode_t mode){
  switch(mode & S_IFMT){
    case S_IFSOCK:return 's';
    case S_IFLNK: return 'l';
    case S_IFREG: return '-';
    case S_IFBLK: return 'b';
    case S_IFCHR: return 'c';
    case S_IFDIR: return 'd';
    case S_IFIFO: return '|';
    default:      return '?';
  }
}

/* Version of stat(), that respects our -L option */
static int my_stat(const char * path, struct stat *st){
  int rv;

  if(is_set_option(O_FOLLOW)){  /* If -L is set */
    rv = stat(path, st);  /* Follow the link, and stat the file it points to */
  }else{
    rv = lstat(path, st); /* Stat the link itself */
  }

  if(rv == -1){
    perror(err_label);
    return -1;
  }
  return 0;
}

/* List one entry */
static int ls_entry(const char * path){

  struct stat st;
  if(my_stat(path, &st) < 0){
    return -1;
  }

  if(is_set_option(O_FILETYPE)){
    printf("%c", mode_to_type(st.st_mode));
  }


  if(is_set_option(O_PERMISSIONS)){
    printf("%s ", mode_to_perms(st.st_mode));
  }


  if(is_set_option(O_LINKS)){
    printf("%i ", (int)st.st_nlink);
  }

	if (is_set_option(O_USERID)){
  	printf("%-10s ", uid_to_name(st.st_uid));
  }


	if(is_set_option(O_GRPID)){
  	printf("%-10s ", gid_to_name(st.st_gid));
  }

  if(is_set_option(O_BYTES)){
    char unit = get_size_unit(st.st_size);
    if(unit != 'b'){
      printf("%5lu%c ", st.st_size, unit);
    }else{
      printf("%6lu ", st.st_size);
    }
  }

  if(is_set_option(O_MTIME)){
  	printf("%s ", make_timestamp(st.st_mtime));
  }

  printf("%s\n", path);

  return 0;
};

/* List one directory */
static int ls_dir(char * dname){

	struct dirent *entry;
	char ename[1000];

  DIR * dirp = opendir(dname);
	if (dirp == NULL) { //if opendir fails
		perror(err_label);
		return -1;
	}

	while ((entry = readdir(dirp)) != NULL) {

    if(entry->d_name[0] == '.'){
      continue;
    }

    snprintf(ename, sizeof(ename), "%s/%s", dname, entry->d_name);
    ls_entry(ename);

    struct stat st;
    if(my_stat(ename, &st) < 0){
      return -1;
    }

    if (S_ISDIR(st.st_mode)){
      //before we add to queue, make sure we can traverse it
      if(S_ISLNK(st.st_mode)){
        if(is_set_option(O_FOLLOW)){
          enqueue(ename);
        }
      }else{
        enqueue(ename);
      }
    }
	}
	closedir(dirp);

  free(dname);

  return 0;
}

/* List using the breadth first logic */
static void breadthfirst(){

  ls_entry(root);
  enqueue(root);
  free(root);

  while(!queue_is_empty()){
    ls_dir(dequeue());
  }
}

/* Set our root */
static void make_root(const int argc, char * const argv[]){

  if(argc == optind){
    root = (char*) malloc(PATH_MAX);
    if(root && (getcwd(root, PATH_MAX) == NULL)){
      free(root);
      root = NULL;
    }
  }else{

    struct stat st;
    if(my_stat(argv[optind], &st) < 0){
      exit(EXIT_FAILURE);
    }

    if (!S_ISDIR(st.st_mode)){
      exit(EXIT_FAILURE);
    }

    root = strdup(argv[optind]);
  }

  if(root == NULL){
    perror(err_label);
    exit(EXIT_FAILURE);
  }
}

/* Set options based on user provided parameters */
static void make_options(const int argc, char * const argv[]){

  //check parameters and set our options
  int option;
  while((option = getopt(argc, argv, "hLdipstgul")) > 0){
    switch(option){
      case 'h':
        printf("Usage: %s [-h] [-L -d -g -i -p -s -t -u | -l] [dirname]\n", my_basename(argv[0]));
        printf("-%c \t %s\n", 'h', "Show help information");
        printf("-%c \t %s\n", 'd', "Show the time of last modification");
        printf("-%c \t %s\n", 'L', "Follow symbolic links");
        printf("-%c \t %s\n", 't', "information on file type");
        printf("-%c \t %s\n", 'p', "permission bits");
        printf("-%c \t %s\n", 'i', "the number of links to file in inode table");
        printf("-%c \t %s\n", 'u', "the uid associated with the file");
        printf("-%c \t %s\n", 'g', "the gid associated with the file");
        printf("-%c \t %s\n", 's', "the size of file in bytes");
        printf("-%c \t %s\n", 'l', "Enables options -tpiugs");
        exit(0);
        break;

      case 'L':
        set_option(O_FOLLOW);
        break;
      case 'd':
        set_option(O_MTIME);
        break;
      case 'i':
        set_option(O_LINKS);
        break;
      case 'p':
        set_option(O_PERMISSIONS);
        break;
      case 's':
        set_option(O_BYTES);
        break;
      case 't':
        set_option(O_FILETYPE);
        break;
      case 'g':
        set_option(O_GRPID);
        break;
      case 'u':
        set_option(O_USERID);
        break;
      case 'l':
        set_option(O_FILETYPE);
        set_option(O_PERMISSIONS);
        set_option(O_LINKS);
        set_option(O_USERID);
        set_option(O_GRPID);
        set_option(O_BYTES);
        break;

      default:
        fprintf(stderr, "Error: Invalid program parameter %c\n", option);
        exit(EXIT_FAILURE);
        break;
    }
  }
}

/* Set the error label for perror(), using our program name */
static void make_err_label(char * prog_path){
  const char * format = "%s: Error";
  const char * prog_name = my_basename(prog_path);
  const int len = strlen(prog_name) + strlen(format) + 1;

  err_label = (char *) malloc(len);
  if(err_label == NULL){
    perror("malloc");
    exit(1);
  }
  snprintf(err_label, len, format, prog_name);
}

int main(const int argc, char * const argv[]){

  make_err_label(argv[0]);
  make_options(argc, argv);
  make_root(argc, argv);

  breadthfirst();

  free(err_label);

  return 0;
}

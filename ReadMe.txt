1) To compile the program : -
  just type make
  $ cd bt/
  $ make

2) To execute the program : -
 specify the options and provide directory name - ./bt [OPTIONS] {DIRNAME}
  $ ./bt -l /tmp/

To, Find the executable named bt, located inside that
directory in which you compiled the module.
To run it simply type:
  ./bt [dirname]

To run command line arguments:
  ./bt [-h] [-L -d -g -i -p -s -t -u | -l] [dirname]
 
  * h Print a help message and exit.											
  * L Follow symbolic links, if any. Default will
    be to not follow symbolic links.
  * t Print information on file type.
  * p Print permission bits as rwxrwxrwx.												
  * i Print the number of links to file in the inode table.
  * u Print the UID associated with the file.
  * g Print the GID associated with the file.
  * s Print the size of file in bytes.
  * d Show the time of last modification.
  * l This option will be used to print information 
    on the file as if the options tpiugs are all specified

3) Version control: -

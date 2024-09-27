#ifndef ptslib_ptsdef_h
#define ptslib_ptsdef_h
#define WIN32
#ifdef WIN32

#include <assert.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <math.h>
#include <process.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__ "("__STR1__(__LINE__)") : Warning Message : "

#else

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/if.h>

#endif

#endif

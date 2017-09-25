#ifndef __PTY__
#define __PTY__

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <string>
#include <signal.h>

#include <pwd.h>

//TODO Should make this class a singleton

struct termios setRawMode(int fd);
struct termios setBashMode(int fd);

enum {SOCKET, PIPE};

class PTY {
public:
	PTY(const std::string &, int mode=PIPE);
	~PTY();

	void initConnection();
	void runProcess(char * command, char *args[]=NULL);

	static void handle_signal(int);
	void stop();

	std::string getFifoIn() const;
	std::string getFifoOut() const;

	static PTY * singleton;
private:
	std::string fifo_path_in;
	std::string fifo_path_out;

	std::string prefix;
	int mode;

	int fd_in;
	int fd_out;

	int fdm;
	int fds;

	int child_pid;
};


#endif

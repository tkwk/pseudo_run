#include "pty.hpp"
#include <iostream>
#include <libgen.h>

PTY * PTY::singleton;

struct termios setRawMode(int fd) {
	struct termios orig_term_settings; // Saved terminal settings
	struct termios new_term_settings; // Current terminal settings
   	tcgetattr(fd, &orig_term_settings);
   	new_term_settings = orig_term_settings;
  	cfmakeraw (&new_term_settings);
  	tcsetattr (fd, TCSANOW, &new_term_settings);
  	return orig_term_settings;
}

struct termios setBashMode(int fd) {
	struct termios orig_term_settings; // Saved terminal settings
	struct termios new_term_settings; // Current terminal settings
   	tcgetattr(fd, &orig_term_settings);
   	new_term_settings = orig_term_settings;
  	cfmakeraw (&new_term_settings);
  	new_term_settings.c_iflag |= (ICRNL|IXON|IUTF8);
  	new_term_settings.c_oflag |= OPOST;
  	new_term_settings.c_lflag |= (IEXTEN|ICANON|ISIG|ECHO);
  	tcsetattr (fd, TCSANOW, &new_term_settings);
  	return orig_term_settings;
}

PTY::PTY(const std::string & pref, int mode) {
	singleton = this;
	signal(SIGPIPE,handle_signal);

	this->mode = mode;
	this->prefix = pref;
	int rc;
	child_pid = 0;
	fdm = posix_openpt(O_RDWR);
	if (fdm < 0)
		fprintf(stderr, "Error %d on posix_openpt()\n", errno);
	rc = grantpt(fdm);
	if (rc != 0)
		fprintf(stderr, "Error %d on grantpt()\n", errno);
	rc = unlockpt(fdm);
	if (rc != 0)
		fprintf(stderr, "Error %d on unlockpt()\n", errno);

	fds = open(ptsname(fdm), O_RDWR);

	//create path to named_pipes if doesnt exist
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	std::string fifo_dir = std::string(homedir)+std::string("/.germinal");

	struct stat st = {0};
	if (stat(fifo_dir.c_str(), &st) == -1) {
    	mkdir(fifo_dir.c_str(), 0700);
	}

	if(prefix.length() == 0)
		prefix = std::string(basename(ptsname(fdm)));

	//create Socket or FIFO

	fifo_path_in = fifo_dir + "/" + prefix + std::string("_in");
	fifo_path_out = fifo_dir + "/" + prefix + std::string("_out");

	if(mode == PIPE) {
		if (stat(fifo_path_in.c_str(), &st) != -1)
    			unlink(fifo_path_in.c_str());
    		if (stat(fifo_path_out.c_str(), &st) != -1)
    			unlink(fifo_path_out.c_str());
	
		mkfifo(fifo_path_in.c_str(), 0600);
		mkfifo(fifo_path_out.c_str(), 0600);
	}
	if(mode == SOCKET) {
		struct sockaddr_un server_in;
		fd_in = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd_in < 0) {
			fprintf(stderr, "Error %d on socket()\n", errno);
		}
		server_in.sun_family = AF_UNIX;
		strcpy(server_in.sun_path, fifo_path_in.c_str());
		if (bind(fd_in, (struct sockaddr *) &server_in, sizeof(struct sockaddr_un))) {
			fprintf(stderr, "Error %d on bind()\n", errno);
		}

		struct sockaddr_un server_out;
		fd_out = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd_out < 0) {
			fprintf(stderr, "Error %d on socket()\n", errno);
		}
		server_out.sun_family = AF_UNIX;
		strcpy(server_out.sun_path, fifo_path_out.c_str());
		if (bind(fd_out, (struct sockaddr *) &server_out, sizeof(struct sockaddr_un))) {
			fprintf(stderr, "Error %d on bind()\n", errno);
		}
	}
}

std::string PTY::getFifoIn() const{
	return fifo_path_in;
}
std::string PTY::getFifoOut() const{
	return fifo_path_out;
}

void PTY::initConnection() {
	if(mode == SOCKET) {
		listen(fd_in, 1);
		accept(fd_in, 0, 0);

		listen(fd_out, 1);
		accept(fd_out, 0, 0);
	}
	if(mode == PIPE) {
		fd_in = open(fifo_path_in.c_str(), O_RDONLY);
		fd_out = open(fifo_path_out.c_str(), O_WRONLY);
	}
	close(0);
	close(1);

	dup(fd_in);
	dup(fd_out);

	close(fd_in);
	close(fd_out);
}

void PTY::runProcess(char * command, char *args[]) {
	int pid;
	int status;
	char input[150];
	char *default_args[] = {command, 0};
	if(args==NULL)
		args = default_args;

	if(pid=fork()) {
		//FATHER
		child_pid = pid;
		struct termios in_orig_term_settings = setRawMode(0);
		struct termios master_orig_term_settings = setRawMode(fdm);

		close(fds);
		fd_set fd_in;

		struct timeval tv;
		
		while(waitpid(-1, &status, WNOHANG) == 0) {
			FD_ZERO(&fd_in);
			FD_SET(0,&fd_in);
			FD_SET(fdm,&fd_in);

			//check for a broken pipe
			write(1,"",1);

			tv.tv_sec = 5;
			tv.tv_usec = 0;
			select(fdm+1, &fd_in, NULL, NULL, &tv);

			if(FD_ISSET(0,&fd_in)) {
				//data to process from stdin
				int count = read(0, input, sizeof(input));
				if(count > 0) {
					write(fdm, input, count);
				}
			}
			if(FD_ISSET(fdm,&fd_in)) {
				//data to process from fdm_out
				int count = read(fdm, input, sizeof(input));
				if(count > 0)
					write(1, input, count);
			}

		}

		tcsetattr(0, TCSANOW, &in_orig_term_settings);
		tcsetattr(fdm, TCSANOW, &master_orig_term_settings);

	}
	else {
		//CHILD
		close(fdm);

  		struct termios slave_orig_term_settings = setBashMode(fds);

  		close(0);
  		close(1);
  		close(2);

  		dup(fds);
  		dup(fds);
  		dup(fds);

  		// Now the original file descriptor is useless
 		close(fds);
 		
 		// Make the current process a new session leader
 		setsid();
 		// As the child is a session leader, set the controlling terminal to be the slave side of the PTY
 		// (Mandatory for programs like the shell to make them manage correctly their outputs)
 		ioctl(0, TIOCSCTTY, 1);

 		execvp(command, args);
 		//whoops
 		exit(1);
	}

}

void PTY::handle_signal(int s) {
	switch(s) {
		case SIGINT:
		case SIGTERM:
		case SIGPIPE:
			singleton->stop();
		break;
		default:
		break;
	}
}

void PTY::stop() {
	unlink(fifo_path_in.c_str());
	unlink(fifo_path_out.c_str());
	int status;
	if(child_pid != 0 && waitpid(child_pid, &status, WNOHANG) == 0) {
		//child alive : kill it
		kill(child_pid,SIGINT);
		if(waitpid(child_pid, &status, WNOHANG) == 0) {
			//give it some time to die
			sleep(1);
			if(waitpid(child_pid, &status, WNOHANG) == 0) {
				kill(child_pid, SIGTERM);
				if(waitpid(child_pid, &status, WNOHANG) == 0) {
					sleep(1);
					if(waitpid(child_pid, &status, WNOHANG) == 0) {
						kill(child_pid, SIGKILL);
					}
				}
			}
		}
	}
}

PTY::~PTY() {
	stop();
}

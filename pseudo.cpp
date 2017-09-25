#include "pty.hpp"
#include <stdio.h>

int main(int argc, char * argv[]) {
	if(argc < 3) {
		printf("Please give a string and a command as an argument \n");
		return 1;
	}
	char * command = argv[2];
	char ** args = &(argv[2]);

	PTY pty(argv[1],PIPE);

	pty.initConnection();
	pty.runProcess(command, args);
	return 0;
}

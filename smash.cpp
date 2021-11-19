#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    struct sigaction act;
    act.sa_flags = SA_RESTART;
    act.sa_sigaction = &alarmHandler;

    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        fprintf(stderr, "smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        fprintf(stderr, "smash error: failed to set ctrl-C handler");
    }
    //if (signal(SIGALRM, alarmHandler) == SIG_ERR) {
    //    fprintf(stderr, "smash error: failed to set alarm");
    //}
    if (sigaction(14, &act, NULL) == ERROR) {
        fprintf(stderr, "smash error: sigaction failed");
    }

    int cnt = 0;
    //TODO: setup sig alarm handler
    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        //std::cout << "the shell pid is: " << getpid() << std::endl;
        std::cout << *(smash.getPPrompt());
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }

    return 0;
}
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    struct sigaction act = { 0 };
    act.sa_flags = SA_RESTART;
    act.sa_sigaction = &alarmHandler;

    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        fprintf(stderr, "smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        fprintf(stderr, "smash error: failed to set ctrl-C handler");
    }
    if (sigaction(SIGALRM, &act, NULL) == ERROR) {
        fprintf(stderr, "smash error: sigaction failed");
    }

    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        std::cout << *(smash.getPPrompt());
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }

    return 0;
}
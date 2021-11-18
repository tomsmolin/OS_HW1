#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
	cout << "smash: got ctrl-Z" << endl;
	SmallShell& smash = SmallShell::getInstance();
	std::string attempt("attempt");
	if (smash.fg_pid != -1)
		smash.job_list.addJob(smash.fg_pid, attempt, true);
	if (kill(smash.fg_pid, SIGSTOP) == -1)
	{
		perror("error");
		return;
	}
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}


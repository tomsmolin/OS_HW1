#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
	cout << "smash: got ctrl-Z" << endl;
	int pid = SmallShell::getInstance().getCurrPid();
	std::string curr_cmd = SmallShell::getInstance().getCurrCmd();
	if (pid != NO_CURR_PID)
	{
		SmallShell::getInstance().getJobs()->addJob(pid, curr_cmd, true);
		if (kill(pid, SIGSTOP) == ERROR)
		{
			fprintf(stderr, "smash error: kill failed\n");
			return;
		}
		cout << "smash: process " << pid << " was stopped" << endl;
		SmallShell::getInstance().resetCurrFgInfo();
	}
}

void ctrlCHandler(int sig_num) {
  cout << "smash: got ctrl-C" <<endl;
  if(SmallShell::getInstance().getCurrPid() != NO_CURR_PID)
  {
    int result = kill(SmallShell::getInstance().getCurrPid(),sig_num);
    if (result == ERROR) {
      fprintf(stderr,"smash error: kill failed\n");
      return;
    }
    int pid = SmallShell::getInstance().getCurrPid();
	SmallShell::getInstance().resetCurrFgInfo();
    cout << "smash: process "<<pid<<" was killed" <<endl;
  }
}


void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}


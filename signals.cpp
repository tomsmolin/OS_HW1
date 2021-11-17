#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
}

void ctrlCHandler(int sig_num) {
  cout << "smash: got ctrl-C" <<endl;
  if(SmallShell::getInstance().getCurrPid() !=-1)
  {
    int result = kill(SmallShell::getInstance().getCurrPid(),sig_num);
    if (result == -1) {
      fprintf(stderr,"smash error: kill failed");
      return;
    }
    int pid = SmallShell::getInstance().getCurrPid();
    SmallShell::getInstance().setCurrPid(-1);
    cout << "smash: process "<<pid<<" was killed" <<endl;
  }
}


void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}


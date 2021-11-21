#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	cout << "smash: got ctrl-Z" << endl;
	int pid = SmallShell::getInstance().getCurrPid();
	if (pid == NOT_SET)
		return;
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

void alarmHandler(int sig_num, siginfo_t* info, void* context) {
	cout << "smash: got an alarm" << endl;
	SmallShell& smash = SmallShell::getInstance();
	std::string cmd = smash.timed_list.front().timeout_cmd;
	int pid = smash.timed_list.front().pid_cmd;
	std::string str("smash: ");
	str.append(cmd).append(" timed out!\n");

	if (kill(pid, SIGKILL) == ERROR)
	{
		fprintf(stderr, "smash error: kill failed\n");
		return;
	}
	smash.timed_list.pop_front();
	smash.timed_list.sort();
	if (!smash.timed_list.empty())
		alarm(difftime(smash.timed_list.front().alrm_time, time(NULL)));
	
	cout << str;
}


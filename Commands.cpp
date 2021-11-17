#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <math.h>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include "signals.h"


using namespace std;
#define ERROR -1
#define LAST_CD "-"
#define MIN_SIG (-35)
#define MAX_SIG (-1)
#define NO_CURR_PID (-1)
#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}
//npos is the maximum value for size_t
string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
//clean all the WHITESPACE from the cmd_line by cleaning from the right side and then from left side
string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str()); //making iss a stream 
  for(std::string s; iss >> s; ) { //iterate all the args one by one 
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1); // put zeros in args[i]
    strcpy(args[i], s.c_str());  // .c_str - is adding /0 at the end
    args[++i] = NULL; 
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

Command::Command(const char* cmd_line) : cmd(cmd_line), argv(0) {
  args = new char*(NULL);
  argv = _parseCommandLine(cmd_line,args);
}

Command::~Command() {
  for(int i=0;i<argv;i++){
    delete args[i];
  }
  delete args;
}

const char* Command::getCmd() {
  return cmd;
}

ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line), jobs(jobs) {}

void ExternalCommand::execute() {
  int pid = fork();
    if (pid == ERROR)
    {
        perror("smash error: fork failed");
        return;
    }
    if(pid == 0)//child
    {
        if(setpgrp() == ERROR) {
            perror("smash error: setpgrp failed");
            return;
        }
        char* cmd_line_char  = (char*)cmd;
        _removeBackgroundSign(cmd_line_char);
        const char *new_args[] = {
                "/bin/bash",
                "-c",
                cmd_line_char,
                NULL
        };

        int result = execv(new_args[0], (char**)new_args);
        if(result == ERROR)
        {
            perror("smash error: execvp failed");
            return;
        }
    }
    /////father
    else{ 
      if(_isBackgroundComamnd(cmd)){
        // char* curr_cmd  = new char;
        // *curr_cmd = *(cmd);
        std::string curr_cmd = cmd;
        jobs->removeFinishedJobs();
        jobs->addJob(pid,curr_cmd);
      }
      else
        {
            SmallShell::getInstance().setCurrPid(pid);
            int result = waitpid(pid,nullptr,WUNTRACED);
            if(result == ERROR) {
              perror("smash error: waitpid failed");
            }
            SmallShell::getInstance().setCurrPid(NO_CURR_PID);
        }
    }
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}

ChangePromptCommand::ChangePromptCommand(const char* cmd_line, string* prompt) : BuiltInCommand(cmd_line), prompt(prompt) {}

void ChangePromptCommand::execute() {
    string def("smash> ");
    if (argv == 1)
        *prompt = def;
    else
    {
        *prompt = args[1];
        (*prompt).append("> ");
    }
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  cout << args[0] << " pid is " << getpid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  char cwd[MAX_CWD_LENGTH]; // make sure it's not suppose to be 80
  if(getcwd(cwd, MAX_CWD_LENGTH) == NULL){
    perror("smash error: getcwd failed");
    return;
  }
  cout << cwd << endl;  
}

static char* getCurrPwd() {
    char* cwd = new char[COMMAND_MAX_LENGTH];
    if (getcwd(cwd, COMMAND_MAX_LENGTH) == NULL)
    {
        perror("smash error: getcwd failed");
        return NULL;
    }

    return cwd;
}


ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), cd_succeeded(false), classPlastPwd(*plastPwd) {}

void ChangeDirCommand::execute() {
    if (argv > 2)
    {
        perror("smash error: cd: too many arguments");
        return;
    }

    char* cwd = getCurrPwd();
    if (cwd == NULL)
    {
        return;
    }

    char* path = args[1];
    if (strcmp(path, "-") == 0)
    {
        if (!classPlastPwd)
        {
            perror("smash error: cd: OLDPWD not set");
            return;
        }
        else
        {
            if (chdir(classPlastPwd) == ERROR)
            {
                perror("smash error: chdir failed");
                return;
            }
        }
    }
    else
    {
        if (chdir(path) == ERROR)
        {
            perror("smash error: chdir failed");
            return;
        }
    }
    cd_succeeded = true;
    classPlastPwd = cwd;
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() {
  jobs->removeFinishedJobs();
  jobs->printJobsList();
}

static bool killFormat(char** args,int argv) {
  if(argv!=3) {
    return false;
  }
  std::stringstream sig_num(args[1]);
  double sig_number=0;
  sig_num >> sig_number;
  bool sig_int = (std::floor(sig_number) == sig_number) ? true : false;
  bool sig_format = (sig_number < MAX_SIG) ? true : false;
  bool sig_exist = (sig_number > MIN_SIG) ? true : false;
  return (sig_format && sig_exist && sig_int);
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
  if(!killFormat(args,argv)) {
      fprintf(stderr,"smash error: kill:invalid arguments\n");
      return;
  }
  std::stringstream job_id(args[2]);
  int id = 0;
  job_id >> id;
  JobsList::JobEntry* curr_job = jobs->getJobById(id);
  if(curr_job == nullptr){
    std::string str = "smash error: kill:job_id "; 
    std::string str2 = args[2];
    std::string str3 = " does not exist\n";
    str.append(str2).append(str3);
    fprintf(stderr,str.c_str());
    curr_job = jobs->getJobById(0);
    return;
  }
  pid_t pid = curr_job->pid;
  int sig_num = 0;
  std::stringstream sig_number(args[1]);
  sig_number >> sig_num;
  sig_num*=-1;
  if (kill(pid, sig_num) == ERROR) {
    perror("smash error: kill failed");
    return;
  }
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
  if (argv<2) {
    exit(1);
  }
  std::cout << "sending SIGKILL signal to " << jobs->jobsDict.size()<< " jobs:\n";
  jobs->printKilledJobList();
  exit(1);
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    int job_id = 0;
    if (argv == 1)
        job_id = jobs->max_job_id;
    else
        job_id = stoi(args[1]);
    
    JobsList::JobEntry* j = jobs->getJobById(job_id);
    int pid = j->pid;
    string job_cmd = j->cmd;
    cout << job_cmd << ": " << pid << endl;

    if (kill(pid, SIGCONT) == ERROR)
    {
        perror("smash error: kill failed");
        return;
    }
    waitpid(pid, NULL, WUNTRACED); // not sure about the optins here
    jobs->removeJobById(job_id);
}

/////////////////////////////joblist//////////////////////

JobsList::JobsList() {
  jobsDict = {};
  max_job_id=0;
  jobs_list_empty=true;
}

JobsList::JobEntry::JobEntry(int pid, int job_id, JobStatus status, time_t insert, std::string cmd): 
pid(pid),job_id(job_id),status(status),insert(insert),cmd(cmd) {};

void JobsList::removeJobById(int jobId){
  jobsDict.erase(jobId);
  maxIdUpdate();
}

void JobsList::addJob(int pid,std::string cmd, bool isStopped) {
  JobStatus curr_status = (isStopped) ?  Stopped : Background;
  jobs_list_empty=false;
  max_job_id++;
  jobsDict[max_job_id] = JobEntry(pid, max_job_id,curr_status,time(NULL),cmd);
}

void JobsList::maxIdUpdate() {
  if(jobs_list_empty){
    max_job_id=0;
  }
  int curr_max=0;
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    if(iter->first>curr_max) {
      curr_max=iter->first;
    }
  }
  max_job_id=curr_max;
}

void JobsList::printJobsList() {
  if(jobsDict.empty()){
    return;
  }
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    std::string end = (iter->second.status==Stopped) ? "(Stopped)\n": "\n";
    double time_diff = difftime(time(NULL),iter->second.insert);
    std::cout << "[" << iter->second.job_id << "]" << iter->second.cmd << ":" << iter->second.pid <<" "<< time_diff << " secs"<< end;
  }
}

void JobsList::printKilledJobList() {
  if(jobsDict.empty()){
    return;
  }
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    std::cout << iter->second.pid << ":" << iter->second.cmd << std::endl;
  }
}

void JobsList::killAllJobs() {
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    if (kill(iter->second.pid, SIGKILL) == ERROR) {
      perror("smash error: kill failed");
      return;
    }
  }
  jobsDict.clear();
  maxIdUpdate();
}

JobsList::JobEntry* JobsList::getJobById(int jobId){
  if (jobsDict.find(jobId) == jobsDict.end()){
    return nullptr;
  }
  return &(jobsDict[jobId]);
}

// JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
void JobsList::removeFinishedJobs() {
  if(jobs_list_empty) {
    return;
  }
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    int status;
    int status_2 = waitpid(iter->second.pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if((WIFEXITED(status) || WIFSIGNALED(status)) && status_2 == iter->second.pid) { //the procces terminated normally or terminated by a signal.
      if(jobsDict.size()== 1){
        jobs_list_empty=true;
        jobsDict.erase(iter->first);
        maxIdUpdate();
        return;
      }
      bool last_iter = (++iter==jobsDict.end()) ? true : false;
      iter--;  
      jobsDict.erase(iter->first);
      maxIdUpdate();
      if(last_iter){
        return;
      }
    }
  }
}

// }
// JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId){

// }
JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    map<int, JobEntry>::reverse_iterator iter;
    for (iter = jobsDict.rbegin(); iter != jobsDict.rend(); iter++)
    {
       if(iter->second.status == Stopped) {
           *jobId = iter->first;
           return &(iter->second);
       }
    }
    return nullptr;
}


SmallShell::SmallShell() : plastPwd(NULL), first_legal_cd(true), prompt("smash> "), job_list(JobsList()) {
    // TODO: add your implementation
    plastPwd = new char* ();
    *plastPwd = NULL;
}


SmallShell::~SmallShell() {
    // TODO: add your implementation
    if (*plastPwd)
        delete[] * plastPwd;

    delete plastPwd;
    
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
  std::string cmd_s = _trim(string(cmd_line));
  std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
   
  if (firstWord.compare("chprompt") == 0) {
     //TODO: add implementation
    return new ChangePromptCommand(cmd_line, getPPrompt());
   }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, plastPwd);
  }
  else if (firstWord.compare("jobs") == 0)
  {
    return new JobsCommand(cmd_line,&job_list);
  }
  else if (firstWord.compare("kill") == 0)
  {
    return new KillCommand(cmd_line,&job_list);
  }
    else if (firstWord.compare("quit") == 0)
  {
    return new QuitCommand(cmd_line,&job_list);
  }

  else if (firstWord.compare("fg") == 0)
  {
      return new ForegroundCommand(cmd_line, &job_list);
  }
  // else {
  //   return new ExternalCommand(cmd_line);
  // }
  /////external commands:
  return new ExternalCommand(cmd_line,&job_list);
  
  return nullptr;
}

void SmallShell::setPLastPwd(Command* cmd) {

    if (strcmp(cmd->args[0], "cd") == 0)
    {
        ChangeDirCommand* temp = (ChangeDirCommand*)cmd;
        if (temp->cd_succeeded)
        {
            if (first_legal_cd)
            {
                *plastPwd = NULL;
                first_legal_cd = false;
                delete[] temp->classPlastPwd;
            }
            else
            {
                if (*plastPwd)
                    delete[] * plastPwd;

                *plastPwd = (temp->classPlastPwd);
            }
        }
    }
}
void SmallShell::setCurrPid(int pid) {
  curr_pid=pid;
}
int SmallShell::getCurrPid() {
  return curr_pid;
}

void SmallShell::executeCommand(const char* cmd_line) {
    // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);
    job_list.removeFinishedJobs();
    cmd->execute();
    setPLastPwd(cmd);
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
#include <iostream>
#include <stdexcept> 
#include <vector>
#include <sstream>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <algorithm>
#include <iomanip>
#include <fcntl.h>
#include "Commands.h"
#include "signals.h"


using namespace std;
#define LAST_CD "-"
#define MIN_SIG (-35)
#define MAX_SIG (-1)
#define NEGATIVE (-1)
#define N (10)
#define NO_CURR_JOBS (0)
#define BUFFER_SIZE (1024)
#define EMPTY_STRING ("")
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

bool _isBackgroundComamnd(const std::string cmd_line) {
  //const string str(cmd_line);
  return /*str*/cmd_line[cmd_line.find_last_not_of(WHITESPACE)] == '&';
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

static int numberOfArgs(std::string cmd_line) {
    int cnt = 0;
    while (cmd_line.size() > 0)
    {
        int arg_prefix = cmd_line.find_first_not_of(WHITESPACE);
        if (arg_prefix >= 0)
            cmd_line.erase(0, arg_prefix);
        else
            break;
        cnt++;

        int whitespace = cmd_line.find_first_of(WHITESPACE);
        if (whitespace >= 0)
            cmd_line.erase(0, whitespace);
        else
            break;
    }
    return cnt;
}

// TODO: Add your implementation for classes in Commands.h 
Command::Command(const char* cmd_line) : cmd(cmd_line), argv(0), timed_entry(NULL), cmd_job_id(NOT_SET) {  
  args = new char*[numberOfArgs(cmd_line) + 1]; //buffer of (+1) due to impl. of _parse command
  argv = _parseCommandLine(cmd_line,args);
}

Command::~Command() {
  for(int i=0;i<argv;i++){
      if (args[i] != NULL)
          free((char*)args[i]);
    args[i] = NULL;
  }
  delete[] args;
  args = NULL; //VALGRIND
  //cmd = NULL; //VALGRIND
}

const char* Command::getCmd() {
  return cmd;
}

int Command::getCmdJobId() {
    return cmd_job_id;
}

void Command::cleanCmdJobId() {
    cmd_job_id = NOT_SET;
}

TimedCommandEntry::TimedCommandEntry(time_t alrm_time, std::string timeout_cmd, int pid_cmd) 
: alrm_time(alrm_time), timeout_cmd(timeout_cmd), pid_cmd(pid_cmd) {}

bool TimedCommandEntry::operator<(TimedCommandEntry const& entry2) {
    /*if (alrm_time < entry2.alrm_time)
        return true;
    else
        return false;*/
    return alrm_time < entry2.alrm_time;
}

bool TimedCommandEntry::operator==(TimedCommandEntry const& entry2) {
    return pid_cmd == entry2.pid_cmd;
}

void TimedCommandEntry::setTimeoutCmd(const char* cmd_line) {
    timeout_cmd = cmd_line;
}

ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line), jobs(jobs) {}

void ExternalCommand::execute() {
  int pid = fork();
    if (pid == ERROR)
    {
        fprintf(stderr, "smash error: fork failed\n");
        return;
    }
    if(pid == 0)//child
    {
        if(setpgrp() == ERROR) {
            fprintf(stderr, "smash error: setpgrp failed\n");
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
            fprintf(stderr, "smash error: execvp failed\n");
            return;
        }
    }
    /////father
    else {
        if (this->timed_entry != NULL)
        {
            this->timed_entry->pid_cmd = pid;
            this->timed_entry = NULL;
        }
        std::string curr_cmd = cmd;
        if(_isBackgroundComamnd(curr_cmd))
        {
            cmd_job_id = jobs->addJob(pid,curr_cmd);
        }
        else
        {
            SmallShell::getInstance().setCurrPid(pid);
            SmallShell::getInstance().setCurrCmd(curr_cmd);
            int status = 0;
            int result = waitpid(pid, &status, WUNTRACED);
            if(result == ERROR) {
                fprintf(stderr, "smash error: waitpid failed\n");
            }
            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                std::list<TimedCommandEntry>& list = SmallShell::getInstance().timed_list;
                std::list<TimedCommandEntry>::iterator it;
                it = find(list.begin(), list.end(), TimedCommandEntry(0, "", pid));
                if (it != list.end())
                { //timeout 6 ../os1-tests-master/./my_sleep 4

//                    list.erase(it);
//                    if (list.empty())
//                        alarm(0);
//                    else
//                    {
//                        int next_alrm = list.front().alrm_time;
//                        alarm(difftime(next_alrm, time(NULL)));
//                    }
                       it->alrm_time = EXITED;
                }
            }
            SmallShell::getInstance().resetCurrFgInfo();
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
  cout << "smash pid is " << getpid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  char cwd[MAX_CWD_LENGTH]; // make sure it's not suppose to be 80
  if(getcwd(cwd, MAX_CWD_LENGTH) == NULL){
    fprintf(stderr, "smash error: getcwd failed\n");
    return;
  }
  cout << cwd << endl;  
}

static char* getCurrPwd() {
    char* cwd = new char[COMMAND_MAX_LENGTH];
    if (getcwd(cwd, COMMAND_MAX_LENGTH) == NULL)
    {
        fprintf(stderr, "smash error: getcwd failed\n");
        return NULL;
    }

    return cwd;
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), cd_succeeded(false), classPlastPwd(*plastPwd) {}

ChangeDirCommand::~ChangeDirCommand() {}

void ChangeDirCommand::execute() {
    if(argv <2 ){ // for cd without arguments
      return;
    }
    if (argv > 2)
    {
        fprintf(stderr, "smash error: cd: too many arguments\n");
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
        if (classPlastPwd == NULL) // When last working directory isn't set on smash
        {
            classPlastPwd = cwd;
            fprintf(stderr, "smash error: cd: OLDPWD not set\n");
            delete[] cwd; //No old pwd is set - therefore the smash won't rec. this mem.
            return;
        }
        else
        {
            if (chdir(classPlastPwd) == ERROR)
            {
                fprintf(stderr, "smash error: chdir failed\n");
                return;
            }
        }
    }
    else
    {
        if (chdir(path) == ERROR)
        {
            fprintf(stderr, "smash error: chdir failed\n");
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
  std::stringstream sig_num(args[1]);
  double sig_number=0;
  sig_num >> sig_number;
  bool sig_int = (std::floor(sig_number) == sig_number) ? true : false;
  bool sig_format = (sig_number < MAX_SIG) ? true : false;
  bool sig_exist = (sig_number > MIN_SIG) ? true : false;
  return (sig_format && sig_exist && sig_int);
}

static bool backAndForegroundFormat(char** args, int argv) {
    if (argv != 2) {
        return false;
    }
    std::stringstream id(args[1]);
    double job_id = 0;
    id >> job_id;
    bool id_int = (std::floor(job_id) == job_id) ? true : false;
    bool id_format = (job_id > 0) ? true : false;
    return (id_format && id_int);
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
    if(argv!=3) {
    fprintf(stderr,"smash error: kill: invalid arguments\n");
    return;
  }
  if(!killFormat(args,argv)) { // as said in piazza invalid sig_num => syscall failed
      fprintf(stderr, "smash error: kill failed\n"); 
      return;
  }
  std::stringstream job_id(args[2]);
  int id = 0;
  job_id >> id;
  JobsList::JobEntry* curr_job = jobs->getJobById(id);
  if(curr_job == nullptr){
    std::string str = "smash error: kill: job-id "; 
    std::string str2 = args[2];
    std::string str3 = " does not exist\n";
    str.append(str2).append(str3);
    fprintf(stderr,str.c_str());
    str = args[2]; //preventing from free invalid pointer
    return;
  }
  pid_t pid = curr_job->pid;
  int sig_num = 0;
  std::stringstream sig_number(args[1]);
  sig_number >> sig_num;
  sig_num*=-1;
  if (kill(pid, sig_num) == ERROR) {
    fprintf(stderr, "smash error: kill failed\n");
    return;
  }
  printf("signal number 9 was sent to pid %d\n",pid);
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
  if (argv<2) {
    delete this;
    exit(1);
  }
  std::cout << "smash: sending SIGKILL signal to " << jobs->jobsDict.size()<< " jobs:\n";
  jobs->printKilledJobList();
  delete this;
  exit(1);
}

JobsList::JobEntry* ForegroundCommand::setCurrJobToForeground() {
     int job_id = 0;
     JobsList::JobEntry* curr_job = NULL;
     if (argv == 1)
     {
         job_id = jobs->max_id;
         if (jobs->max_id == NO_CURR_JOBS)
         {
             fprintf(stderr, "smash error: fg: jobs list is empty\n");
             return NULL;
         }
         return jobs->getJobById(job_id);
     }
     else
     {
         if (!backAndForegroundFormat(args, argv)) {
             fprintf(stderr, "smash error: fg: invalid arguments\n");
             return NULL;
         }
         stringstream id(args[1]);
         id >> job_id;
         curr_job = jobs->getJobById(job_id);
         if (curr_job == NULL)
         {
             std::string str1("smash error: fg: job-id ");
             std::string str2(args[1]);
             str1.append(str2).append(" does not exist\n");
             fprintf(stderr, str1.c_str());
             return NULL;
         }
         return jobs->getJobById(job_id);
     }
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() { 
    JobsList::JobEntry* curr_job = setCurrJobToForeground();
    if (curr_job == NULL)
        return;

    int pid = curr_job->pid;
    string job_cmd = curr_job->cmd;
    cout << job_cmd << " : " << pid << endl;
    if (kill(pid, SIGCONT) == ERROR)
    {
        fprintf(stderr, "smash error: kill failed\n");
        return;
    }
    int status = 0;
    SmallShell::getInstance().setCurrPid(pid);
    SmallShell::getInstance().setCurrCmd(job_cmd);
    SmallShell::getInstance().setCurrFgFromJobs(curr_job->job_id);
    int result = waitpid(pid, &status, WUNTRACED);
    if (result == ERROR)
    {
        fprintf(stderr, "smash error: waitpid failed\n");
        return;
    }
    if (WIFSTOPPED(status))
            return;

    jobs->removeJobById(curr_job->job_id);
}

JobsList::JobEntry* BackgroundCommand::setCurrJobToBackground() {
    int job_id = 0;
    if (argv == 1)
    {
        if (jobs->getLastStoppedJob(&job_id) == nullptr)
        {
            fprintf(stderr, "smash error: bg: there is no stopped jobs to resume\n");
            return NULL;
        }
        return jobs->getJobById(job_id);
    }
    else
    {
        std::stringstream id(args[1]);
        id >> job_id;
        if (!backAndForegroundFormat(args, argv)) {
            fprintf(stderr, "smash error: bg: invalid arguments\n");
            return NULL;
        }
        map<int, JobsList::JobEntry>::iterator it = jobs->jobsDict.find(job_id);
        std::string str1("smash error: bg: job-id ");
        std::string str2 = args[1];
        if (it == jobs->jobsDict.end())
        {
            std::string str3(" does not exist\n");
            str1.append(str2).append(str3);
            fprintf(stderr, str1.c_str());
            return NULL;
        }
        if (it->second.status == Background)
        {
            std::string str4(" is already running in the background\n");
            str1.append(str2).append(str4);
            fprintf(stderr, str1.c_str());
            return NULL;
        }
        return &it->second;
    }
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void BackgroundCommand::execute() {

    JobsList::JobEntry* curr_job = setCurrJobToBackground();
    if (curr_job == NULL)
    {
        return;
    }
    std::string job_cmd = curr_job->cmd;
    int pid = curr_job->pid;
    cout << job_cmd << " : " << pid << endl;
    if (kill(pid, SIGCONT) == ERROR)
    {
        fprintf(stderr, "smash error: kill failed\n");
        return;
    }
    jobs->removeJobById(curr_job->job_id);
    jobs->addJob(pid, job_cmd);
}

HeadCommand::HeadCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

int HeadCommand::setLinesNum() {
    if (argv == 2)
    {
        return N;
    }
    try {
        int num = stoi(args[1]);
        return NEGATIVE*num;

    } catch (std::exception& e) {
        // This error wasn't mentioned in the ex.
        fprintf(stderr, "smash error: head: invalid arguments\n");
        return ERROR;
    }
}

void HeadCommand::execute() {
    if (argv == 1) {
        fprintf(stderr, "smash error: head: not enough arguments\n");
        return;
    }
    int lines_num = setLinesNum();
    if (lines_num == ERROR)
        return;

    int file_index = 2;
    if (argv == 2)
        file_index = 1;

    std::ifstream ifs(args[file_index], std::ifstream::in); //Constructor opens the file
    if (ifs.fail())
    {
        fprintf(stderr, "smash error: open failed: No such file or directory\n");
        return;
    }
    std::string str;
    int w_result = 0;
    while (lines_num > 0)
    {
        std::getline(ifs, str);
        if (ifs.eof())
        {
            if (str.compare("") != 0)
            {
                w_result = write(1, str.c_str(), str.size());
                if (w_result == ERROR)
                {
                    fprintf(stderr, "smash error: write failed\n");
                    return;
                }
                if (w_result < str.size())
                {
                    fprintf(stderr, "write wasn't able to write all bytes\n");
                    return;
                }
            }
            break;
        }
        if (ifs.bad() || ifs.fail())
        {
            fprintf(stderr, "smash error: read failed\n");
            return;
        }
        str.append("\n");
        w_result = write(1, str.c_str(), str.size());
        if (w_result == ERROR) {
            fprintf(stderr, "smash error: write failed\n");
            return;
        }
        if (w_result < str.size())
        {
            fprintf(stderr, "write wasn't able to write all bytes\n");
            return;
        }
        lines_num--;
    }
    ifs.clear();
    ifs.close();
    if (ifs.fail())
    {
        fprintf(stderr, "smash error: close failed\n");
        return;
    }
}

/////////////////////////////joblist//////////////////////

JobsList::JobsList() {
  jobsDict = {};
  max_id=0;
  jobs_list_empty=true;
}

JobsList::JobEntry::JobEntry(int pid, int job_id, JobStatus status, time_t insert, std::string cmd): 
pid(pid),job_id(job_id),status(status),insert(insert),cmd(cmd) {};

void JobsList::removeJobById(int jobId){
  jobsDict.erase(jobId);
  maxIdUpdate();
}

int JobsList::addJob(int pid,std::string cmd, bool isStopped) {
  maxIdUpdate();
  removeFinishedJobs();
  JobStatus curr_status = (isStopped) ?  Stopped : Background;
  jobs_list_empty=false;
  max_id++;
  jobsDict[max_id] = JobEntry(pid, max_id,curr_status,time(NULL),cmd);
  return max_id;
}

void JobsList::maxIdUpdate() {
  if(jobs_list_empty){
    max_id=0;
    return;
  }
  int curr_max = 0;
  // int i = 1;
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    if(iter->first>curr_max) {
      curr_max=iter->first;
    }
    // if(i != iter->first){
    //   max_id = i;
    //   return;
    // }
    // i++;
  }
  max_id = curr_max;
}

void JobsList::printJobsList() {
  if(jobsDict.empty()){
    return;
  }
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    std::string end = (iter->second.status==Stopped) ? " (stopped)\n": "\n";
    double time_diff = difftime(time(NULL),iter->second.insert);
    std::cout << "[" << iter->second.job_id << "]" << " " << iter->second.cmd << " : " << iter->second.pid <<" "<< time_diff << " secs"<< end;
  }
}

void JobsList::printKilledJobList() {
  if(jobsDict.empty()){
    return;
  }
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    std::cout << iter->second.pid << ": " << iter->second.cmd << std::endl;
  }
}

void JobsList::killAllJobs() {
  map<int, JobEntry>::iterator iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    if (kill(iter->second.pid, SIGKILL) == ERROR) {
      fprintf(stderr, "smash error: kill failed\n");
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

void JobsList::removeFinishedJobs() {
  if(jobs_list_empty) {
    return;
  }
  
  map<int, JobEntry>::iterator iter;
  map<int, JobEntry>::iterator temp_iter;
  for (iter = jobsDict.begin(); iter != jobsDict.end(); iter++) {
    int status;
    int status_2 = waitpid(iter->second.pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    // cout << "dgb" << iter->second.pid << endl;
    // cout << "dgb2 " << status_2 << endl;
    if(((WIFEXITED(status) || WIFSIGNALED(status)) && status_2 == iter->second.pid) || kill(iter->second.pid, 0) != 0) { //the procces terminated normally or terminated by a signal.
      // cout << "DGB" << endl;
      if(jobsDict.size()== 1){
        jobs_list_empty=true;

        /* When we're traversing a map with a loop and an iterator \
         * deleting some element could be problematic and "confusing" \
         * for the iterator. Therefore, we need to somehow make sure it reaches \
         * the next element properly. for ex, with this implementation (There are more elegant ways though): */
        temp_iter = ++iter;
        jobsDict.erase((--iter)->first);
        iter = temp_iter;


        maxIdUpdate();
        return;
      }
      bool last_iter = (++iter==jobsDict.end()) ? true : false; // TO ASK: why is this necessary?
      iter--;

      /* same note from above* */
      temp_iter = ++iter;
      jobsDict.erase((--iter)->first);
      iter = temp_iter;

      maxIdUpdate();
      if(last_iter){
        return;
      }
    }
    // cout << "endll" << endl;
  }
  // cout << "end" << endl;
}

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

/////////////////////////////end of joblist//////////////////////

SmallShell::SmallShell() : plastPwd(NULL), legal_cd_made_before(false), prompt("smash> "),
                           job_list(JobsList()), curr_pid(NO_CURR_PID),
                           curr_cmd("No Current cmd"), curr_fg_from_jobs(false) {
    // TODO: add your implementation
    plastPwd = new char* ();
    *plastPwd = NULL;
}


SmallShell::~SmallShell() {
    if (*plastPwd)
        delete[] * plastPwd;

    delete plastPwd;
}


static bool redirectionParse(const char* cmd_line ,string& cmd_command, string& file_name){
  bool append = (std::string(cmd_line).find(">>") != std::string::npos) ? true : false;
  int offset = append+1;
  std::string delimeter = (append) ? ">>" : ">";
  cmd_command = _trim(std::string(cmd_line).substr(0,std::string(cmd_line).find(delimeter)));
  file_name = _trim(std::string(cmd_line).substr(std::string(cmd_line).find(delimeter)+offset));
  return append;
}

static bool pipeParse(const char* cmd_line ,string& first_command, string& second_command){
  bool std_err = (std::string(cmd_line).find("|&") != std::string::npos) ? true : false;
  int offset = std_err+1;
  std::string delimeter = (std_err) ? "|&" : "|";
  first_command = _trim(std::string(cmd_line).substr(0,std::string(cmd_line).find(delimeter)));
  second_command = _trim(std::string(cmd_line).substr(std::string(cmd_line).find(delimeter)+offset));
  return std_err;
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {

  std::string cmd_s = _trim(string(cmd_line));
  std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));  
    if (string(cmd_line).find(">") != string::npos) {
        return new RedirectionCommand(cmd_line);
    }
    else if (string(cmd_line).find("|") != string::npos) {
        return new PipeCommand(cmd_line);
    }
    else if (firstWord.compare("head") == 0) {
        return new HeadCommand(cmd_line);
    }
    else if (firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0) {
        return new ChangePromptCommand(cmd_line, getPPrompt());
    }
    else if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0) {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0 || firstWord.compare("cd&") == 0) {
        return new ChangeDirCommand(cmd_line, plastPwd);
    }
    else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0) {
        return new JobsCommand(cmd_line, &job_list);
    }
    else if (firstWord.compare("kill") == 0 || firstWord.compare("kill&") == 0) {
        return new KillCommand(cmd_line, &job_list);
    }
    else if (firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0) {
        return new QuitCommand(cmd_line, &job_list);
    }
    else if (firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0) {
        return new ForegroundCommand(cmd_line, &job_list);
    }
    else if (firstWord.compare("bg") == 0 || firstWord.compare("bg&") == 0) {
        return new BackgroundCommand(cmd_line, &job_list);
    }

     return new ExternalCommand(cmd_line, &job_list);

    // else {
    //   return new ExternalCommand(cmd_line);
    // }
    /////external commands:
    return nullptr; // should it be here?
}

void SmallShell::setPLastPwd(Command* cmd) {
    if (strcmp(cmd->args[0], "cd") == 0)
    {
        ChangeDirCommand* temp = (ChangeDirCommand*)cmd;
        if (temp->cd_succeeded)
        {
            if (!legal_cd_made_before) // upon first successful cd - entered once
            {
                *plastPwd = temp->classPlastPwd;
                legal_cd_made_before = true;
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

void SmallShell::setCurrCmd(std::string cmd) {
    curr_cmd = cmd;
}

void SmallShell::setCurrFgFromJobs(int job_id) {
    curr_fg_from_jobs = true;
    curr_fg_from_jobs_id = job_id;
}

int SmallShell::getCurrFgFromJobsListId() {
    return curr_fg_from_jobs_id;
}

bool SmallShell::CurrFgIsFromJobsList() {
    return curr_fg_from_jobs;
}

std::string SmallShell::getCurrCmd() {
    return curr_cmd;
}

void SmallShell::resetCurrFgInfo() {
    curr_pid = NO_CURR_PID;
    curr_cmd = "No Current cmd";
    curr_fg_from_jobs = false;
    curr_fg_from_jobs_id = NO_CURR_JOB_ID;
}

JobsList* SmallShell::getJobs() {
    return &job_list;
}

static int setTimeoutDuration(char* duration_str) {
    int duration = 0;
    std::stringstream duration_ss(duration_str);
    duration_ss >> duration;
    //ADD MORE CHECKING FOR LEGAL INT
    if (duration < 1)
        return ERROR;
    return duration;
}

static void getTrimmedCmdAndDuration(const char* cmd_line, std::string& new_cmd_line, int* duration) {
    int num_of_args = numberOfArgs(cmd_line);
    if (num_of_args < 3)
    {
        *duration = ERROR;
        return;
    }
    char** args = new char*[num_of_args + 1]; //buffer of (+1) due to impl. of _parse command
    int argv = _parseCommandLine(cmd_line, args);
    *duration = setTimeoutDuration(args[1]);
    if (*duration == ERROR)
        return;
    new_cmd_line = args[2];
    for (int i = 3; i < argv; i++)
    {
        new_cmd_line.append(" ").append(args[i]);
    }

    for (int i = 0; i < argv; i++) {
        if (args[i] != NULL)
            free(args[i]);
    }
    delete[] args;
}

void SmallShell::executeCommand(const char* cmd_line) {

    std::string cmd_s = _trim(string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    Command* cmd = NULL;

    if (firstWord.compare("timeout") == 0)
    {
        std::string new_cmd_line("");
        int duration = 0;
        getTrimmedCmdAndDuration(cmd_line, new_cmd_line, &duration);
        if (duration == ERROR)
        {
            fprintf(stderr, "smash error: timeout: invalid arguments\n");
            return;
        }
        cmd = CreateCommand(new_cmd_line.c_str());
        //cmd->updateCmdForTimeout(cmd_line);
        TimedCommandEntry entry(time(NULL) + duration, cmd_line, NOT_SET); // should implement inst.
        //operator compares absulote alarm times
        if (timed_list.front() < entry)
        {
            timed_list.push_back(entry);
            cmd->timed_entry = &timed_list.back();
        }
        else
        {
            timed_list.push_front(entry);
            cmd->timed_entry = &timed_list.front();
        }
        alarm(difftime(timed_list.front().alrm_time, time(NULL)));
    }
    else
    {
        cmd = CreateCommand(cmd_line);
    }

    job_list.removeFinishedJobs();
    cmd->execute();
    if (cmd->getCmdJobId() != NOT_SET)
    {
        getJobs()->getJobById(cmd->getCmdJobId())->cmd = cmd_line; //update full cmd line for timeout commands
        cmd->cleanCmdJobId(); // Safety measure
    }
    timed_list.sort();
    setPLastPwd(cmd);
    delete cmd;
    cmd = NULL; // VALGRIND
}

//////////pipes and redirections////////////
RedirectionCommand::RedirectionCommand(const char* cmd_line) : Command(cmd_line), command_cmd(EMPTY_STRING), file_name(EMPTY_STRING) {
  append = redirectionParse(cmd_line, command_cmd, file_name);
  if(command_cmd!=EMPTY_STRING){ // for example: >>a.txt
    cmd = command_cmd.c_str();
  }
} 

void RedirectionCommand::execute() {
  if(command_cmd==EMPTY_STRING){ // for example: >>a.txt should to nothing as said in piazza
    return;
  }
  int stdout_fd = dup(1);
  if(stdout_fd == ERROR) {
    fprintf(stderr,"smash error: dup failed\n");
    return;
  }
  int fd;
  if (!append) {
    fd = open(file_name.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
  }
  else {
    fd = open(file_name.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0666);
  }

  if (fd == ERROR) {
    fprintf(stderr,"smash error: open failed\n");
    return;
  }

  int result = dup2(fd,1);
  if(result == ERROR) {
    fprintf(stderr,"smash error: dup2 failed\n");
    return;
  }
  SmallShell::getInstance().executeCommand(command_cmd.c_str());
  result = dup2(stdout_fd,1); // back to normal
  if(result == ERROR) {
    fprintf(stderr,"smash error: dup2 failed\n");
    return;
  }
  result = close(fd);
  if (result == ERROR) {
    fprintf(stderr,"smash error: close failed\n");
  }
}
 
PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line), first_command(EMPTY_STRING), second_command(EMPTY_STRING) {
  is_stderr = pipeParse(cmd_line,first_command,second_command);
  cmd = first_command.c_str();
}

enum {RD,WR};

void PipeCommand::execute() {
  int fd[2];
  int result = pipe(fd);
  if(result == ERROR) {
    fprintf(stderr,"smash error: pipe failed");
    return;
  }
   //close stdout('|') or stderr('|&')
  int fd_to_close=1;
  if(is_stderr) {
    fd_to_close=2;
  }
  int pid_1 = fork();
  if(pid_1 ==ERROR) {
    fprintf(stderr,"smash error: fork failed");
    exit(0);
  }
  //first child
  if (pid_1 == 0) {
    if(setpgrp() == ERROR) {
      fprintf(stderr,"smash error: setpgrp failed");
      exit(0);
    }
    if(dup2(fd[WR],fd_to_close) == ERROR) { // 1 or 2 -> write pipe
      fprintf(stderr,"smash error: dup2 failed");
      exit(0);
    }
    if(close(fd[RD]) == ERROR) {
      fprintf(stderr,"smash error: close failed");
      exit(0);
    }
    if(close(fd[WR]) == ERROR) {
      fprintf(stderr,"smash error: close failed");
      exit(0);
    }
    SmallShell::getInstance().executeCommand(first_command.c_str());
    exit(0);
  }
  int pid_2 = fork();
  if(pid_2 ==ERROR) {
    fprintf(stderr,"smash error: fork failed");
    exit(0);
  }
  // second child
  if (pid_2 == 0) {
    if(setpgrp() == ERROR) {
      fprintf(stderr,"smash error: setpgrp failed");
      exit(0);
    }
    if(dup2(fd[RD],0) == ERROR) { //0 -> read pipe
      fprintf(stderr,"smash error: dup2 failed");
      exit(0);
    }
    if(close(fd[RD]) == ERROR) {
      fprintf(stderr,"smash error: close failed");
      exit(0);
    }
    if(close(fd[WR]) == ERROR) {
      fprintf(stderr,"smash error: close failed");
      exit(0);
    }
    SmallShell::getInstance().executeCommand(second_command.c_str());
    exit(0);
  }
  close(fd[RD]);
  close(fd[WR]);
  if(waitpid(pid_1, nullptr, 0) == ERROR) {
    fprintf(stderr,"smash error: waitpid failed");
    exit(0);
  }
  if(waitpid(pid_2, nullptr, 0) == ERROR) {
    fprintf(stderr,"smash error: waitpid failed");
    exit(0);
  }
}
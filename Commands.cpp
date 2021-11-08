#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;
#define MAX_CWD_LENGTH 256
#define ERROR -1
#define LAST_CD "-"
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

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  cout << getpid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  char cwd[MAX_CWD_LENGTH];
  if(getcwd(cwd, MAX_CWD_LENGTH) == NULL){
    perror("getcwd() error");
    return;
  }
  cout << cwd << endl;  
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line) , plastPwd(plastPwd) {}

void ChangeDirCommand::execute() {
  int chdir_return;
  if(argv>2) {
    perror("smash error: cd: too many arguments");
    return;
  }
  if(std::strcmp(args[1],"-")) {
    std::cout<<args[1]<<endl;
    chdir_return=chdir(args[1]);
  }
  else{
    if(*plastPwd==NULL) {
      perror("smash error: cd: OLDPWD not set");
      return;
    }
    chdir_return=chdir(*plastPwd);
  }

  if(chdir_return==ERROR){
    perror("Need to be changed");
  }
}


SmallShell::SmallShell() {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
  std::string cmd_s = _trim(string(cmd_line));
  std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  // if (firstWord.compare("chprompt") == 0) {
  //   //TODO: add implementation
    
  // }
  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line,plastPwd);
  }
  std::cout<<"ssss";
  // else {
  //   return new ExternalCommand(cmd_line);
  // }
  
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
}
#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_

void ctrlZHandler(int sig_num);
void ctrlCHandler(int sig_num);
void alarmHandler(int sig_num, siginfo_t* info, void* context);

#endif //SMASH__SIGNALS_H_

#ifndef agent_Options_H
#define agent_Options_H
#include <string>
extern jvmtiEnv *jvmti;
extern JavaVM *javaVM;
void agentCommand(std::string command, std::string target);

#endif /* agent_Options_H */
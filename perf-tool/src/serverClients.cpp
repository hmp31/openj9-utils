#include "serverClients.hpp"

using namespace std;

string NetworkClient::handlePoll(char buffer[])
{
    int n = read(socketFd, buffer, 255);
    string s = string(buffer);
    s.pop_back();

    if (n < 0)
    {
        perror("ERROR reading from socket");
    }
    else if (n > 0)
    {
        return s;
    }

    return "";
}

LoggingClient::LoggingClient(string filename)
{
    logFile.open(filename);
    if (!logFile.is_open())
    {
        perror("ERROR opening logs file");
    }
}

void LoggingClient::logData(string message, string recievedFrom)
{
    if (logFile.is_open())
    {
        logFile << "recieved: " << message << " | from: " << recievedFrom << endl;
    }
}

CommandClient::CommandClient(std::string filename) {
    commandsFile.open(filename);
    if (!commandsFile.is_open()) {
        printf("filename: %s\n", filename);
        perror("ERROR opening commands file");
    }

    commands = json::parse(commandsFile);
}

// string CommandClient::handlePoll()
// {
//     if (currentInterval <= 0)
//     {
//         getline(commandsFile, prevCommand);
//         if (!commandsFile.eof())
//         {
//             currentInterval = ServerConstants::COMMAND_INTERVALS;
//             return prevCommand;
//         }
//     }
//     else
//     {
//         currentInterval = currentInterval - ServerConstants::POLL_INTERVALS;
//     }

//     return "";
// }
json CommandClient::handlePoll() {
    static int commandNumber = 0;
    static const int numCommands = commands.size();
    
    if (currentInterval <= 0) {
        if(commandNumber < numCommands){
            commandNumber++;
        }
    } else {
        currentInterval = currentInterval - ServerConstants::POLL_INTERVALS;
    }

    return commands[commandNumber];
}
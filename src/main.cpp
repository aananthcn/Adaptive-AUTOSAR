#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

#include <iostream>

bool running;
AsyncBsdSocketLib::Poller poller;
application::platform::ExecutionManagement *executionManagement;


void PrintUsage()
{
    std::cout << "Usage: <program> [config] [evconf] [dmconf] [phmcfg]\n" << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  config - Path to the general configuration file." << std::endl;
    std::cout << "  evconf - Path to the extended vehicle configuration file." << std::endl;
    std::cout << "  dmconf - Path to the diagnostic manager configuration file." << std::endl;
    std::cout << "  phmcfg - Path to the health monitoring configuration file." << std::endl;
    std::cout << "\nIf any argument is omitted, a default file path will be used." << std::endl;
    std::cout << "\nAdditional prompts:" << std::endl;
    std::cout << "  - The program may ask for the VCC API key interactively." << std::endl;
    std::cout << "  - The program may ask for the bearer token interactively, with input hidden for security." << std::endl;
}

void performPolling()
{
    const std::chrono::milliseconds cSleepDuration{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (running)
    {
        poller.TryPoll();
        std::this_thread::sleep_for(cSleepDuration);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        std::cerr << "[ERROR]: Incorrect number of arguments provided!\n"
                << "Expected 4 arguments, but received " << (argc - 1) << ".\n\n";
        PrintUsage();
        return -1;
    }

    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);

    bool _successful{_argumentConfiguration.TryAskingVccApiKey()};
    if (!_successful)
    {
        std::cout << "Asking for the VCC API key is failed!";
        return -1;
    }

    std::system("clear");
    _successful = _argumentConfiguration.TryAskingBearToken();
    if (!_successful)
    {
        std::cout << "Asking for the OAuth 2.0 bear key is failed!";
        return -1;
    }

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    std::getchar();
    std::system("clear");
    std::getchar();

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}
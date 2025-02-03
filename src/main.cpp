#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>

std::atomic<bool> running{true};

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

// Signal handler to stop execution on SIGTERM or SIGABRT
void SignalHandler(int signal)
{
    std::cout << "\n[INFO]: Caught signal " << signal << ", shutting down...\n";
    running = false;
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
    // Register signal handlers
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGINT, SignalHandler);
    std::cout << "Starting AUTOSAR Execution Management ...\n";

    if (argc != 5) {
        std::cerr << "[ERROR]: Incorrect number of arguments provided!\n"
                << "Expected 4 arguments, but received " << (argc - 1) << ".\n\n";
        PrintUsage();
        return -1;
    }

    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);

#if EM_SECURITY_ENABLED
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
#endif

    running = true;
    executionManagement = new application::platform::ExecutionManagement(&poller);
    executionManagement->Initialize(_argumentConfiguration.GetArguments());
    std::cout << "[INFO]: created ExecutionManagement::Main() thread.\n";

    //std::future<void> _future{std::async(std::launch::async, performPolling)};
    std::thread pollingThread(performPolling);
    std::cout << "[INFO]: created performPolling() thread to poll for events.\n";

    // Wait until signal is received
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Cleanup
    if (pollingThread.joinable()) {
        pollingThread.join();
    }

    int _result = 0;
    try {
        _result = executionManagement->Terminate();
    } catch (const std::exception &e) {
        std::cerr << "[ERROR]: Exception in Terminate(): " << e.what() << "\n";
        _result = -1; // Indicate failure
    } catch (...) {
        std::cerr << "[ERROR]: Unknown exception in Terminate()\n";
        _result = -1;
    }
    running = false;
    delete executionManagement;
    std::cout << "[INFO]: Shutting down...\n";

    return _result;
}
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

class ProcessManager {
private:
    struct ProcessInfo {
        PROCESS_INFORMATION pi;
        std::chrono::steady_clock::time_point startTime;
        bool isRunning;
    };
    
    std::vector<ProcessInfo> processes;
    const DWORD TIMEOUT_SECONDS = 10; // Таймаут у секундах

public:
    ProcessManager() {}
    
    bool createProcess(const std::wstring& programPath, const std::wstring& args = L"") {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        std::wstring commandLine = programPath;
        if (!args.empty()) {
            commandLine += L" " + args;
        }
        
        // Створення процесу
        if (!CreateProcessW(
            NULL,                    // Модуль додатку (NULL, якщо вказано шлях до програми)
            const_cast<LPWSTR>(commandLine.c_str()), // Командний рядок
            NULL,                   // Атрибути безпеки для процесу
            NULL,                   // Атрибути безпеки для потоку
            FALSE,                  // Успадковування дескрипторів
            0,                      // Прапори створення
            NULL,                   // Середовище процесу
            NULL,                   // Робочий каталог
            &si,                    // Інформація про запуск
            &pi                     // Інформація про процес
        )) {
            std::cout << "CreateProcess failed. Error: " << GetLastError() << std::endl;
            return false;
        }

        // Зберігаємо інформацію про процес
        ProcessInfo processInfo;
        processInfo.pi = pi;
        processInfo.startTime = std::chrono::steady_clock::now();
        processInfo.isRunning = true;
        
        processes.push_back(processInfo);
        std::cout << "Process created with ID: " << pi.dwProcessId << std::endl;
        return true;
    }
    
    void monitorProcesses() {
        while (!processes.empty()) {
            for (auto& proc : processes) {
                if (!proc.isRunning) continue;

                DWORD exitCode;
                if (GetExitCodeProcess(proc.pi.hProcess, &exitCode)) {
                    if (exitCode != STILL_ACTIVE) {
                        std::cout << "Process " << proc.pi.dwProcessId 
                                << " completed with exit code: " << exitCode << std::endl;
                        proc.isRunning = false;
                        continue;
                    }
                }

                // Перевірка таймауту
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>
                              (now - proc.startTime).count();
                
                if (duration >= TIMEOUT_SECONDS) {
                    std::cout << "Process " << proc.pi.dwProcessId 
                             << " exceeded timeout. Terminating..." << std::endl;
                    if (TerminateProcess(proc.pi.hProcess, 1)) {
                        proc.isRunning = false;
                    }
                }
            }
            
            // Видалення завершених процесів
            auto it = processes.begin();
            while (it != processes.end()) {
                if (!it->isRunning) {
                    CloseHandle(it->pi.hProcess);
                    CloseHandle(it->pi.hThread);
                    it = processes.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Невелика пауза для зменшення навантаження
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    ~ProcessManager() {
        // Завершення всіх активних процесів
        for (auto& proc : processes) {
            if (proc.isRunning) {
                TerminateProcess(proc.pi.hProcess, 1);
            }
            CloseHandle(proc.pi.hProcess);
            CloseHandle(proc.pi.hThread);
        }
    }
};

int main() {
    ProcessManager pm;

    // Створення різних процесів
    if (pm.createProcess(L"calc.exe")) {
        std::cout << "Started Calculator process\n";
    }
    if (pm.createProcess(L"cmd.exe", L"/c echo Hello from Command Prompt! && pause")) {
        std::cout << "Started Command Prompt process\n";
    }
    if (pm.createProcess(L"explorer.exe")) {
        std::cout << "Started Explorer process\n";
    }

    // Відстеження процесів
    pm.monitorProcesses();

    return 0;
}

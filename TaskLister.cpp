// Marwan Hussein Galal
// marwanhussein2004@gmail.com
// =====================================================================
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <Psapi.h>
#elif defined(__linux__) || defined(linux) || defined(__linux)
#include <sys/types.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#endif

using namespace std;

class ProcessInfo {
private:
    string name;
    int PID;
    size_t MemoryUsage;
    int status;
public:
    ProcessInfo(string name, int PID, size_t MemoryUsage, int status) {
        this->name = name;
        this->PID = PID;
        this->MemoryUsage = MemoryUsage;
        this->status = status;
    };
    ProcessInfo() {
        this->name = "";
        this->PID = 0;
        this->MemoryUsage = 0;
        this->status = 0;
    };
    const size_t getMemoryUsage() {
        return MemoryUsage;
    };
    const string getName() {
        return name;
    };
    const int getPID() {
        return PID;
    };
    const int getStatus() {
        return status;
    };
    ProcessInfo operator=(ProcessInfo& p) {
        this->name = p.getName();
        this->PID = p.getPID();
        this->MemoryUsage = p.getMemoryUsage();
        this->status = p.getStatus();
        return *this;
    };
};

class ProcessList {
private:
    vector<ProcessInfo>processes;
public:
    enum class SortBy {
        Name,
        PID,
        MemoryUsage,
        Ascending,
        Descending
    };
#if defined(_WIN32) || defined(_WIN64)
    void loadProcesses() {
        DWORD processlist[1024];
        DWORD processIds[1024], bytesReturned;
        // Use EnumProcesses to get the list of PIDs of all processes running on the system
        if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned)) {
            cerr << "Failed to enumerate processes." << endl;
            return;
        }
        // Calculate the number of PIDs returned
        DWORD numProcesses = bytesReturned / sizeof(DWORD);
        // Iterate through the list of process IDs returned by EnumProcesses
        for (int i = 0; i < numProcesses; i++) {
            // Skip invalid or system-level processes with PID 0
            if (processIds[i] != 0) {
                // Open the process using its PID with permissions to query information and read memory
                // Returns a handle to the process if successful, or NULL otherwise
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIds[i]);
                // Check if the process was successfully opened
                if (hProcess != NULL) {
                    // Declare variables to store module handle, required bytes, and process memory information
                    HMODULE hMod;
                    DWORD bytesRequired;
                    PROCESS_MEMORY_COUNTERS pmc;
                    DWORD status;
                    // Retrieve the first module's handle (usually the executable file) using EnumProcessModules
                    // Also, get memory usage details for the process using GetProcessMemoryInfo
                    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &bytesRequired) && GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)) && GetExitCodeProcess(hProcess, &status)) {
                        // Declare a buffer to store the process's executable name
                        char szProcessName[MAX_PATH];
                        // Get the base name (executable name) of the module (process) and store it in szProcessName
                        if (GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(char))) {
                            // Convert the C-style string to a normal string for easier handling
                            string processName(szProcessName);
                            // Create a ProcessInfo object to store the process name, PID, and memory usage
                            ProcessInfo temp(processName, processIds[i], pmc.WorkingSetSize, status);
                            // Add the ProcessInfo object to the vector
                            processes.push_back(temp);
                        }
                    }
                    // Close the process handle to release resources
                    CloseHandle(hProcess);
                }
            }
        }
    }
#elif defined(__linux__) || defined(linux) || defined(__linux)
    void loadProcesses() {
        DIR* procDir; // Directory pointer for "/proc"
        struct dirent* procEntry; // Directory entry pointer
        // Open the "/proc" directory
        if ((procDir = opendir("/proc")) != NULL) {
            // Iterate through each entry in the "/proc" directory
            while ((procEntry = readdir(procDir)) != NULL) {
                // Process directories with numeric names (representing process IDs)
                if (isdigit(*procEntry->d_name)) {
                    string pidDir = "/proc/" + string(procEntry->d_name); // Path to process directory
                    string statusFilePath = pidDir + "/status"; // Path to "status" file
                    ifstream statusFile(statusFilePath); // Open the "status" file
                    if (!statusFile.is_open()) {
                        continue; // Skip if the file cannot be opened
                    }
                    string line, processName;
                    int processId = -1;
                    size_t memoryUsageKb = 0;
                    int processState = 0;
                    // Read and parse the "status" file line by line
                    while (getline(statusFile, line)) {
                        // Extract process name
                        if (line.find("Name:") == 0) {
                            processName = line.substr(line.find(':') + 1);
                            processName.erase(processName.find_last_not_of(" \t\n\r\f\v") + 1); // Trim trailing whitespace
                        }
                        // Extract process ID
                        else if (line.find("Pid:") == 0) {
                            try {
                                processId = stoi(line.substr(line.find(':') + 1));
                            } catch (...) {
                                processId = -1; // Default to -1 if parsing fails
                            }
                        }
                        // Extract memory usage in KB
                        else if (line.find("VmSize:") == 0) {
                            try {
                                memoryUsageKb = stoi(line.substr(line.find(':') + 1));
                            } catch (...) {
                                memoryUsageKb = 0; // Default to 0 if parsing fails
                            }
                        }
                        // Extract process state (I = idle  R = running  S = sleeping D = uninterruptible sleep T = stopped   Z = zombie)
                        else if (line.find("State:") == 0) {
                            string state = line.substr(line.find(':') + 1);
                            state.erase(state.find_last_not_of(" \t\n\r\f\v") + 1); // Trim trailing whitespace
                            if (state[1] == 'S') {
                                processState = 0;
                            } else if (state[1] == 'R') {
                                processState = 1;
                            } else if (state[1] == 'D') {
                                processState = 2;
                            } else if (state[1] == 'T') {
                                processState = 3;
                            } else if (state[1] == 'Z') {
                                processState = 4;
                            } else if (state[1] == 'I') {
                                processState = 5;
                            }
                        }
                    }
                    statusFile.close(); // Close the "status" file
                    // Add process to the list if valid
                    if (processId != -1 && !processName.empty()) {
                        ProcessInfo process(processName, processId, memoryUsageKb, processState);
                        processes.push_back(process);
                    }
                }
            }
            closedir(procDir); // Close the "/proc" directory
        }
    }
#endif
    void sortProcesses(SortBy sortBy, SortBy order) {
        switch (order) {
        case SortBy::Ascending:
            switch (sortBy) {
            case SortBy::Name:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getName() > processes[j].getName()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            case SortBy::PID:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getPID() > processes[j].getPID()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            case SortBy::MemoryUsage:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getMemoryUsage() > processes[j].getMemoryUsage()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            }
            break;
        case SortBy::Descending:
            switch (sortBy) {
            case SortBy::Name:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getName() < processes[j].getName()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            case SortBy::PID:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getPID() < processes[j].getPID()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            case SortBy::MemoryUsage:
                for (int i = 0; i < processes.size(); i++) {
                    for (int j = i + 1; j < processes.size(); j++) {
                        if (processes[i].getMemoryUsage() < processes[j].getMemoryUsage()) {
                            ProcessInfo temp = processes[i];
                            processes[i] = processes[j];
                            processes[j] = temp;
                        }
                    }
                }
                break;
            }
            break;
        }
    }
    void printProcesses() {
        cout << setw(37) << left << "Name" << left << setw(10) << "|" << setw(13) << left << "PID" << setw(10) << "|" << setw(10) << right << "Memmory Usage" << setw(10) << "|" << setw(10) << internal << "Status" << endl;
        cout << setw(108) << setfill('-') << "-" << endl;
        cout << setfill(' ');
        for (int i = 0; i < processes.size(); i++) {
            string status;
            string name = processes[i].getName();
#if defined(_WIN32) || defined(_WIN64)
            if (processes[i].getStatus() == 259) {
                status = "Running";
            } else {
                status = "Not Running";
            }
#elif defined(__linux__) || defined(linux) || defined(__linux)
            if (processes[i].getStatus() == 0) {
                status = "Sleeping";
            } else if (processes[i].getStatus() == 1) {
                status = "Running";
            } else if (processes[i].getStatus() == 2) {
                status = "Uninterruptible Sleep";
            } else if (processes[i].getStatus() == 3) {
                status = "Stopped";
            } else if (processes[i].getStatus() == 4) {
                status = "Zombie";
            } else if (processes[i].getStatus() == 5) {
                status = "Idle";
            } else {
                status = "Unknown";
            }
#endif
            if (name.size() > 38) {
                name.resize(35);
                name.append("...");
            }
            cout << setw(47) << left << name << setw(10) << left << processes[i].getPID() << setw(20) << right << processes[i].getMemoryUsage() << left << " KB"
                << setw(23) << internal << status << endl;
        }
        cout << setw(108) << setfill('=') << '=' << endl;
    }
};

// Function to get user input choice with validation
int GetInputChoice(const string& prompt, int gratest_choice) {
    int input;
    string line;
    while (true) {
        cout << prompt;
        getline(cin, line);
        if (!line.empty()) {
            try {
                input = stoi(line);
                if (input > gratest_choice) {
                    cout << "Please enter a valid Choice.\n";
                } else {
                    return input;
                }
            } catch (invalid_argument& e) {
                cout << "Invalid input. Please enter a number.\n";
            }
        } else {
            cout << "Empty input. Please try again.\n";
        }
    }
}

// Function to display sort menu
void SortMenu(ProcessList& pl) {
    int typeChoice, orderChoice;
    cout << "Sort By:" << endl;
    cout << "1. Name" << endl;
    cout << "2. PID" << endl;
    cout << "3. Memory Usage" << endl;
    cout << "4. Exit" << endl;
    typeChoice = GetInputChoice("Enter your choice: ", 4);
    if (typeChoice == 4) {
        exit(0);
    } else {
        ProcessList::SortBy sortBy = static_cast<ProcessList::SortBy>(typeChoice - 1);
        cout << "Order:" << endl;
        cout << "1. Ascending" << endl;
        cout << "2. Descending" << endl;
        orderChoice = GetInputChoice("Enter your choice: ", 2);
        ProcessList::SortBy order = static_cast<ProcessList::SortBy>(orderChoice + 2);
        pl.sortProcesses(sortBy, order);
    }
}

int main() {
#if not defined(_WIN32) && not defined(_WIN64) && not defined(__linux__) && not defined(linux) && not defined(__linux)
    cout << "Sorry :( your OS is not supported." << endl;
    return 1;
#endif
    cout << "Welcome to the Process List Program" << endl;
    cout << "====================================" << endl;
    ProcessList pl;
    while (true) {
        pl.loadProcesses();
        SortMenu(pl);
        pl.printProcesses();
    }
    return 0;
}
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <regex>
#include <queue>

// Helper function to print program usage
void print_usage(char* program_name) {
    std::cout << "Usage: " << program_name << " <pattern> [-d|--dir <directory>] [-l|--log_file <log_file_name>] [-r|--result_file <result_file_name>] [-t|--threads <num_threads>]\n";
}

// Struct to store file path and line number where pattern is found
struct Match {
    std::string file_path;
    int line_number;
    std::string line_content;
};

// Global variables
std::vector<std::thread> thread_pool;
std::mutex thread_mutex;
std::mutex log_mutex;
std::mutex result_mutex;
std::atomic<int> files_searched(0);
std::atomic<int> files_with_pattern(0);
std::atomic<int> patterns_number(0);
std::string search_pattern;
std::string start_directory = ".";
std::string log_file_name;
std::string result_file_name;
int num_threads = 4;
std::vector<std::string> processed_files;

// Cleaner function for .log + .txt
void clear(std::string x)
{
    std::ofstream file(x);
}

// Helper function to process a single file and find matches
void process_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << "Error opening file: " << file_path << std::endl;
        return;
    }
    std::string line;
    int line_number = 0;
    std::vector<Match> matches;
    while (std::getline(file, line)) {
        line_number++;
        std::smatch match;
        if (std::regex_search(line, match, std::regex(search_pattern))) {
            std::lock_guard<std::mutex> lock(result_mutex);
            matches.push_back({ file_path, line_number, line });
            patterns_number++;
        }
    }
    file.close();
    files_searched++;
    if (!matches.empty()) {
        std::lock_guard<std::mutex> lock(result_mutex);
std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
    if (a.file_path != b.file_path) {
        return a.file_path < b.file_path;
    } else {
        return a.line_number < b.line_number;
    }
});
        for (const auto& match : matches) {
            std::ofstream result_file(result_file_name, std::ios_base::app);
            if (result_file) {
                result_file << match.file_path << ":" << match.line_number << ": " << match.line_content << "\n";
                result_file.close();
            }
            else {
                std::lock_guard<std::mutex> lock(log_mutex);
                std::cerr << "Error opening file: " << result_file_name << std::endl;
            }
        }
        files_with_pattern++;
    }
}

// Helper function for each thread to get a file to process from the queue
void process_files() {
    std::string file_path;
    while (true) {
        std::unique_lock<std::mutex> lock(thread_mutex);
        if (processed_files.empty()) {
            lock.unlock();
            break;
        } else {
            std::sort(processed_files.begin(), processed_files.end());
            file_path = processed_files.back();
            processed_files.pop_back();
            lock.unlock();
            process_file(file_path);
            std::lock_guard<std::mutex> lock(log_mutex);
            std::ofstream log_file(log_file_name, std::ios_base::app);
            if (log_file) {
                log_file << std::this_thread::get_id() << ":" << file_path << "," << std::endl;
                log_file.close();
            } else {
                std::cerr << "Error opening file: " << log_file_name << std::endl;
            }
        }
    }
}

// Main function to parse command line arguments and start the search
int main(int argc, char** argv) {
auto start = std::chrono::steady_clock::now();
    // Parse command line arguments
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    search_pattern = argv[1];
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--dir") {
            start_directory = argv[++i];
        }
        else if (arg == "-l" || arg == "--log_file") {
            log_file_name = argv[++i];
        }
        else if (arg == "-r" || arg == "--result_file") {
            result_file_name = argv[++i];
        }
        else if (arg == "-t" || arg == "--threads") {
            num_threads = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set default values for log and result file names
    if (log_file_name.empty()) {
        log_file_name = std::string(argv[0]) + ".log";
        clear(log_file_name);
    }
    if (result_file_name.empty()) {
        result_file_name = std::string(argv[0]) + ".txt";
        clear(result_file_name);
    }

    // Get a list of all files in the start directory and its subdirectories
    std::vector<std::string> file_paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(start_directory)) {
        if (!entry.is_directory()) {
            file_paths.push_back(entry.path().string());
        }
    }

    // Add files to the processed_files queue
    std::reverse(file_paths.begin(), file_paths.end());
    for (const auto& file_path : file_paths) {
        processed_files.push_back(file_path);
    }

    // Start the thread pool
    for (int i = 0; i < num_threads; i++) {
        thread_pool.emplace_back(process_files);
    }

    // Wait for all threads to finish
    for (auto& thread : thread_pool) {
        thread.join();
    }

    // Print program statistics
    std::cout << "Searched files: " << files_searched << std::endl;
    std::cout << "Files with pattern: " << files_with_pattern << std::endl;
    std::cout << "Patterns number: " << patterns_number << std::endl;
    std::cout << "Result file: " << std::filesystem::absolute(result_file_name) << std::endl;
    std::cout << "Log file: " << std::filesystem::absolute(log_file_name) << std::endl;
    std::cout << "Used threads: " << num_threads << std::endl;
    auto end = std::chrono::steady_clock::now();
    std::cout << "Elapsed time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() 
    << " [ms]" << std::endl;
    return 0;
}

#ifndef CODE_COMPUTER_H
#define CODE_COMPUTER_H

#include <string>
#include <vector>
#include <condition_variable>
#include "components.h"

using std::string;

static const string COMPUTERS_FOLDER = "computers/";
static const string COMPUTER_ADDRESS_FILE = "/address.txt";
static const string COMPUTER_MEMORY_FILE = "/memory.txt";
static const string COMPUTER_COMPONENTS_FILE = "/components.txt";
static const string COMPUTER_TEMP_FS_FILE = "/tempfs.txt";


static string get_computer_address(const string &project_dir, const string &computer_name);

static long long get_computer_memory(const string &project_dir, const string &computer_name);

static long long get_current_time();

class Project;
class Component;
class Session;
class Filesystem;

class Computer {
private:
    friend Session;
    Session *session = nullptr;
    std::vector<Component *> components;
public:
    const string address;
    const string name;
    const long long start_time;
    const long long memory;
    long long used_memory = 0;
    std::queue<string> signal_queue;
    std::mutex queue_lock;
    std::condition_variable queue_notifier;
    Filesystem *tmp_fs;

    explicit Computer(string &project_dir, string &name, std::map<string, Component *> &all_components);

    int get_components(std::vector<Component *> *v);

    Component *get_component(const string &component_address);
};

#endif //CODE_COMPUTER_H

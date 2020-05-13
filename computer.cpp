#include "computer.h"
#include "components.h"
#include <chrono>


Computer::Computer(string &project_dir, string &name,
                   std::map<string, Component *> &all_components) :
        address(get_computer_address(project_dir, name)),
        name(name), start_time(get_current_time()), memory(get_computer_memory(project_dir, name)) {
    std::ifstream in(project_dir + COMPUTERS_FOLDER + name + COMPUTER_COMPONENTS_FILE);
    string component_name;
    while (in >> component_name) {
        Component *component = all_components[component_name];
        components.push_back(component);
        component->computer = this;
    }
    components.push_back(new ComputerComponent(this));
    std::ifstream in2(project_dir + COMPUTERS_FOLDER + name + COMPUTER_TEMP_FS_FILE);
    string tmp_fs_name;
    in2 >> tmp_fs_name;
    tmp_fs = dynamic_cast<Filesystem *>(all_components[tmp_fs_name]);
}

int Computer::get_components(std::vector<Component *> *v = nullptr) {
    int c = 0;
    for (auto component : components) {
        if (v) v->push_back(component);
        c++;
    }
    return c;
}

Component *Computer::get_component(const string &component_address) {
    for (Component *component : components) {
        if (component->address == component_address) return component;
    }
    return nullptr;
}

Component *Computer::get_component_by_name(const string &component_name) {
    for (Component *component : components) {
        if (component->name == component_name) return component;
    }
    return nullptr;
}


string get_computer_address(const string &project_dir, const string &computer_name) {
    std::ifstream in(project_dir + COMPUTERS_FOLDER + computer_name + COMPUTER_ADDRESS_FILE);
    string address;
    in >> address;
    return address;
}

long long get_computer_memory(const string &project_dir, const string &computer_name) {
    std::ifstream in(project_dir + COMPUTERS_FOLDER + computer_name + COMPUTER_MEMORY_FILE);
    long long memory;
    in >> memory;
    return memory;
}

long long get_current_time() {
    return
            std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
            ).count();
}



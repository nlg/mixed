// TODO
#include <stdio.h>
#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <mutex>

#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

using namespace std;

struct Hw
{
    void hello() { cout << "hello" << endl; }
    int i;
};

typedef std::pair<uint64_t,weak_ptr<Hw>> Handle;
typedef std::vector<Handle> Handles;
typedef Handles::iterator Handles_iterator;
typedef void (*FP)();

struct Lookup_table {
	Lookup_table() : next_id(1) {}
	void* insert(weak_ptr<Hw> ptr);
    void  erase(void* handle);
	weak_ptr<Hw> find(void* handle);
	uint64_t next_id;
	Handles handles;
	mutex my_mutex;
};

static Lookup_table lookup_table;

struct IsHandle
{
    IsHandle(uint64_t id) : _id(id) {}
    bool operator()(const Handle& h) const {
        return h.first == _id;
    }
    unsigned _id;
};

// real_pointer -> /transformation/ -> unique_faux_pointer
void* Lookup_table::insert(weak_ptr<Hw> ptr)
{
    my_mutex.lock();    
    if (next_id == 0) return NULL; // wrap around
    uint64_t temp_id = next_id;
    handles.push_back(Handle(next_id++, ptr));
	my_mutex.unlock();
    return reinterpret_cast<void*>(temp_id);
}

// unique_faux_pointer -> /lookup/ -> real_pointer
weak_ptr<Hw> Lookup_table::find(void* handle)
{
	my_mutex.lock();
	weak_ptr<Hw> out_ptr;
	uint64_t id = reinterpret_cast<uint64_t>(handle);
    Handles_iterator it = boost::find_if(lookup_table.handles, IsHandle(id));
    if (it != lookup_table.handles.end()) {        
        out_ptr = it->second;
    }
    my_mutex.unlock();
    return out_ptr;
}

void Lookup_table::erase(void* handle)
{
	my_mutex.lock();
	uint64_t id = reinterpret_cast<uint64_t>(handle);
    boost::remove_erase_if(lookup_table.handles, IsHandle(id));    
    my_mutex.unlock();
}
static void test(void* ctx)
{
    if (ctx == NULL) {
        cout << "ERROR" << endl;
    }
    weak_ptr<Hw> wp = lookup_table.find(ctx);
    shared_ptr<Hw> sp = wp.lock();
    if (sp) {
        sp->hello();
    }
    else {
        cout << "dead" << endl;
    }
}

void foo()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

void f1()
{    
    shared_ptr<Hw> sptr(new Hw());
    void* ptr = lookup_table.insert(sptr);
    if (ptr == NULL) {
        cout << "fail" << endl;
    }
    test(ptr);
    cout << lookup_table.handles.size() << endl;
}

int main()
{
    std::thread t[1000];    
    for (auto i = 0; i < 1000; ++i) {
        t[i] = thread(f1);
    }

    try {
    	for (auto j = 0; j < 1000; ++j) t[j].join();
    } catch (const std::system_error& e) {
        std::cout << "Caught a system_error\n";
        std::cout << "the error description is " << e.what() << '\n';
    }

    return 0;
}

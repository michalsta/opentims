#include <thread>
#include "thread_mgr.h"
#include "so_manager.h"
#include "bruker_api.h"

std::unique_ptr<ThreadingManager> ThreadingManager::instance;


ThreadingManager::ThreadingManager() :
n_threads(std::thread::hardware_concurrency()),
is_opentims_threading(false)
{}

ThreadingManager::~ThreadingManager() {};

ThreadingManager* ThreadingManager::get_instance()
{
    if(!instance)
        instance = std::make_unique<ThreadingManager>();
    return instance.get();
}

void ThreadingManager::opentims_threading()
{
    is_opentims_threading = true;
}

void ThreadingManager::converter_threading()
{
    is_opentims_threading = false;
}

void ThreadingManager::set_num_threads(size_t n)
{
    if(n == 0)
        n_threads = std::thread::hardware_concurrency();
    else
        n_threads = n;
}



/*
 * BrukerThreadingManager
 */

BrukerThreadingManager::BrukerThreadingManager(const ThreadingManager& prev_instance, const std::string& bruker_so_path) :
ThreadingManager(prev_instance),
bruker_lib(bruker_so_path),
tims_set_num_threads(bruker_lib.symbol_lookup<tims_set_num_threads_t>("tims_set_num_threads"))
{
    set_bruker_threads();
}

BrukerThreadingManager::~BrukerThreadingManager() {};

void BrukerThreadingManager::SetupBrukerThreading(const std::string& bruker_so_path)
{
    ThreadingManager::instance = std::make_unique<BrukerThreadingManager>(*ThreadingManager::get_instance(), bruker_so_path);
}

void BrukerThreadingManager::set_bruker_threads()
{
    if(is_opentims_threading)
        tims_set_num_threads(1);
    else
        tims_set_num_threads(n_threads);
}
void BrukerThreadingManager::opentims_threading()
{
    ThreadingManager::opentims_threading();
    set_bruker_threads();
}

void BrukerThreadingManager::converter_threading()
{
    ThreadingManager::converter_threading();
    set_bruker_threads();
}

void BrukerThreadingManager::set_num_threads(size_t n)
{
    ThreadingManager::set_num_threads(n);
    set_bruker_threads();
}

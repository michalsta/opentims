#pragma once

#include "bruker_api.h"
#include "so_manager.h"

class ThreadingManager
{
 protected:
    static std::unique_ptr<ThreadingManager> instance;
    size_t n_threads;
    bool is_opentims_threading;


 public:
    ThreadingManager();
    virtual ~ThreadingManager();

    static ThreadingManager* get_instance();

    virtual void set_num_threads(size_t n);

    virtual void opentims_threading();

    virtual void converter_threading();
};

class BrukerThreadingManager : public ThreadingManager
{
    const LoadedLibraryHandle bruker_lib;
    tims_set_num_threads_t* const tims_set_num_threads;
    void set_bruker_threads();
 public:
    BrukerThreadingManager() = delete;
    BrukerThreadingManager(const ThreadingManager& prev_instance, const std::string& bruker_so_path);

    static void SetupBrukerThreading(const std::string& so_path);

    virtual ~BrukerThreadingManager();
    
    void set_num_threads(size_t n) override final;

    void opentims_threading() override final;

    void converter_threading() override final;
};

#include "app/SimWorker.hpp"

#include "sim/SimulationRunner.hpp"

namespace agentbiosim::app
{
SimWorker::SimWorker(sim::SimulationRunner& runner) : runner_(runner)
{
    thread_ = std::thread(&SimWorker::threadMain, this);
}

SimWorker::~SimWorker()
{
    stop();
}

void SimWorker::request(const unsigned int steps, const double dt)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingSteps_ = steps;
        dt_ = dt;
        hasWork_ = true;
    }
    cv_.notify_all();
}

void SimWorker::wait()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // Ocioso = sem request pendente E não processando. (Se nunca houve request,
    // já está ocioso e retorna na hora.)
    cv_.wait(lock, [this] { return !hasWork_ && !busy_; });
}

void SimWorker::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) return;
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void SimWorker::threadMain()
{
    for (;;)
    {
        unsigned int steps = 0;
        double dt = 0.0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return hasWork_ || stop_; });
            if (stop_)
            {
                return;
            }
            steps = pendingSteps_;
            dt = dt_;
            hasWork_ = false;
            busy_ = true;
        }

        // Fora do lock: o trabalho pesado. O worker só toca o runner via step().
        for (unsigned int i = 0; i < steps; ++i)
        {
            runner_.step(dt);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            busy_ = false;
        }
        cv_.notify_all();  // acorda wait() da main (barreira)
    }
}
} // namespace agentbiosim::app

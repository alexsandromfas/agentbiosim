#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

namespace agentbiosim::sim
{
class SimulationRunner;
}

namespace agentbiosim::app
{
// Fase 35 (Estágio 2): roda os PASSOS da simulação numa thread separada, com
// barreira por frame (modelo produtor/consumidor double-buffer).
//
// Contrato de uso pela main thread, por frame:
//   wait();                 // BARREIRA: espera o batch anterior terminar (worker ocioso)
//   ... ler/mutar o runner com segurança (drenar comandos, capturar snapshot, ImGui) ...
//   request(N, dt);         // dispara: o worker roda N× runner.step(dt) em background
//   ... desenhar o SNAPSHOT (sobrepõe o worker); nunca ler os stores vivos aqui ...
//
// O worker só chama runner.step(); NUNCA toca a fila de comandos nem a UI. O único
// estado partilhado com a main são os stores do runner, e a barreira garante que as
// duas fases nunca se sobrepõem neles. Determinismo intacto: mesma ordem de step().
class SimWorker
{
public:
    explicit SimWorker(sim::SimulationRunner& runner);
    ~SimWorker();

    SimWorker(const SimWorker&) = delete;
    SimWorker& operator=(const SimWorker&) = delete;
    SimWorker(SimWorker&&) = delete;
    SimWorker& operator=(SimWorker&&) = delete;

    // Dispara N passos de `dt`. Não bloqueia. (N==0 é um no-op válido.)
    void request(unsigned int steps, double dt);
    // Barreira: bloqueia até o worker ficar ocioso (request pendente concluído).
    void wait();
    // Encerra a thread limpa (join). Idempotente; chamado também pelo dtor.
    void stop();

private:
    void threadMain();

    sim::SimulationRunner& runner_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;

    // Protegidos por mutex_:
    unsigned int pendingSteps_ = 0;
    double dt_ = 0.0;
    bool hasWork_ = false;  // request a processar
    bool busy_ = false;     // worker processando (entre pegar o request e terminar)
    bool stop_ = false;     // sinal de encerramento
};
} // namespace agentbiosim::app

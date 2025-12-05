import subprocess
import os
import time
import glob
from concurrent.futures import ThreadPoolExecutor, as_completed

# --- CONFIGURAÇÃO GERAL ---
EXECUTABLE = "../build/Release/tabu_ecp" 
RESULTS_FILE_PREFIX = "resultados_calibracao"

# Caminho para a pasta de instâncias
INSTANCES_DIR = "instancias_grandes_aleatorias/*.txt" 

# Busca todos os arquivos automaticamente
INSTANCES = glob.glob(INSTANCES_DIR)

# Ordena para garantir reprodutibilidade na ordem de inserção da fila (opcional, mas bom pra debug)
INSTANCES.sort()

if not INSTANCES:
    print(f"ERRO: Nenhuma instância encontrada em {INSTANCES_DIR}")
    exit(1)
else:
    print(f"Encontradas {len(INSTANCES)} instâncias para teste.")

for instance in INSTANCES:
    # Substitui barras invertidas \\ por barras normais (Windows) 
    new_instance = instance.replace("\\", "/")
    INSTANCES[INSTANCES.index(instance)] = new_instance



# Defina quantos processos simultâneos você quer. 
MAX_WORKERS = os.cpu_count() 

BASE_CONFIG = {
    "alpha": 0.6,
    "beta": 10,
    "p_limit": 500, 
    "p_strength": 0.1,
    "aspiration": 1,
    "time_limit": 300, # Atualizado para 5 minutos
    "max_iter": 10000000,
    "output": "",
    "instance": "",
}


SEEDS = [25, 35, 45, 55, 65] 

def run_command(params):
    """Executa o C++ (Função Worker)"""
    
    cmd = [
        EXECUTABLE,
        params["output"], # Arquivo específico desta config
        params["instance"],
        "--seed", str(params["seed"]),
        "--alpha", str(params["alpha"]),
        "--beta", str(params["beta"]),
        "--perturbation_limit", str(params["p_limit"]),
        "--perturbation_strength", str(params["p_strength"]),
        "--aspiration", str(params["aspiration"]),
        "--time_limit", str(params["time_limit"]),
        "--max_iter", str(params["max_iter"])
    ]
    
    try:
        # capture_output=True evita spam no terminal
        subprocess.run(cmd, check=True, capture_output=True)
        # Feedback minimalista: Nome do arquivo da instância (sem o caminho todo)
        inst_name = os.path.basename(params['instance'])
        # print(f"Ok: {inst_name} | Seed {params['seed']}") 
    except subprocess.CalledProcessError as e:
        print(f"Erro na execução ({params['instance']}): {e}")

def main():
    # Nota: Não removemos os arquivos aqui porque agora são muitos arquivos diferentes.
    # Se quiser limpar, teria que deletar resultados_calibracao_*.csv manualmente antes.

    print(f"Iniciando Bateria de Testes em PARALELO ({MAX_WORKERS} workers)...")
    start_time = time.time()
    
    tasks = []

    # ==========================================
    # 1. Configurar tarefas de Alpha
    # ==========================================
    alphas = [0.0, 0.1, 0.6, 1.2]
    for inst in INSTANCES:
        for alpha in alphas:
            for seed in SEEDS:
                p = BASE_CONFIG.copy()
                # O arquivo de saída agrupa todas as instâncias daquela config específica
                output_name = f"{RESULTS_FILE_PREFIX}_alpha_{alpha}_seed_{seed}.csv"
                p.update({
                    "instance": inst, 
                    "seed": seed, 
                    "alpha": alpha, 
                    "output": output_name
                })
                tasks.append(p)

    # ==========================================
    # 2. Configurar tarefas de Beta
    # ==========================================
    betas = [0, 30, 50]
    for inst in INSTANCES:
        for beta in betas:
            for seed in SEEDS:
                p = BASE_CONFIG.copy()
                output_name = f"{RESULTS_FILE_PREFIX}_beta_{beta}_seed_{seed}.csv"
                p.update({
                    "instance": inst, 
                    "seed": seed, 
                    "beta": beta, 
                    "output": output_name
                })
                tasks.append(p)

    # ==========================================
    # 3. Configurar tarefas de Perturbation Limit
    # ==========================================
    limits_to_test = [250, 1000, 750]
    for inst in INSTANCES:
        for limit in limits_to_test:
            for seed in SEEDS:
                p = BASE_CONFIG.copy()
                output_name = f"{RESULTS_FILE_PREFIX}_limit_{limit}_seed_{seed}.csv"
                p.update({
                    "instance": inst, 
                    "seed": seed, 
                    "p_limit": limit, 
                    "output": output_name
                })
                tasks.append(p)

    # ==========================================
    # 4. Configurar tarefas de Perturbation Strength
    # ==========================================
    strengths_to_test = [0.0, 0.2, 0.3, 0.4, 0.5, 1.0]
    for inst in INSTANCES:
        for strength in strengths_to_test:
            for seed in SEEDS:
                p = BASE_CONFIG.copy()
                output_name = f"{RESULTS_FILE_PREFIX}_strength_{strength}_seed_{seed}.csv"
                p.update({
                    "instance": inst, 
                    "seed": seed, 
                    "p_strength": strength, 
                    "output": output_name
                })
                tasks.append(p)

    print(f"Total de tarefas na fila: {len(tasks)}")
    
    # Executa em Paralelo
    # Dica: Se seu PC começar a travar por falta de memória RAM (100 instâncias x Threads), 
    # reduza max_workers manualmente, ex: max_workers=4
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = [executor.submit(run_command, task) for task in tasks]
        
        # Barra de progresso simples
        completed = 0
        for future in as_completed(futures):
            completed += 1
            if completed % 100 == 0:
                print(f"Progresso: {completed}/{len(tasks)} tarefas concluídas...")
            try:
                future.result()
            except Exception as exc:
                print(f'Tarefa gerou exceção: {exc}')

    elapsed = time.time() - start_time
    print(f"\nFIM! Tempo total: {elapsed:.2f} segundos.")

if __name__ == "__main__":
    main()
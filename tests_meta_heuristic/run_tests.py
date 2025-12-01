import subprocess
import os
import time

# --- CONFIGURAÇÃO GERAL ---
EXECUTABLE = "../build/Release/tabu_ecp.exe"  
RESULTS_FILE = "resultados_calibracao_5_minutos.csv"

# Instâncias de Calibração
INSTANCES = [
    "../instances/dlnb01.dat",
    "../instances/dlnb02.dat",
    "../instances/dlnb03.dat",
    "../instances/dlnb04.dat",
    "../instances/dlnb05.dat",
    "../instances/dlnb06.dat",
    "../instances/dlnb07.dat",
    "../instances/dlnb08.dat",
    "../instances/dlnb09.dat",
    "../instances/dlnb10.dat",
    "../instances/dlnb11.dat"
]

# Configuração Base
BASE_CONFIG = {
    "alpha": 0.6,
    "beta": 10,
    "p_limit": 1000,
    "p_strength": 0.2,
    "aspiration": 1,
    "time_limit": 300, # 5 minutos, com 5 seeds = 25 minutos por instância. São 11 instâncias = 275 minutos (~4.58 horas). Isso para cada configuração de parâmetro. São 16 configurações = ~73 horas no total.
    "max_iter": 10000000
}

SEEDS = [10, 20, 30, 40, 50] 

# DEFINIÇÃO DOS EXPERIMENTOS

def run_command(params):
    """Monta e executa o comando C++"""
    cmd = [
        EXECUTABLE,
        RESULTS_FILE,
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
    
    # Executa e aguarda
    try:
        # print(f"Running: {' '.join(cmd)}")
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Erro ao executar: {e}")
    except FileNotFoundError:
        print(f"Executável não encontrado: {EXECUTABLE}")
        exit(1)

def main():
    # Limpa arquivo de resultados anterior (opcional)
    if os.path.exists(RESULTS_FILE):
        os.remove(RESULTS_FILE)

    print(f"Iniciando Bateria de Testes...")
    print(f"Resultados serão salvos em: {RESULTS_FILE}")

    total_runs = 0
    start_time = time.time()

    # ==========================================
    # ETAPA 1: CALIBRAÇÃO DE ALPHA
    # Variamos Alpha, mantendo o resto fixo no Base
    # ==========================================
    print("\n--- ETAPA 1: Variando ALPHA ---")
    alphas_to_test = [0, 0.3, 0.6, 0.9, 1.2]
    
    for inst in INSTANCES:
        for alpha in alphas_to_test:
            for seed in SEEDS:
                # Copia a base e modifica o que queremos testar
                current_params = BASE_CONFIG.copy()
                current_params["instance"] = inst
                current_params["seed"] = seed
                current_params["alpha"] = alpha # VARIAÇÃO
                
                print(f"\r[Exp Alpha] Inst: {inst} | Alpha: {alpha} | Seed: {seed}...", end="")
                run_command(current_params)
                total_runs += 1

    # ==========================================
    # ETAPA 2: CALIBRAÇÃO DE BETA
    # Variamos Beta, usando Alpha Base
    # ==========================================
    print("\n\n--- ETAPA 2: Variando BETA ---")
    betas_to_test = [0, 20]
    
    for inst in INSTANCES:
        for beta in betas_to_test:
            for seed in SEEDS:
                current_params = BASE_CONFIG.copy()
                current_params["instance"] = inst
                current_params["seed"] = seed
                current_params["beta"] = beta # VARIAÇÃO
                
                print(f"\r[Exp Beta] Inst: {inst} | Beta: {beta} | Seed: {seed}...", end="")
                run_command(current_params)
                total_runs += 1

    # ==========================================
    # ETAPA 3: CALIBRAÇÃO PERTURBAÇÃO (LIMIT)
    # ==========================================
    print("\n\n--- ETAPA 3: Variando PERTURBATION LIMIT ---")
    limits_to_test = [250, 500, 750]
    
    for inst in INSTANCES:
        for limit in limits_to_test:
            for seed in SEEDS:
                current_params = BASE_CONFIG.copy()
                current_params["instance"] = inst
                current_params["seed"] = seed
                current_params["p_limit"] = limit # VARIAÇÃO
                
                print(f"\r[Exp Limit] Inst: {inst} | Limit: {limit} | Seed: {seed}...", end="")
                run_command(current_params)
                total_runs += 1
    
    # ==========================================
    # ETAPA 4: CALIBRAÇÃO PERTURBAÇÃO (STRENGTH)
    # ==========================================
    print("\n\n--- ETAPA 4: Variando PERTURBATION STRENGTH ---")
    strengths_to_test = [0.0, 0.1, 0.3, 0.4, 0.5]

    for inst in INSTANCES:
        for strength in strengths_to_test:
            for seed in SEEDS:
                current_params = BASE_CONFIG.copy()
                current_params["instance"] = inst
                current_params["seed"] = seed
                current_params["p_strength"] = strength
                
                print(f"\r[Exp Strength] Inst: {inst} | Strength: {strength} | Seed: {seed}...", end="")
                run_command(current_params)
                total_runs += 1

    elapsed = time.time() - start_time
    print(f"\n\nFIM! {total_runs} testes executados em {elapsed:.2f} segundos.")
    print(f"Analise o arquivo '{RESULTS_FILE}' no Excel/Pandas para escolher os melhores parametros.")

if __name__ == "__main__":
    main()
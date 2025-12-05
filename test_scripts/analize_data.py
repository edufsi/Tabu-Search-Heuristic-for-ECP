import pandas as pd
import glob
import os
import numpy as np

# --- CONFIGURAÇÃO ---
# Padrão para encontrar os arquivos (ex: "resultados_*.csv" ou apenas "*.csv" na pasta de output)
FILE_PATTERN = "*.csv" 
OUTPUT_DIR = "resultados_instancias_aleatorias_10_segundos_com_otimizacao"
INSTANCES_DIR = "instancias_aleatorias"

INSTANCES_FILES = glob.glob(os.path.join(INSTANCES_DIR, "*.txt"))

# --- DICIONÁRIO DE ÓTIMOS ---
OPTIMAL_K_VALUES = dict()

for result in INSTANCES_FILES:
    filename = os.path.basename(result)
    parts = filename.replace(".txt", "").split("_")

    if len(parts) >= 5:
        optimal_k = parts[4]
        try:
            OPTIMAL_K_VALUES[filename] = int(optimal_k)
            print(f"Carregado ótimo para {filename}: {optimal_k}")
        except ValueError:
            print(f"Aviso: Valor ótimo inválido em {filename}: {optimal_k}")


def load_data():
    """Carrega e concatena todos os arquivos de resultado."""
    all_files = glob.glob(os.path.join(OUTPUT_DIR, FILE_PATTERN))
    
    if not all_files:
        print("Nenhum arquivo encontrado com o padrão:", FILE_PATTERN)
        return pd.DataFrame()

    df_list = []
    print(f"Encontrados {len(all_files)} arquivos. Processando...")

    for filename in all_files:
        try:
            temp_df = pd.read_csv(filename, sep=';', engine='python')
                        
            df_list.append(temp_df)
        except Exception as e:
            print(f"Erro ao ler {filename}: {e}")

    if not df_list:
        return pd.DataFrame()

    # Junta tudo num dataframe só
    full_df = pd.concat(df_list, ignore_index=True)
    
    # Limpeza: Normalizar nome da instância (remover caminhos ../instances/)
    # Se a coluna for "Instance", pega só o nome do arquivo
    if 'Instance' in full_df.columns:
        full_df['Instance_Name'] = full_df['Instance'].apply(lambda x: os.path.basename(str(x)))
    
    return full_df

def calculate_metrics(df):
    """Calcula gap de otimalidade e outras métricas derivadas."""
    
    # Função auxiliar para buscar o ótimo
    def get_optimal(instance_name):
        return OPTIMAL_K_VALUES.get(instance_name, np.nan)

    # Adiciona coluna de Ótimo
    df['K_Optimal'] = df['Instance_Name'].apply(get_optimal)

    # Cálculo da % de Otimalidade Atingida
    # Fórmula: (K_Otimo / K_Final). Se for 1.0 (100%), atingiu o ótimo.
    df['Optimality_Ratio'] = df.apply(
        lambda row: (row['K_Optimal'] / row['SF']) * 100 if pd.notnull(row['K_Optimal']) and row['SF'] > 0 else np.nan, 
        axis=1
    )

    # Cálculo do Gap Absoluto (Distância do Ótimo)
    df['Gap_Abs'] = df['SF'] - df['K_Optimal']

    return df

def generate_report(df):
    """Gera tabelas dinâmicas e resumos estatísticos."""
    
    # Colunas que definem a "Configuração" (Parâmetros variados)
    # Removemos Seed, Instance e métricas de resultado
    config_cols = ['Alpha', 'Beta', 'P_Limit', 'P_Str', 'Asp']
    
    # Verifica quais dessas colunas realmente existem no CSV
    valid_config_cols = [c for c in config_cols if c in df.columns]
    
    print("\n--- RESUMO GERAL ---")
    print(f"Total de Execuções: {len(df)}")
    print(f"Instâncias únicas: {df['Instance_Name'].nunique()}")
    
    if 'Optimality_Ratio' in df.columns:
        missing_opt = df['Optimality_Ratio'].isna().sum()
        if missing_opt > 0:
            print(f"AVISO: {missing_opt} execuções sem valor ótimo conhecido (adicione ao dicionário OPTIMAL_K_VALUES).")

    # --- ANÁLISE 1: Agrupado por Configuração (Média das Seeds e Instâncias) ---
    # Aqui calculamos a performance média geral de cada configuração de parâmetros
    
    summary = df.groupby(valid_config_cols).agg({
        'SF': ['mean', 'std'],            # Solução Final (K)
        'Optimality_Ratio': ['mean'],     # Quão perto do ótimo (em %)
        'TotalIter': ['mean'],            # Esforço computacional
        'Time(s)': ['mean']               # Tempo
    }).reset_index()

    # Achatando o MultiIndex das colunas para facilitar leitura
    summary.columns = ['_'.join(col).strip() if col[1] else col[0] for col in summary.columns.values]
    
    # Ordenar por Melhor Otimalidade (maior é melhor) e Menor Tempo
    if 'Optimality_Ratio_mean' in summary.columns:
        summary = summary.sort_values(by=['Optimality_Ratio_mean', 'Time(s)_mean'], ascending=[False, True])
    else:
        summary = summary.sort_values(by=['SF_mean', 'Time(s)_mean'], ascending=[True, True])

    print("\n--- RANKING DE CONFIGURAÇÕES (Top 10) ---")
    # Formatação para print bonito
    print(summary.head(10).to_string(index=False, float_format="%.2f"))

    # Salva o resumo
    summary.to_csv("analise_resumo_configuracoes.csv", index=False, sep=';')
    print("\n-> Resumo por configuração salvo em 'analise_resumo_configuracoes.csv'")

    # --- ANÁLISE 2: Detalhe por Instância para a Melhor Configuração ---
    # Pega a melhor configuração (primeira linha do summary)
    best_config = summary.iloc[0]
    print(f"\n--- DETALHE DA MELHOR CONFIGURAÇÃO ---")
    
    # Filtra o DF original para pegar apenas as linhas dessa config
    # Isso é um pouco chato dinamicamente, vamos tentar simplificar
    # Vamos assumir que o usuário quer ver a média por instância de TUDO
    
    instance_summary = df.groupby(['Instance_Name'] + valid_config_cols).agg({
        'SF': 'mean',
        'Gap_Abs': 'mean',
        'Time(s)': 'mean',
        'TotalIter': 'mean'
    }).reset_index()
    
    instance_summary.to_csv("analise_detalhada_instancias.csv", index=False, sep=';')
    print("-> Detalhe por instância/config salvo em 'analise_detalhada_instancias.csv'")

def main():
    df = load_data()
    
    if df.empty:
        print("Nenhum dado carregado.")
        return

    # Processamento
    df = calculate_metrics(df)
    
    # Salva o "Tabelão" unificado e processado
    df.to_csv("dataset_completo_processado.csv", index=False, sep=';')
    print("-> Dataset completo salvo em 'dataset_completo_processado.csv'")
    
    # Gera relatórios
    generate_report(df)

if __name__ == "__main__":
    main()

#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <string>
#include <algorithm> // Para std::shuffle
#include <numeric>   // Para std::iota

// Estrutura para garantir que o usuário não faça bobagem com os parâmetros
void generate_equitable_instance(int n, int k_target, double density, int seed, std::string filename) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // 1. Garantir Partições Equilibradas (Hard Constraint)
    // Criamos um vetor onde cada cor aparece exatamente o número de vezes necessário
    std::vector<int> real_color;
    real_color.reserve(n);

    for (int i = 0; i < n; ++i) {
        // A mágica do módulo: distribui i entre 0..k-1 ciclicamente.
        // Isso garante que a diferença de tamanho entre as classes seja no máximo 1.
        real_color.push_back(i % k_target); 
    }

    // 2. Embaralhar as cores
    // Sem isso, o vértice 0 sempre teria a cor 0, vértice 1 a cor 1, etc.
    // O shuffle mantém as contagens exatas (equidade), mas distribui aleatoriamente.
    std::shuffle(real_color.begin(), real_color.end(), rng);

    // DEBUG: Verificar tamanhos das classes (Sanity Check)
    std::vector<int> counts(k_target, 0);
    for(int c : real_color) counts[c]++;
    
    std::cout << "--- Gerando " << filename << " ---\n";
    std::cout << "Target K=" << k_target << ". Tamanhos das classes: ";
    bool balanced = true;
    int min_s = n, max_s = 0;
    for(int c : counts) {
        std::cout << c << " ";
        if(c < min_s) min_s = c;
        if(c > max_s) max_s = c;
    }
    std::cout << "\n";
    
    if (max_s - min_s > 1) {
        std::cerr << "ERRO CRITICO: Particoes nao estao equilibradas! O gerador falhou.\n";
        exit(1);
    }

    // 3. Gerar arestas
    // Só adicionamos aresta se os vértices tiverem cores DIFERENTES (respeitando a partição)
    std::vector<std::pair<int, int>> edges;
    // Estimativa para reserve (opcional, melhora performance)
    edges.reserve((size_t)(n * (n-1) / 2 * density));

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            // Regra Fundamental: Vértices da mesma cor NÃO podem ter aresta (Independent Set)
            if (real_color[i] != real_color[j]) {
                // Se são de cores diferentes, jogamos a moeda baseada na densidade
                if (dist(rng) < density) {
                    // +1 porque o formato padrão DIMACS costuma ser 1-based (opcional)
                    // Seu parser lê e converte 'a-1', então aqui salvamos como 1-based para ser compatível
                    edges.push_back({i + 1, j + 1}); 
                }
            }
        }
    }

    // 4. Salvar em arquivo
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Erro ao criar arquivo " << filename << "\n";
        return;
    }
    
    // Header padrão: Número de Vértices, Número de Arestas
    out << n << " " << edges.size() << "\n";
    
    for (const auto& e : edges) {
        out << e.first << " " << e.second << "\n";
    }
    out.close();

    std::cout << "Arquivo gerado com sucesso. Arestas: " << edges.size() << "\n\n";
}

int main() {
    // ==========================================
    // CONFIGURAÇÃO DOS CENÁRIOS DE CALIBRAÇÃO
    // ==========================================
    
    std::mt19937 rng(seed);

    // Gera uma série de 100 instâncias com entre 100 e 200 vértices, variando k entre 5 e 30 e com densidades variadas.
    for (int i = 0; i < 100; ++i) {
        int n = 100 + (i % 101); // 100 a 200
        int k = 5 + (i % 26);    // 5 a 30
        double density = 0.1 + ((i % 10) * 0.1); // 0.1 a 1.0
        int seed = 2000 + i;
        std::string filename = "calib_instance_" + std::to_string(i+1) + ".txt";
        
        generate_equitable_instance(n, k, density, seed, filename);
    }

    // Cenário 1: Fácil (Esparso, muitas cores permitidas)
    // N=500, K=50 (Tamanho das classes = 10). Densidade 20%
    //generate_equitable_instance(500, 50, 0.2, 1001, "calib_easy.txt");

    // Cenário 2: Médio (Típico)
    // N=500, K=25 (Tamanho das classes = 20). Densidade 50%
   // generate_equitable_instance(500, 25, 0.5, 1002, "calib_medium.txt");

    // Cenário 3: Difícil (Denso, poucas cores = classes grandes e "apertadas")
    // N=500, K=10 (Tamanho das classes = 50). Densidade 60%
    // Aqui o Tabu Search vai sofrer para encontrar a combinação exata sem conflitos
    //generate_equitable_instance(500, 10, 0.6, 1003, "calib_hard.txt");

    return 0;
}
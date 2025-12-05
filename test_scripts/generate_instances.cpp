#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <string>
#include <algorithm> 
#include <numeric>   

// Gera um Grafo com K exato usando Partição Plantada + Clique Plantado
// - Garante Chi(G) <= K (pela partição)
// - Garante Chi(G) >= K (pelo clique de tamanho K embutido)
// - Permite densidade variável
void generate_exact_k_instance(int n, int k_target, double density, int seed, std::string filename) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // 1. Distribuir vértices nas partições (Equidade garantida)
    std::vector<int> real_color;
    real_color.reserve(n);

    // Vetores auxiliares para saber quem pertence a qual cor (para plantar o clique)
    std::vector<std::vector<int>> nodes_by_color(k_target);

    // Gera lista de índices embaralhada para não viciar a ordem
    std::vector<int> p(n);
    std::iota(p.begin(), p.end(), 0);
    std::shuffle(p.begin(), p.end(), rng);

    // Atribui cores de forma balanceada
    real_color.assign(n, 0);
    for (int i = 0; i < n; ++i) {
        int v = p[i];
        int c = i % k_target;
        real_color[v] = c;
        nodes_by_color[c].push_back(v);
    }

    // 2. Escolher representantes para o Clique (garantia de limite inferior)
    std::vector<int> clique_nodes;
    for (int c = 0; c < k_target; ++c) {
        if (!nodes_by_color[c].empty()) {
            // Escolhe um vértice aleatório dessa cor
            std::uniform_int_distribution<int> d_idx(0, nodes_by_color[c].size() - 1);
            int chosen = nodes_by_color[c][d_idx(rng)];
            clique_nodes.push_back(chosen);
        }
    }

    // Marca quem é do clique para busca rápida (O(1))
    std::vector<bool> is_in_clique(n, false);
    for(int v : clique_nodes) is_in_clique[v] = true;

    // 3. Gerar arestas
    std::vector<std::pair<int, int>> edges;
    // Estimativa de reserva
    edges.reserve((size_t)(n * (n-1) / 2 * density)); 

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            // Só pode haver aresta entre cores diferentes
            if (real_color[i] != real_color[j]) {
                
                bool must_add = false;
                
                // Se ambos fazem parte do clique plantado, TEM que ter aresta
                if (is_in_clique[i] && is_in_clique[j]) {
                    must_add = true;
                }
                
                // Se deve adicionar (clique) OU sorteio da densidade
                if (must_add || dist(rng) < density) {
                    // Input 1-based para DIMACS
                    edges.push_back({i + 1, j + 1}); 
                }
            }
        }
    }

    // 4. Salvar
    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Erro ao criar arquivo " << filename << "\n";
        return;
    }
    
    out << n << " " << edges.size() << "\n";
    for (const auto& e : edges) {
        out << e.first << " " << e.second << "\n";
    }
    out.close();

    std::cout << "Gerado [EXATO K=" << k_target << ", Dens=" << density << "]: " << filename << " (" << edges.size() << " arestas)\n";
}

int main() {
    // ==========================================
    // GERADOR DE INSTÂNCIAS COM K EXATO E DENSIDADE VARIÁVEL
    // ==========================================
    
    // 1. Bateria Aleatória (100 instâncias)
    // Variamos N, K e agora a Densidade também
    // Gera 50 instâncias "Grandes"
    for (int i = 0; i < 20; ++i) {
        int seed = 5000 + i; 
        std::mt19937 rng(seed);
        
        // 500 a 1000 vértices
        int n = rng() % 501 + 500; 
        
        int k = rng() % 46 + 5;    // 5 a 50 cores
        
        // Densidade entre 0.1 e 0.9
        std::uniform_real_distribution<double> d_dist(0.1, 0.9);
        double density = d_dist(rng);

        // Nome padronizado para o glob pegar depois
        // calib_instance_X_seed_Y_k_Z.txt
        std::string filename = "calib_instance_" + std::to_string(i+1) + 
                               "_" + std::to_string(seed) + 
                               "_" + std::to_string(k) + ".txt";
        
        generate_exact_k_instance(n, k, density, seed, filename);
    }

    // 2. Instâncias de Calibração Padrão (Com densidades controladas)
    // Fácil: Esparso
    //generate_exact_k_instance(500, 50, 0.2, 1001, "calib_easy.txt");   
    // Médio: Densidade média
    //generate_exact_k_instance(500, 25, 0.5, 1002, "calib_medium.txt"); 
    // Difícil: Denso (mas não completo)
    //generate_exact_k_instance(500, 10, 0.8, 1003, "calib_hard.txt");   

    return 0;
}
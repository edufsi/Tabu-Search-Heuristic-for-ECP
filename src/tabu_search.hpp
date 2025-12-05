// solution.hpp
// C++17 header-only: structures and incremental operations for TabuEQCol
// Save as: solution.hpp
//
// Usage example (sketch):
//   Instance inst = read_instance(...);
//   SolutionManager S(inst, K);                    // builds empty solution
//   auto init = greedy_balanced_initial(inst, K); // or any initial colors vector
//   S.compute_from_coloring(init);                // compute conflicts, f, C(s), sizes
//   S.move_vertex(v, newColor);                   // apply move (updates f, C(s), sizes)
//   S.exchange_vertices(v,u);                     // apply exchange
//   S.get_conflicting_vertices();                 // get C(s)
//

#pragma once
#include <vector>
#include <chrono>
#include <utility>
#include <algorithm>
#include <random>
#include <limits>
#include <cassert>
#include "stopCriterion.hpp"


struct TabuConfig {
    int max_iter = 10000;
    double alpha = 0.6;
    int beta = 10;
    int perturbation_limit = 1000; // iterações sem melhora para perturbar
    double perturbation_strength = 0.16; // percentagem de n vértices a perturbar
    int aspiration = 1; // 0 = off, 1 = on
};


struct CandidateMove {
    int type; // 0 move, 1 swap
    int v;
    int target; // u ou cor
};

struct TabuResult {
    bool solved;           // Se chegou a custo 0
    int iterations;        // Quantas iterações rodou
    long long final_obj;   // Valor da Solução Final (SF)
};

namespace tabueqcol {

// ------------------ Instance ------------------
struct Instance {
    int n = 0;
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<int>> adj;
    std::vector<int> degree;
    int max_degree = 0;

    Instance() = default;

    // Build adjacency after filling edges
    void build_adj() {
        adj.assign(n, {});
        degree.assign(n, 0);
        for (auto &e : edges) {
            int a = e.first;
            int b = e.second;
            // assume 0-based vertices; if input is 1-based, convert earlier
            adj[a].push_back(b);
            adj[b].push_back(a);
            degree[a]++; degree[b]++;
            if(degree[a] > max_degree) max_degree = degree[a];
            if(degree[b] > max_degree) max_degree = degree[b];
        }
    }
};

// ------------------ SolutionManager ------------------
struct SolutionManager {
    const Instance* inst = nullptr;
    int n = 0;
    int k = 0;

    // core state
    std::vector<int> color;        // color[v] in [0..k-1]
    std::vector<int> classSize;    // classSize[c]
    std::vector<int> conflicts;    // conflicts[v] = #neighbors with same color
    // conflictingVertices + index for O(1) add/remove
    std::vector<int> conflictingVertices;
    std::vector<int> conflictingIndex; // -1 if not present
    // objective: number of conflicting edges (sum |E(Vi)|)
    long long obj = 0;

    // precomputed floor sizes for equity
    int floor_size = 0;
    int big_size = 0; // floor_size + 1

    // constructor: empty initialization for given instance and k
    SolutionManager() = default;
    SolutionManager(const Instance& I, int K = -1) {
        init(&I, K);
    }

    void init(const Instance* Iptr, int K = -1) {
        auto t1 = std::chrono::high_resolution_clock::now();
        inst = Iptr;
        n = inst->n;

        // Teorema de Hajnal-Szemerédi
        // Um grafo de grau máximo delta pode ser equitativamente colorido com delta + 1 cores
        // Começamos resolvendo o problema com k = delta + 1 cores
        if (K == -1) {
            k = Iptr->max_degree + 1;
        } else {
            k = K;
        }
        
        color.assign(n, -1);
        classSize.assign(k, 0);
        conflicts.assign(n, 0);
        conflictingVertices.clear();
        conflictingIndex.assign(n, -1);
        obj = 0;
        floor_size = n / k;
        big_size = floor_size + 1;

        auto t2 = std::chrono::high_resolution_clock::now();

    }


    void construct_greedy_initial(int seed = 0) {
        // 1. Limpar estado atual
        std::fill(color.begin(), color.end(), -1);
        std::fill(classSize.begin(), classSize.end(), 0);
        std::fill(conflicts.begin(), conflicts.end(), 0);
        conflictingVertices.clear();
        std::fill(conflictingIndex.begin(), conflictingIndex.end(), -1);
        obj = 0;

        // Constantes auxiliares
        int floor_nk = n / k;
        int max_r = n - k * floor_nk; // Número alvo de classes grandes
        int current_r = 0;            // Classes grandes atuais

        std::mt19937 rng(seed);

        // Vértices aleatórios
        std::vector<int> vertices(n);

        for(int i=0; i<n; ++i)
            vertices[i] = i;

        std::shuffle(vertices.begin(), vertices.end(), rng);

        for (int v : vertices) {
            // Regra do M: Se ainda precisamos de classes grandes, permitimos floor+1
            // Se já atingimos o limite de classes grandes, travamos em floor
            int M = (current_r < max_r) ? (floor_nk + 1) : floor_nk;

            // Construir conjunto I de candidatos viáveis por tamanho
            // Dica de performance: Para k pequeno isso é ok. Para k muito grande, 
            // manter listas de cores por tamanho seria mais rápido, mas aqui é inicialização.
            std::vector<int> I;
            I.reserve(k);
            for (int c = 0; c < k; ++c) {
                if (classSize[c] <= M - 1) {
                    I.push_back(c);
                }
            }

            int chosenColor = -1;

            // Tentar encontrar o menor i em I que não gera conflitos NOVOS para o vértice v
            // Nota: O artigo diz "E(Vi) is not incremented". Isso significa que o vértice v
            // não deve ter vizinhos na cor c.
            for (int c : I) {
                bool conflictFound = false;
                for (int u : inst->adj[v]) {
                    // Só checamos vizinhos que JÁ foram coloridos
                    if (color[u] == c) {
                        conflictFound = true;
                        break;
                    }
                }
                if (!conflictFound) {
                    chosenColor = c;
                    break;
                }
            }

            // Se não for possível, escolher aleatoriamente em I
            if (chosenColor == -1) {
                if (I.empty()) {
                    // Fallback de segurança (teoricamente não deve acontecer se K for viável pra equidade básica)
                    // Escolhe a cor com menor tamanho atual
                    chosenColor = 0; 
                    for(int c=1; c<k; ++c) if(classSize[c] < classSize[chosenColor]) chosenColor = c;
                } else {
                    std::uniform_int_distribution<int> dist(0, (int)I.size() - 1);
                    chosenColor = I[dist(rng)];
                }
            }

            // Atribuição
            color[v] = chosenColor;
            classSize[chosenColor]++;
            
            // Atualiza contagem de classes "grandes"
            if (classSize[chosenColor] == floor_nk + 1) {
                current_r++;
            }

            // ATUALIZAÇÃO INCREMENTAL DE CONFLITOS E OBJETIVO
            // Como acabamos de colorir v, verificamos seus vizinhos já coloridos
            for (int u : inst->adj[v]) {
                if (color[u] != -1) { // Vizinho já colorido
                    if (color[u] == chosenColor) {
                        // Novo conflito gerado na aresta (u, v)
                        obj++; // Aresta conflitante conta +1
                        
                        // Atualiza contadores de conflito dos vértices
                        conflicts[v]++;
                        conflicts[u]++;
                        
                        // Gerencia a lista de vértices conflitantes
                        if (conflicts[v] == 1 && conflictingIndex[v] == -1) {
                            conflictingIndex[v] = conflictingVertices.size();
                            conflictingVertices.push_back(v);
                        }
                        if (conflicts[u] == 1 && conflictingIndex[u] == -1) {
                            conflictingIndex[u] = conflictingVertices.size();
                            conflictingVertices.push_back(u);
                        }
                    }
                }
            }
        }
    }


// O objetivo é construir uma solução inicial para k cores a partir de uma solução conhecida para k+1 cores
    // Assume que 'this->k' é o alvo e 'prevSol.k' é k+1
    void construct_greedy_from_previous(const SolutionManager& prevSol, int seed = 0) {
        // 1. Limpeza e Validações
        std::fill(color.begin(), color.end(), -1);
        std::fill(classSize.begin(), classSize.end(), 0);
        std::fill(conflicts.begin(), conflicts.end(), 0);
        conflictingVertices.clear();
        std::fill(conflictingIndex.begin(), conflictingIndex.end(), -1);
        obj = 0;

        int prev_k = prevSol.k; // Esperado que seja k + 1
        
        // 2. Permutação e Mapeamento (Sua lógica estava correta)
        std::mt19937 rng(seed);
        std::vector<int> perm(prev_k); // Índices de 0 a k
        
        for (int i = 0; i < prev_k; ++i) perm[i] = i;

        std::shuffle(perm.begin(), perm.end(), rng);

        int removed_color = perm[prev_k - 1]; // Escolhemos o último da permutação para remover

        std::vector<int> color_map(prev_k, -1);
        int new_color_index = 0;
        for (int i = 0; i < prev_k; ++i) {
            // Mapeia todas as cores, exceto a removida, para o intervalo [0, k-1]
            if (i != removed_color) { // Usamos o índice original 'i', não perm[i], para verificar
                 // A lógica de permutação define qual COR será removida. 
                 // Se perm = {2, 0, 1} e removemos o último (1), então mapped: 2->0, 0->1.
            }
        }
        
        // CORREÇÃO DA LÓGICA DE PERMUTAÇÃO:
        // É mais simples pensar assim: perm[0]..perm[k-1] são as cores que ficam. perm[k] é a que sai.
        // Vamos remapear perm[0]->0, perm[1]->1, ...
        removed_color = perm[prev_k - 1]; // A cor que estava no índice original 'removed_color' sai.
        
        // Refazendo o mapa para garantir consistência
        std::fill(color_map.begin(), color_map.end(), -1);
        int current_target_color = 0;
        for (int i = 0; i < prev_k - 1; ++i) {
            color_map[perm[i]] = current_target_color++;
        }
        // Nota: color_map[removed_color] continua -1

        std::vector<int> uncolored_vertices;
        uncolored_vertices.reserve(n / k + 1);

        // 3. Transferência e Identificação dos Órfãos
        for (int v = 0; v < n; ++v) {
            int old_c = prevSol.color[v];
            if (old_c == removed_color) {
                uncolored_vertices.push_back(v);
            } else {
                int new_c = color_map[old_c];
                color[v] = new_c;
                classSize[new_c]++;
            }
        }



// 4. (OTIMIZADO) Transferência de Estado de Conflitos
        // Em vez de recalcular tudo olhando arestas, confiamos no prevSol
        
        this->obj = prevSol.obj; // Começamos com o custo total anterior

        for (int v = 0; v < n; ++v) {
            int old_c = prevSol.color[v];
            
            if (old_c == removed_color) {
                // Se o vértice pertencia à cor removida, ele agora é -1.
                // Se ele tinha conflitos (estava ligado a outro vértice da MESMA cor removida),
                // esses conflitos deixam de existir temporariamente.
                
                // CUIDADO: O obj conta arestas. O conflicts conta vizinhos.
                // Cada aresta conflitante contribui +1 para o 'conflicts' de DOIS vértices.
                // Logo, a contribuição total desse vértice para a soma de conflitos é prevSol.conflicts[v].
                // Mas para o 'obj' (arestas), devemos subtrair metade disso? 
                // NÃO. É mais seguro subtrair iterando as arestas SÓ DENTRO da classe removida 
                // para não subtrair duas vezes a mesma aresta.
                
                // Abordagem segura para corrigir o 'obj':
                // Subtraímos as arestas conflitantes onde v < u (para contar 1x cada aresta)
                if (prevSol.conflicts[v] > 0) {
                     for (int u : inst->adj[v]) {
                        // Se u também era da cor removida e u > v, essa aresta contava no obj
                        if (u > v && prevSol.color[u] == removed_color) {
                            this->obj--; 
                        }
                     }
                }

                // O vértice v agora está sem cor, então conflitos são 0
                this->conflicts[v] = 0; 
                this->conflictingIndex[v] = -1; // Não está na lista de conflitos
                
            } else {
                // Vértice manteve a cor (apenas mapeada). O número de conflitos dele é IDÊNTICO.
                this->conflicts[v] = prevSol.conflicts[v];
                
                // Se ele tem conflitos, precisamos adicioná-lo à lista de controle de vértices conflitantes
                if (this->conflicts[v] > 0) {
                    this->conflictingIndex[v] = this->conflictingVertices.size();
                    this->conflictingVertices.push_back(v);
                } else {
                    this->conflictingIndex[v] = -1;
                }
            }
        }

        // 5. Procedimento Guloso para os vértices restantes (Procedure 1)
        // Embaralha para evitar viés na ordem de inserção

        int floor_nk = n / k;
        int max_r = n - k * floor_nk; 

        int current_r = 0;
        for (int c = 0; c < k; ++c) {
            if (classSize[c] >= floor_nk + 1) { // >= para segurança, mas deve ser ==
                current_r++;
            }
        }


        std::shuffle(uncolored_vertices.begin(), uncolored_vertices.end(), rng);

        for (int v : uncolored_vertices) {
            // Define M (teto atual permitido)
            int M = (current_r < max_r) ? (floor_nk + 1) : floor_nk;

            // Define I: classes que aceitam mais vértices
            std::vector<int> I;
            for (int c = 0; c < k; ++c) {
                if (classSize[c] <= M - 1) {
                    I.push_back(c);
                }
            }

            int chosenColor = -1;

            // Tenta encontrar cor em I que não aumente conflitos
            for (int c : I) {
                bool wouldIncrement = false;
                for (int u : inst->adj[v]) {
                    if (color[u] == c) {
                        wouldIncrement = true;
                        break;
                    }
                }
                if (!wouldIncrement) {
                    chosenColor = c;
                    break;
                }
            }

            // Se falhar, escolhe aleatório em I
            if (chosenColor == -1) {
                if (I.empty()) {
                   // Fallback extremo (não deve ocorrer se matemática estiver ok)
                   chosenColor = 0; 
                } else {
                    std::uniform_int_distribution<int> dist(0, (int)I.size() - 1);
                    chosenColor = I[dist(rng)];
                }
            }

            // Atribui
            color[v] = chosenColor;
            classSize[chosenColor]++;
            if (classSize[chosenColor] == floor_nk + 1) {
                current_r++;
            }

            // Atualiza conflitos incrementais (apenas para o vértice recém inserido)
            for (int u : inst->adj[v]) {
                if (color[u] != -1) { // Vizinho já colorido (herdado ou recém inserido)
                    if (color[u] == chosenColor) {
                        obj++; 
                        conflicts[v]++;
                        conflicts[u]++;
                        
                        if (conflicts[v] == 1 && conflictingIndex[v] == -1) {
                            conflictingIndex[v] = conflictingVertices.size();
                            conflictingVertices.push_back(v);
                        }
                        if (conflicts[u] == 1 && conflictingIndex[u] == -1) {
                            conflictingIndex[u] = conflictingVertices.size();
                            conflictingVertices.push_back(u);
                        }
                    }
                }
            }
        }
    }


    // ---------- helper: recompute objective from scratch (for debug) ----------
    long long recompute_objective_slow() const {
        long long sum = 0;
        for (int v = 0; v < n; ++v) {
            for (int u : inst->adj[v]) {
                if (color[v] == color[u]) sum++;
            }
        }
        return sum / 2;
    }

    // ---------- debug/validation ----------
    bool validate_consistency() const {
        // check class sizes
        std::vector<int> cs(k,0);
        for (int v = 0; v < n; ++v) {
            int c = color[v];
            if (c < 0 || c >= k) return false;
            cs[c]++;
        }
        if (cs != classSize) return false;
        // check conflicts vector
        for (int v = 0; v < n; ++v) {
            int cnt = 0;
            for (int u : inst->adj[v]) if (color[u] == color[v]) ++cnt;
            if (cnt != conflicts[v]) return false;
        }
        // check obj
        if (obj != recompute_objective_slow()) return false;
        // check conflictingVertices list
        std::vector<int> mark(n,0);
        for (int v : conflictingVertices) {
            mark[v] = 1;
            if (conflictingIndex[v] == -1) return false;
        }
        for (int v = 0; v < n; ++v) {
            if (conflicts[v] > 0 && mark[v] == 0) return false;
            if (conflicts[v] == 0 && mark[v] == 1) return false;
        }
        return true;
    }


    // Matriz Tabu: tabu_matrix[v][c] guarda a iteração até a qual v está proibido de ir para c
    std::vector<std::vector<int>> tabu_matrix;

    // Inicializa estrutura Tabu (chamar antes de começar o solve)
    void init_tabu() {
        // Aloca n x k preenchido com 0
        tabu_matrix.assign(n, std::vector<int>(k, 0));
    }

    // ==============================================================
    // CÁLCULO DE DELTA (AVALIAÇÃO DE CUSTO)
    // ==============================================================

    // Calcula variação de conflitos ao mover v de old_c para new_c
    // Complexidade: O(grau(v))
    int get_move_delta(int v, int old_c, int new_c) const {
        int delta = 0;
        for (int u : inst->adj[v]) {
            int c_u = color[u];
            if (c_u == -1) continue;
            
            if (c_u == old_c) delta--; // Removeria um conflito existente
            else if (c_u == new_c) delta++; // Criaria um novo conflito
        }
        return delta;
    }

    // Calcula variação de conflitos ao trocar cores entre v (cor c_v) e u (cor c_u)
    // Complexidade: O(grau(v) + grau(u))
    int get_swap_delta(int v, int u) const {
        int c_v = color[v];
        int c_u = color[u];
        
        // Se tiverem a mesma cor, custo não muda (movimento inútil que não deveria acontecer)
        if (c_v == c_u) return 0;

        int delta = 0;

        // 1. Variação para v (saindo de c_v indo para c_u)
        // Ignoramos 'u' aqui temporariamente para tratar aresta (u,v) separadamente
        for (int w : inst->adj[v]) {
            if (w == u) continue; // Trata aresta direta depois
            int c_w = color[w];
            if (c_w == c_v) delta--; // Perde conflito atual
            else if (c_w == c_u) delta++; // Ganha conflito novo
        }

        // 2. Variação para u (saindo de c_u indo para c_v)
        for (int w : inst->adj[u]) {
            if (w == v) continue;
            int c_w = color[w];
            if (c_w == c_u) delta--; // Perde conflito atual
            else if (c_w == c_v) delta++; // Ganha conflito novo
        }

        // A aresta (u, v) nunca gera ou remove conflito no SWAP de cores distintas.
        // Antes: v(c_v) - u(c_u). Diferentes. Sem conflito.
        // Depois: v(c_u) - u(c_v). Diferentes. Sem conflito.
        // (Assumindo que c_v != c_u, que já checamos no início)
        
        return delta;
    }

    // ==============================================================
    // APLICAÇÃO DE MOVIMENTOS
    // ==============================================================

    // Atualiza estruturas de dados após mover v de old_c para new_c
    void apply_move(int v, int new_c) {
        int old_c = color[v];
        
        // 1. Atualiza cor e tamanhos
        color[v] = new_c;
        classSize[old_c]--;
        classSize[new_c]++;

        // 2. Atualiza conflitos dos vizinhos e do próprio v
        // Precisamos recalcular conflitos de v do zero ou incrementalmente?
        // Incremental é mais seguro se feito com cuidado.
        
        // Primeiro, limpamos os conflitos que v tinha na cor antiga
        // E removemos v da contagem de conflitos dos vizinhos
        for (int u : inst->adj[v]) {
            if (color[u] == old_c) {
                // v e u colidiam. Agora não colidem mais.
                obj--; // Aresta resolvida
                conflicts[v]--; 
                update_conflict_status(v);
                
                conflicts[u]--;
                update_conflict_status(u);
            }
        }

        // Agora adicionamos os conflitos na nova cor
        for (int u : inst->adj[v]) {
            if (color[u] == new_c) {
                // v e u agora colidem
                obj++;
                conflicts[v]++;
                update_conflict_status(v);

                conflicts[u]++;
                update_conflict_status(u);
            }
        }
    }


    // Helper auxiliar lento mas seguro para SWAP (chamar só dentro do apply_swap)
    // Para implementação de alta performance, expanda a lógica incremental.
    void apply_swap_safe(int v, int u) {

        int c_v = color[v];
        int c_u = color[u];
        
        // Move v para c_u (temporariamente classSize desbalanceia, mas ok)
        apply_move(v, c_u); 
        // Agora v tem cor c_u.
        
        // Move u para c_v
        apply_move(u, c_v);
        
        // ClassSize voltou ao normal?
        // v saiu de c_v (-1), entrou em c_u (+1)
        // u saiu de c_u (-1), entrou em c_v (+1)
        // Saldo zero. Correto.
    }

    // Gerencia adição/remoção do vetor conflictingVertices em O(1)
    void update_conflict_status(int x) {
        if (conflicts[x] > 0) {
            if (conflictingIndex[x] == -1) {
                // x tem conflitos mas não está na lista de vértices conflitantes, então o adiciona
                conflictingIndex[x] = conflictingVertices.size();
                conflictingVertices.push_back(x);
            }
        } else {
            if (conflictingIndex[x] != -1) {
                // Remove O(1): swap com o último
                int idx = conflictingIndex[x];
                int lastVal = conflictingVertices.back();
                
                conflictingVertices[idx] = lastVal;
                conflictingIndex[lastVal] = idx;
                
                conflictingVertices.pop_back();
                conflictingIndex[x] = -1;
            }
        }
    }

    // ==============================================================
    // CORE DO TABU SEARCH
    // ==============================================================

    TabuResult run_tabu_search(const TabuConfig& config, const StopCriterion& stop, int seed = 0) {

        TabuResult result;
        result.solved = false;
        result.final_obj = obj;
        result.iterations = 0;


        if (obj == 0){
            result.solved = true;
            return result;
        } // Já está ótimo


        init_tabu();
        std::mt19937 rng(seed);
        int best_obj_found = obj;

        int iter = 0;
        int no_improve_iter = 0; // Para critério de parada extra se quiser


        // Para identificar W+ e W-
        // W+ são classes com tamanho >= floor_size + 1 (na prática, igual a big_size)
        // W- são classes com tamanho <= floor_size

        while (iter < config.max_iter && obj > 0) {
            if(iter % 128 == 0 && stop.is_time_up())
                return result;
            
            int best_delta = 99999999;
            int move_type = -1; // 0: Move, 1: Swap
            int best_v = -1, best_u_or_color = -1;
            
            std::vector<CandidateMove> candidates;

            // --- 1. Avaliar MOVE (Transfer) ---
            // Só possível se existem classes de tamanhos diferentes, ou seja, n % k != 0
            // Mas o artigo diz: "se n%k != 0, W+ não é vazio".
            // Movemos v de W+ para W-

            // Critério de perturbação simples: se nenhuma melhoria em X iterações, executa 
            if (no_improve_iter >= config.perturbation_limit && config.perturbation_strength > 0) {
                //printf("Perturbing solution at iter %d (stuck for %d iter)...\n", iter, no_improve_iter);
                
                // Executa, por exemplo, n/2 swaps totalmente aleatórios
                // Isso vai estragar o obj momentaneamente, mas tira do buraco
                for (int p = 0; p < n * config.perturbation_strength; ++p) {
                    std::uniform_int_distribution<int> d_n(0, n-1);
                    int v1 = d_n(rng);
                    int v2 = d_n(rng);
                    if (v1 != v2 && color[v1] != color[v2]) {
                        apply_swap_safe(v1, v2);
                    }
                }
                
                // Reseta o contador
                no_improve_iter = 0;
                iter++;
                // Opcional: Limpar a Tabu Matrix após perturbação para dar liberdade total
                init_tabu();
                
                // Continua o loop...
                continue;
            }
            
            bool can_do_transfer = (n % k != 0); 
            
            if (can_do_transfer) {
                // Candidatos: v pertencente a C(s) AND v está numa classe W+
                // Iterar sobre conflictingVertices
                for (int v : conflictingVertices) {
                    int c_v = color[v];
                    if (classSize[c_v] == big_size) { // v está em W+
                        // Tentar mover para qualquer j em W-
                        for (int j = 0; j < k; ++j) {
                            if (classSize[j] == floor_size) { // j está em W-
                                int delta = get_move_delta(v, c_v, j);
                                
                                // Tabu Check
                                bool is_tabu = (tabu_matrix[v][j] > iter);
                                // Aspiration Check
                                bool aspiration = (obj + delta < best_obj_found);
                                
                                if (!is_tabu || (aspiration && config.aspiration)) {


                                    if (delta < best_delta) {
                                        best_delta = delta;
                                        move_type = 0;
                                        best_v = v;
                                        best_u_or_color = j;
                                        candidates.clear(); // Limpa candidatos anteriores
                                        candidates.push_back({move_type, v, j}); // Adiciona o novo
                                    }
                                    else if (delta == best_delta) {
                                        best_delta = delta;
                                        move_type = 0;
                                        best_v = v;
                                        best_u_or_color = j;
                                        // Empate: adiciona como candidato
                                        candidates.push_back({move_type, v, j});
                                    }
                                    // Melhoria de primeiro nível (First Improvement) pode ser mais rápida?
                                    // Tabu geralmente usa Best Improvement na vizinhança.
                                }
                            }
                        }
                    }
                }
            }

            // --- 2. Avaliar SWAP (Exchange) ---
            // "Escolher v em C(s). Escolher u qualquer tal que (u not em C(s) OU color[u] < color[v])"
            // Essa restrição reduz pela metade a busca em pares conflitantes e evita simetria.
            
            // Dica de performance: Swap é O(N * |C(s)|). Isso pode ser pesado.
            // Se estiver lento, amostrar apenas uma parte dos vizinhos ou usar lista de candidatos.
            
            for (int v : conflictingVertices) {
                int c_v = color[v];
                
                // Iterar todos os vértices u para tentar troca
                // (Em implementações avançadas, iteraríamos apenas u que tem cores diferentes e adjacências relevantes)
                for (int u = 0; u < n; ++u) {
                    if (v == u) continue;
                    int c_u = color[u];
                    if (c_v == c_u) continue; // Mesma cor, inútil trocar

                    // Regra do artigo para evitar simetria e redundância
                    bool u_in_conflicts = (conflicts[u] > 0);
                    
                    // Se u também é conflitante, só checa se c_u < c_v para não testar o par (v,u) e depois (u,v)
                    if (u_in_conflicts && c_u > c_v) continue; 
                    
                    int delta = get_swap_delta(v, u);

                    // Tabu Check para Swap?
                    // Geralmente considera-se tabu se mover v para c_u OU mover u para c_v é tabu.
                    bool is_tabu = (tabu_matrix[v][c_u] > iter) || (tabu_matrix[u][c_v] > iter);
                    bool aspiration = (obj + delta < best_obj_found);

                    if (!is_tabu || aspiration) {
                        if (delta < best_delta) {
                            best_delta = delta;
                            move_type = 1;
                            best_v = v;
                            best_u_or_color = u;
                            candidates.clear();
                            candidates.push_back({move_type, v, u});
                        }
                        else if (delta == best_delta) {
                            best_delta = delta;
                            move_type = 1;
                            best_v = v;
                            best_u_or_color = u;
                            candidates.push_back({move_type, v, u});// Empate: adiciona como candidato
                        }
                    }
                }
            }

        

            // --- Escolher aleatoriamente entre os melhores candidatos ---
            if (!candidates.empty()) {
                // Escolhe aleatoriamente entre os melhores empatados
                std::uniform_int_distribution<int> cand_dist(0, candidates.size() - 1);
                CandidateMove chosen = candidates[cand_dist(rng)];
                
                // Configura as variáveis para execução
                move_type = chosen.type;
                best_v = chosen.v;
                best_u_or_color = chosen.target;
            } else {
                move_type = -1; // Nenhum movimento possível
            }


            // --- 3. Executar Movimento ---
            if (move_type != -1) {
                // Calcular Tenure
                int tenure;

                // alpha * |C(s)| + random(beta)
                std::uniform_int_distribution<int> dist(0, config.beta);
                //printf("Config.beta = %d\n", config.beta);
                tenure = (int)(config.alpha * conflictingVertices.size()) + dist(rng);
                //printf("%d\n", dist(rng));
                


                if (move_type == 0) {
                    // MOVE
                    int v = best_v;
                    int old_c = color[v];
                    int new_c = best_u_or_color;
                    
                    apply_move(v, new_c);
                    
                    // Atualizar Tabu: proibir v de voltar para old_c
                    tabu_matrix[v][old_c] = iter + tenure;

                } else {
                    // SWAP
                    int v = best_v;
                    int u = best_u_or_color;
                    int c_v_old = color[v]; // será nova cor de u
                    int c_u_old = color[u]; // será nova cor de v
                    
                    // Usa o apply_swap_safe que internamente chama 2 moves
                    apply_swap_safe(v, u);
                    
                    // Atualizar Tabu: proibir reverter
                    tabu_matrix[v][c_v_old] = iter + tenure;
                    tabu_matrix[u][c_u_old] = iter + tenure;
                }

                // Atualizar Melhor Global
                if (obj < best_obj_found) {
                    best_obj_found = obj;
                    no_improve_iter = 0;
                }
                else {
                    no_improve_iter++;
                }
            } else {
                // Travou (nenhum movimento não-tabu possível e sem aspiração)
                // Isso é raro com aspiração, mas pode acontecer se a vizinhança for vazia
                break; 
            }

            iter++;
    }
        
        result.iterations = iter;
        result.final_obj = best_obj_found;
        result.solved = (best_obj_found == 0);

        return result;

    }

}; // end SolutionManager

} 


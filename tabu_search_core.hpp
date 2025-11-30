// Adicione os parâmetros na sua struct ou passe na função
    // Sugestão: Crie uma struct para configurar a busca
    struct TabuConfig {
        int max_iter = 10000;
        int fixed_tenure = -1; // Se >= 0, usa fixo
        double alpha = 0.6;
        int beta = 10;
    };

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

    void apply_swap(int v, int u) {
        int c_v = color[v];
        int c_u = color[u];
        
        // Truque: um swap é tecnicamente dois moves, mas precisamos cuidar para não
        // corromper o estado no meio.
        // Maneira mais fácil: mudar a cor na memória e recalcular vizinhos afetados.
        
        color[v] = c_u;
        color[u] = c_v;
        // classSize não muda em swap!
        
        // Recalcular conflitos para v
        // Atenção: a cor de v já mudou para c_u. A cor de u já mudou para c_v.
        
        // É mais seguro (e rápido o suficiente) recalcular os conflitos locais 
        // desses dois vértices do que tentar fazer aritmética incremental complexa 
        // considerando a aresta (u,v).
        
        recalc_conflicts_local(v);
        recalc_conflicts_local(u);
        
        // Vizinhos de v e u também tiveram seus contadores de conflito alterados?
        // Sim. Se w era vizinho de v (cor A) e w tinha cor B.
        // v mudou para B. Agora w tem +1 conflito.
        // Precisamos visitar vizinhos.
        
        // Como a lógica incremental é chata no Swap, vou sugerir um helper robusto:
        // Mas para performance máxima, a lógica incremental manual é:
        
        // 1. Reverteu conflitos antigos?
        //    Para cada vizinho w de v:
        //       Se color[w] == c_v (antiga de v): obj--; conflicts[w]--; update(w);
        //       Se color[w] == c_u (nova de v):   obj++; conflicts[w]++; update(w);
        //    (Mesma coisa para u, mas cuidado com a aresta (u,v) para não contar 2x)
    }

    // Helper auxiliar lento mas seguro para SWAP (chamar só dentro do apply_swap)
    // Para implementação de alta performance, expanda a lógica incremental.
    void apply_swap_safe(int v, int u) {
        // Remove conflitos antigos da contabilidade global
        remove_vertex_conflicts_from_obj(v);
        remove_vertex_conflicts_from_obj(u); // Se v e u conectados, remove a aresta 2x? Cuidado.

        // Solução mais simples para Swap: 
        // 1. Identificar vizinhos afetados
        // 2. Atualizar cores
        // 3. Recalcular
        
        // Vamos usar a lógica de "Apply Move" duas vezes, mas cuidando do ClassSize
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

    bool run_tabu_search(const TabuConfig& config, const StopCriterion& stop, int seed = 0) {
        if (obj == 0) return true; // Já viável

        init_tabu();
        std::mt19937 rng(seed);
        int best_obj_found = obj;

        int iter = 0;
        int no_improve_iter = 0; // Para critério de parada extra se quiser

        // Para identificar W+ e W-
        // W+ são classes com tamanho >= floor_size + 1 (na prática, igual a big_size)
        // W- são classes com tamanho <= floor_size

        while (iter < config.max_iter && obj > 0) {
            if(iter % 128 == 0){
                if(stop.is_time_up()){
                    return false;
                }
            }

            int best_delta = 99999999;
            int move_type = -1; // 0: Move, 1: Swap
            int best_v = -1, best_u_or_color = -1;
            
            // --- 1. Avaliar MOVE (Transfer) ---
            // Só possível se existem classes de tamanhos diferentes, ou seja, n % k != 0
            // Mas o artigo diz: "se n%k != 0, W+ não é vazio".
            // Movemos v de W+ para W-


            
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
                                
                                if (!is_tabu || aspiration) {
                                    if (delta < best_delta) {
                                        best_delta = delta;
                                        move_type = 0;
                                        best_v = v;
                                        best_u_or_color = j;
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
                        }
                    }
                }
            }

            // --- 3. Executar Movimento ---
            if (move_type != -1) {
                // Calcular Tenure
                int tenure;
                if (config.fixed_tenure >= 0) {
                    tenure = config.fixed_tenure;
                } else {
                    // alpha * |C(s)| + random(beta)
                    std::uniform_int_distribution<int> dist(0, config.beta);
                    tenure = (int)(config.alpha * conflictingVertices.size()) + dist(rng);
                }

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
                }
            } else {
                // Travou (nenhum movimento não-tabu possível e sem aspiração)
                // Isso é raro com aspiração, mas pode acontecer se a vizinhança for vazia
                break; 
            }

            iter++;
        }

        return (obj == 0);
    }
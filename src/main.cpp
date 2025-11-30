// main.cpp - TabuCol scaffold for Equitable Coloring Problem (ECP)
// C++17 - single-file scaffold
// Build with: mkdir build && cd build && cmake .. && make
// Run example: ./tabu_ecp --inst instances/inst1.txt --K 10 --time 60 --seed 1 --tenure 7 --cand 50 --swap 0.2 --out run1.json

#include "args.hpp"
#include "tabu_search.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <utility>

using namespace std;




static void die(const string &msg);
tabueqcol::Instance read_instance(const string &path);
int compute_conflicts(const tabueqcol::Instance &inst, const vector<int> &color);
vector<int> color_sizes(const vector<int> &color, int K);





int main(int argc, char** argv) {
    try {
        Arguments args = parse_arguments(argc, argv);

        std::cout << "Input:  " << args.input_file << "\n";
        std::cout << "Output: " << args.output_file << "\n";
        std::cout << "seed = " << args.seed << "\n";
        std::cout << "alpha = " << args.alpha << "\n";
        std::cout << "beta = " << args.beta << "\n";
        std::cout << "tenure = " << args.tenure_type << "\n";
        std::cout << "aspiration = " << args.aspiration << "\n";
        std::cout << "time_limit = " << args.time_limit << "s\n";
        std::cout << "max_iter = " << args.max_iter << "\n";

        const tabueqcol::Instance inst = read_instance(args.input_file);
        printf("Instancia lida: %d vertices, %d arestas, grau max %d\n", inst.n, (int)inst.edges.size(), inst.max_degree);
        
        StopCriterion globalStop(args.time_limit);
        
        TabuConfig tabuConfig;
        tabuConfig.max_iter = args.max_iter; // Iterações máximas PER K (para não ficar preso infinitamente)
        tabuConfig.fixed_tenure = -1; // Tenure dinâmico
        tabuConfig.alpha = args.alpha;
        tabuConfig.beta = (int)args.beta;
        tabuConfig.perturbation_limit = args.perturbation_limit;

        printf("Stop Criterion e Tabu Config Inicializados!\n");

        // 1. Inicializa com K seguro (Hajnal-Szemerédi)
        tabueqcol::SolutionManager currentS(inst, -1); // Usa max_degree + 1
        currentS.construct_greedy_initial(args.seed);

        printf("Solução inicial construída.\n");

        // Como começamos com max_degree+1, a inicial já é viável ou muito fácil de resolver
        tabueqcol::SolutionManager bestFeasibleS = currentS; 
        int best_k_found = currentS.k;

        printf("Iniciando com K=%d. Tempo max: %.2fs\n", currentS.k, args.time_limit);

        while (!globalStop.is_time_up()) {
            
            // Tenta resolver para o K atual
            bool solved = currentS.run_tabu_search(tabuConfig, globalStop, args.seed);

            if (solved) {
                // SUCCESSO!
                printf("[%.2fs] K=%d resolvido! Iteracoes: %d\n", globalStop.get_elapsed(), currentS.k, tabuConfig.max_iter);
                
                // 1. Salva essa como a melhor
                bestFeasibleS = currentS; // Cópia completa do estado
                best_k_found = currentS.k;

                // 2. Tenta reduzir K
                if (best_k_found == 1) break; // Impossível reduzir mais

                int next_k = best_k_found - 1;
                
                // 3. Prepara próxima iteração: Cria novo gerenciador e usa "From Previous"
                tabueqcol::SolutionManager nextS(inst, next_k);
                nextS.construct_greedy_from_previous(bestFeasibleS, args.seed);
                
                // 4. Atualiza currentS para o próximo loop
                currentS = nextS; // nextS pode ter conflitos iniciais, o Tabu vai tentar zerar

            } else {
                // FALHA
                // Se saiu do run_tabu_search retornando false, pode ser por dois motivos:
                // A) Acabou o tempo global.
                // B) Atingiu max_iter local sem convergir (mínimo local difícil).
                
                if (globalStop.is_time_up()) {
                    printf("[TIMEOUT] Tempo esgotado tentando resolver K=%d.\n", currentS.k);
                } else {
                    printf("[LIMIT] Nao convergiu para K=%d apos %d iteracoes.\n", currentS.k, tabuConfig.max_iter);
                }
                
                // Em ambos os casos, não conseguimos provar viabilidade para este K.
                // Paramos a busca e reportamos o último K que funcionou.
                break;
            }
        }

        // RESULTADO FINAL
        printf("\n=== RESULTADO FINAL ===\n");
        printf("Melhor K Equitativo (Chi_eq): %d\n", best_k_found);
        // Opcional: Validar bestFeasibleS novamente para ter certeza absoluta

    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        return 1;
    }


}


static void die(const string &msg) {
    cerr << msg << "\n";
    exit(1);
}



tabueqcol::Instance read_instance(const string &path){
    ifstream in(path);
    if(!in) die("Cannot open instance file: " + path);
    
    tabueqcol::Instance I;
    int kpairs;
    
    if(!(in >> I.n >> kpairs)) die("Bad instance header");
    
    I.edges.reserve(kpairs);
    
    for(int t=0; t<kpairs; ++t){
        int a, b; 
        in >> a >> b;
        // Assume input 1-based -> converte para 0-based aqui
        I.edges.emplace_back(a-1, b-1);
    }

    I.build_adj(); 

    return I;
}


// compute conflicts: number of edges whose endpoints share same color
int compute_conflicts(const tabueqcol::Instance &inst, const vector<int> &color){
    int conflicts = 0;
    for(auto &e: inst.edges){
        if(color[e.first] == color[e.second]) ++conflicts;
    }
    return conflicts;
}




// sizes per color
vector<int> color_sizes(const vector<int> &color, int K){
    vector<int> sz(K,0);
    for(int v=0; v<(int)color.size(); ++v){
        int c = color[v];
        if(c>=0 && c<K) ++sz[c];
    }
    return sz;
}



/*
Implementação do ECP via TabuEQCol baseado no TABUCOL original para GCP
Referência primária: A tabu search heuristic for the Equitable Coloring Problem, I. Méndez Díaz, G. Nasini, D. Severín
Link: https://arxiv.org/pdf/1405.7020v1

=============================
SOLUÇÃO
uma partição dos vértices {V1, V2, ..., VK} representada por um vetor color[v] = c (cor atribuída ao vértice v)
============================

============================
FUNÇÃO OBJETIVO
número de conflitos (arestas com pontas na mesma cor)
============================

I. M´endez D´ıaz3, G. Nasini1,2 and D. Sever´ın1,2
Search space and objective function. A solution s is a partition (V1,V2,...,Vk)
of the set of vertices. Let E(Vi) be the set of edges of G with both endpoints in Vi.
The objective function to be minimized is f(s) = ∑i=1..k |E(Vi)|, i.e., the number of conflicting edges in s.

Referência Primária: TABUCOL original

============================

// A ideia então é encontrar soluções com f(s) = 0 (nenhum conflito) e que sejam equilibradas (tamanhos dos conjuntos Vi diferem no máximo em 1).
// Uma vez encontrada uma solução factível para um dado K número de cores, podemos tentar diminuir K e repetir o processo até não ser mais possível.

*/




/*

==========================
SOLUÇÃO INICIAL

  
TABUCOL

I. M´endez D´ıaz3, G. Nasini1,2 and D. Sever´ın1,2
 Start with empty sets V1,V2,...,Vk and, at each step, choose
 a non-considered vertex v randomly and put it into Vi with the smallest
 possible i such that E(Vi) is not incremented. If it is not possible, choose a
 random number j ∈ {1,...,k} and put v into Vj.

 Referência primária:  17. Bl¨ochliger I., Zufferey, N.: A graph coloring heuristic using partial solutions and a reactive tabu scheme. Comput. Oper. Res. 35(3), 960-975 (2008)

 Considerações: não gera solução inicial sempre válida, mas chega próximo. Poderia fazer um algoritmo guloso que ao invés de atribuir j aleatoriamente se não houver cor viável, 
 simplesmente atribui j = (numero de cores usadas) + 1, e atualiza o número de cores usadas





TUBUEQCOL
I. M´endez D´ıaz3, G. Nasini1,2 and D. Sever´ın1,2

Denote W+(s) = {i : |Vi| = ⌊n/k⌋ + 1} and W−(s) = {i :
 |Vi| = ⌊n/k⌋}, where Vi are the color classes of s. Since s satisfies the equity
 constraint, we have that W+(s) and W−(s) determine a partition of {1,...,k}
 and, in particular, |W+(s)| = r where r = n − k⌊n/k⌋. From now on, we just
 write W+ and W−. These sets will be useful in the development of the algorithm.

1) Start with empty sets V1,V2,...,Vk and an integer ˜r ← 0 (this value
 will have the cardinal of W+). At each step, define set I = {i : |Vi| ≤ M − 1},
 where M is the maximum allowable size of a class:
    a) if ~r < r, M = ⌊n/k⌋ + 1;
    b) if ~r = r, M = ⌊n/k⌋.

 (once we already have r class of size ⌊n/k⌋+1, the size of the remaining classes
must not exceed ⌊n/k⌋). Then, choose a non-considered vertex v randomly and
 put it into a class Vi such that i ∈ I is the smallest possible and E(Vi) is not
 incremented. If it is not possible, i is chosen ramdonly from I. To keep ˜r up
 to date, each time a vertex is added to a set Vi such that |Vi| = ⌊n/k⌋, ˜r is
    incremented by one.


Esse método funciona bem para gerar soluções iniciais equilibradas do completo zero, mas uma vez que temos uma solução para K cores e 
queremos gerar uma solução inicial para K-1 cores, podemos nos aproveitar da solução anterior para gerar uma solução inicial melhor.

2) Let p : {1,...,k+1} → {1,...,k+1} be a bijective function (i.e. a
 random permutation) and let V1,V2,...,Vk,Vk+1 be the color classes of the
 known (k +1)-eqcol. Set Vi = Vp(i) for all i ∈ {1,...,k}, and ˜r = |W+|. Then,
 run Procedure 1 to assign a color to the remaining vertices which are those belonging to Vp(k+1).



============================






==========================
VIZINHANÇA

Se temos uma solução s com n vértices e k cores, se k não divide n de forma inteira, então existem partições Vi com tamanhos diferentes (algumas com ⌊n/k⌋ e outras com ⌊n/k⌋+1), ou seja, W+ não é vazio.
Isso significa que podemos mover pelo menos um vértice de uma partição Vi em W+ para uma partição Vj em W- sem violar a restrição de equidade.
Entretanto, quando r é muito pequeno ou muito grande (próximo de k), o número de partições em W+ ou W- é pequeno, e isso limita o número de movimentos possíveis.
Para contornar esse problema, o algoritmo TabuEQCol também permite o movimento de "exchange"

Temos então 2 tipos de movimentos:

Definição: seja C(s) o conjunto de vértices conflitantes em uma solução: C(s) = { v pertence a V: v é incidente em alguma aresta de E(V1) U E(V2) U ... U E(Vk) }
1) Move (aplicável quando k não divide n): escolher um vértice conflitante v em C(s) pertencente a uma partição Vi em W+ e movê-lo para uma partição Vj em W- (j diferente de i).
2) Exchange (aplicável sempre): escolher um vértice conflitante v em C(s). Seja i a cor de v em s. Escolher um vértice u com cor j tal que ou u não pertence a C(s) ou j < i (previne avaliar movimentos simétricos).
Então apenas troque os vértices v e u de Vi e Vj (Vi = Vi \ {v} U {u} e Vj = Vj \ {u} U {v}).
A nova função objetivo é então avaliada, e pode ser avaliada em tempo linear:
f(s') = f(s) - (uw pertence a E | w pertence a Vj (porque u já pertencia a Vj)) - (vw pertence a E | w pertence a Vi (porque v já pertencia a Vi)) + (vw pertence a E | w pertence a Vi \ {v} (o novo conjunto sem v em Vi)) + (uw pertence a E | w pertence a Vj \ {u} (o novo conjunto sem u em Vj))
 



==========================
MOVIMENTOS TABU
Uma vez feito um movimentos que moveu v de Vi, adicionamos a lista Tabu a entrada (v, i) proibindo que v volte para Vi por um certo número de iterações.

==========================
TABU TENURE

TABUCOL original usa Tabu Tenure fixo de 7 iterações. 
Referência primária: 17. Bl¨ochliger I., Zufferey, N.: A graph coloring heuristic using partial solutions and a reactive tabu scheme. Comput. Oper. Res. 35(3), 960-975 (2008)

TABUEQCOL propoem tabu tenure dinâmico:
alpha * |C(s)| + random(beta)

Rerefrência: I. M´endez D´ıaz3, G. Nasini1,2 and D. Sever´ın1,2


OU:
Referência: I. Blöchliger, N. Zufferey / Computers & Operations Research 35 (2008) 960–975, Agraph coloring heuristic using partial solutions and a reactive tabu scheme
const t = alpha * |C(s)|
random([t, t + 10])

*/


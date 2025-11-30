using HiGHS
using JuMP
using Printf

println("OK: HiGHS funcionando!")

# =================================
# ECP - Equitable Coloring Problem
# =================================
function build_ecp_model(n, I; timelimit=10.0, logfile="saida_higHS.log")
    # V = conjunto de vértices (v em V), |V| = n
    V = collect(1:n)

    # K = conjunto de cores (k em K), |K| = n
    K = collect(1:n)

    # I = relação de incompatibilidade (i, j) pertence a I, onde i, j em V e i != j
    
    m = Model(HiGHS.Optimizer)

    set_optimizer_attribute(m, "time_limit", timelimit)   # meia hora
    set_optimizer_attribute(m, "log_file", logfile)

    # VARIÁVEIS
    # x[v in V, k in K] = 1 se vértice v é atribuído a cor k, 0 caso contrário
    # Ck = cor k existe, tem ao menos um vértice atribuído a ela
    @variable(m, x[v in V, k in K], Bin)
    @variable(m, Ck[k in K], Bin)

    # FUNÇÃO OBJETIVO: minimizar o número de cores usadas
    # min sum(Ck for k in K)
    @objective(m, Min, sum(Ck[k] for k in K))

    # RESTRIÇÕES:

    # 1) Cada vértice deve ser atribuído a exatamente uma cor
    # sum(x[v in V, k in K]) = 1 para todo v em V
    @constraint(m, [v in V], sum(x[v, k] for k in K) == 1)

    # 2) Vértices incompatíveis não podem ser atribuídos à mesma cor
    # x[i, k] + x[j, k] <= 1 para todo (i, j) em I e para todo k em K
    @constraint(m, [ (i,j) in I, k in K], x[i, k] + x[j, k] <= 1)

    # 3) Seja qk = sum(x[v in V, k]) o número de vértices atribuídos à cor k, então para cada par de cores k, l em K:
    # qk - ql <= 1 + M (2 - Ck - Cl), onde M é uma constante grande o suficiente, no caso maior que |V| = n
    M = n + 1
    @constraint(m, [k in K, l in K], sum(x[v, k] for v in V) - sum(x[v, l] for v in V) <= 1 + M * (2 - Ck[k] - Ck[l]))

    # 4) Ck = 1 se ao menos um vértice é atribuído à cor k, 0 caso contrário
    # Ck >= x[v, k] para todo v em V
    @constraint(m, [k in K, v in V], Ck[k] >= x[v, k])

    # As restrições acima já resolvem o problema. A restrição 4 será satisfeita corretamente pela função objetivo que minimiza o número de cores usadas
    # Mas agora podemos adicionar restrições que cortam o espaço de soluções para melhorar o desempenho do solver

    # 5) Ordenação simétrica das cores. Isso retira simples permutações de soluções do espaço das soluções
    # Ck >= Ck+1 para todo k em K, k < |K|
    @constraint(m, [k in K[1:end-1]], Ck[k] >= Ck[k+1])


    # =================================
    # RESOLUÇÃO DO MODELO
    optimize!(m)

    return m, V, K, x

end



function ler_instancia(caminho_arquivo::String)
    linhas = readlines(caminho_arquivo)

    # Ler n e k
    n, k = parse.(Int, split(linhas[1]))

    # Ler pares de incompatibilidade
    I = Vector{Tuple{Int,Int}}()
    for idx in 2:(k+1)
        i, j = parse.(Int, split(linhas[idx]))
        push!(I, (i, j))
    end

    return n, I
end



# LOOP PRINCIPAL


# Pega o nome de todos os arquivos no diretorio 
for nome_arquivo in readdir("instancias_ecp")

    # Pega o nome do arquivo passado como argumento na linha de comando
    #nome_arquivo = ARGS[1]
    println("Lendo instância do arquivo: ", nome_arquivo)
    n, I = ler_instancia("instancias_ecp/" * nome_arquivo)
    println("Número de vértices: ", n)
    println("Número de pares de incompatibilidade: ", length(I))

    m, V, K, x = build_ecp_model(n, I; timelimit=1800.0, logfile="saida_gurobi_$(nome_arquivo).log")

    println("Instância ", nome_arquivo, " resolvida.")

    # Arquivo de saída por instância
    saida = "resultados/$(replace(nome_arquivo, ".txt" => ".out"))"

    open(saida, "w") do io
        println(io, "===== RESULTADOS DA INSTÂNCIA: $nome_arquivo =====")
        println(io, "Status: ", termination_status(m))

        if has_values(m)
            println(io, "Melhor objetivo encontrado: ", objective_value(m))
            println(io, "Lower bound: ", objective_bound(m))

            println(io, "\nAtribuições:")
            for v in V
                k = findfirst(k -> value(x[v,k]) > 0.5, K)
                println(io, "  Vértice $v -> Cor $k")
            end
        else
            println(io, "Modelo não produziu solução viável.")
        end
    end
end





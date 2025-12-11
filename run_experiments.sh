# Script para executar varredura de parametros e coletar resultados
# Uso: ./run_experiments.sh

echo "=========================================="
echo "EXPERIMENTOS DE CACHE THRASHING"
echo "=========================================="

# Compila o programa
echo "Compilando..."
gcc -Wall -Wextra -O2 -std=c11 -o thrashing thrashing.c -lm
if [ $? -ne 0 ]; then
    echo "Erro na compilacao!"
    exit 1
fi

# Cria diretorio para resultados
mkdir -p resultados
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTPUT_CSV="resultados/thrashing_${TIMESTAMP}.csv"

# Cabeçalho do CSV
echo "teste,num_arrays,stride_bytes,iterations,num_runs,tempo_medio_ms,desvio_padrao_ms,tempo_por_acesso_ns" > "$OUTPUT_CSV"

echo ""
echo "Executando experimentos..."
echo "Resultados serao salvos em: $OUTPUT_CSV"
echo ""

# Arrays para armazenar resultados do resumo
declare -a NUM_ARRAYS_VALUES=()
declare -a TEMPO_VALUES=()
declare -a TEMPO_ACESSO_VALUES=()

# Experimento 1: Varredura de NUM_ARRAYS (2 a 20)
echo "=== Experimento 1: Varredura NUM_ARRAYS (2 a 20) ==="
for num_arrays in 2 4 6 8 10 12 14 16 18 20; do
    echo "Testando num_arrays=$num_arrays..."
    
    # Executa e captura output completo
    OUTPUT=$(./thrashing 1000000 $num_arrays 32768 5 2>&1)
    
    # Extrai valores corretamente (uma linha por vez)
    TEMPO=$(echo "$OUTPUT" | grep "Tempo (media" | head -1 | sed 's/.*: \([0-9.]*\) ms.*/\1/')
    DP=$(echo "$OUTPUT" | grep "Tempo (media" | head -1 | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
    TEMPO_ACESSO=$(echo "$OUTPUT" | grep "Tempo medio por acesso" | head -1 | sed 's/.*: \([0-9.]*\) ns.*/\1/')
    
    # Armazena para resumo
    NUM_ARRAYS_VALUES+=($num_arrays)
    TEMPO_VALUES+=($TEMPO)
    TEMPO_ACESSO_VALUES+=($TEMPO_ACESSO)
    
    echo "thrashing,$num_arrays,32768,1000000,5,$TEMPO,$DP,$TEMPO_ACESSO" >> "$OUTPUT_CSV"
done

# Arrays para comparacao de solucoes
declare -a SOLUCOES_NAMES=()
declare -a SOLUCOES_TEMPO=()
declare -a SOLUCOES_TEMPO_ACESSO=()

# Experimento 2: Comparacao de solucoes (com num_arrays=16 para thrashing real)
echo ""
echo "=== Experimento 2: Comparacao de Solucoes (num_arrays=16) ==="
echo "Executando todos os testes (thrashing, padding, fusion, blocking)..."
# Executa uma vez e extrai todos os resultados
OUTPUT=$(./thrashing 1000000 16 32768 5 2>&1)
echo "Extraindo resultados..."

# Extrai resultados de cada teste usando os titulos dos testes
# TESTE 1 THRASHING
TEMPO_THRASH=$(echo "$OUTPUT" | grep -A 20 "TESTE 1 THRASHING" | grep "Tempo (media" | head -1 | sed 's/.*: \([0-9.]*\) ms.*/\1/')
DP_THRASH=$(echo "$OUTPUT" | grep -A 20 "TESTE 1 THRASHING" | grep "Tempo (media" | head -1 | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
TEMPO_ACESSO_THRASH=$(echo "$OUTPUT" | grep -A 20 "TESTE 1 THRASHING" | grep "Tempo medio por acesso" | head -1 | sed 's/.*: \([0-9.]*\) ns.*/\1/')

# SOLUCAO 1 PADDING
TEMPO_PADDING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 1 PADDING" | grep "Tempo (media" | head -1 | sed 's/.*: \([0-9.]*\) ms.*/\1/')
DP_PADDING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 1 PADDING" | grep "Tempo (media" | head -1 | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
TEMPO_ACESSO_PADDING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 1 PADDING" | grep "Tempo medio por acesso" | head -1 | sed 's/.*: \([0-9.]*\) ns.*/\1/')

# SOLUCAO 2 ARRAY FUSION
TEMPO_FUSION=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 2 ARRAY FUSION" | grep "Tempo (media" | head -1 | sed 's/.*: \([0-9.]*\) ms.*/\1/')
DP_FUSION=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 2 ARRAY FUSION" | grep "Tempo (media" | head -1 | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
TEMPO_ACESSO_FUSION=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 2 ARRAY FUSION" | grep "Tempo medio por acesso" | head -1 | sed 's/.*: \([0-9.]*\) ns.*/\1/')

# SOLUCAO 3 LOOP BLOCKING
TEMPO_BLOCKING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 3 LOOP BLOCKING" | grep "Tempo (media" | head -1 | sed 's/.*: \([0-9.]*\) ms.*/\1/')
DP_BLOCKING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 3 LOOP BLOCKING" | grep "Tempo (media" | head -1 | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
TEMPO_ACESSO_BLOCKING=$(echo "$OUTPUT" | grep -A 20 "SOLUCAO 3 LOOP BLOCKING" | grep "Tempo medio por acesso" | head -1 | sed 's/.*: \([0-9.]*\) ns.*/\1/')

# Armazena para resumo
SOLUCOES_NAMES=("thrashing" "padding" "fusion" "blocking")
SOLUCOES_TEMPO=("$TEMPO_THRASH" "$TEMPO_PADDING" "$TEMPO_FUSION" "$TEMPO_BLOCKING")
SOLUCOES_TEMPO_ACESSO=("$TEMPO_ACESSO_THRASH" "$TEMPO_ACESSO_PADDING" "$TEMPO_ACESSO_FUSION" "$TEMPO_ACESSO_BLOCKING")

# Salva no CSV
echo "thrashing,16,32768,1000000,5,$TEMPO_THRASH,$DP_THRASH,$TEMPO_ACESSO_THRASH" >> "$OUTPUT_CSV"
echo "padding,16,32768,1000000,5,$TEMPO_PADDING,$DP_PADDING,$TEMPO_ACESSO_PADDING" >> "$OUTPUT_CSV"
echo "fusion,16,32768,1000000,5,$TEMPO_FUSION,$DP_FUSION,$TEMPO_ACESSO_FUSION" >> "$OUTPUT_CSV"
echo "blocking,16,32768,1000000,5,$TEMPO_BLOCKING,$DP_BLOCKING,$TEMPO_ACESSO_BLOCKING" >> "$OUTPUT_CSV"

# Experimento 3: Varredura de STRIDE (para verificar impacto)
echo ""
echo "=== Experimento 3: Varredura STRIDE (16KB a 64KB) ==="
for stride in 16384 32768 49152 65536; do
    echo "Testando stride=$stride bytes..."
    OUTPUT=$(./thrashing 1000000 16 $stride 5 2>&1)
    TEMPO=$(echo "$OUTPUT" | grep "Tempo (media" | sed 's/.*: \([0-9.]*\) ms.*/\1/')
    DP=$(echo "$OUTPUT" | grep "Tempo (media" | sed 's/.*DP: \([0-9.]*\) ms.*/\1/')
    TEMPO_ACESSO=$(echo "$OUTPUT" | grep "Tempo medio por acesso" | sed 's/.*: \([0-9.]*\) ns.*/\1/')
    
    echo "thrashing,16,$stride,1000000,5,$TEMPO,$DP,$TEMPO_ACESSO" >> "$OUTPUT_CSV"
done

echo ""
echo "=========================================="
echo "EXPERIMENTOS CONCLUIDOS"
echo "=========================================="
echo "Resultados salvos em: $OUTPUT_CSV"
echo ""

# ==========================================
# RESUMO FORMATADO - RESULTADOS MAIS IMPORTANTES
# ==========================================
echo "=========================================="
echo "RESUMO DOS RESULTADOS"
echo "=========================================="
echo ""

# Tabela 1: Thrashing vs Num Arrays
echo "1. THRASHING: Tempo vs Numero de Arrays"
echo "   (Esperado: aumento quando num_arrays > 8)"
echo "   ---------------------------------------------------------"
printf "   %-10s | %-15s | %-20s\n" "Arrays" "Tempo (ms)" "Tempo/Acesso (ns)"
echo "   ---------------------------------------------------------"
for i in "${!NUM_ARRAYS_VALUES[@]}"; do
    num=${NUM_ARRAYS_VALUES[$i]}
    tempo=${TEMPO_VALUES[$i]}
    tempo_acesso=${TEMPO_ACESSO_VALUES[$i]}
    if [ $num -gt 8 ]; then
        printf "   %-10s | %-15s | %-20s *\n" "$num" "$tempo" "$tempo_acesso"
    else
        printf "   %-10s | %-15s | %-20s\n" "$num" "$tempo" "$tempo_acesso"
    fi
done
echo "   ---------------------------------------------------------"
echo "   * = Excede associatividade 8-way (thrashing esperado)"
echo ""

# Tabela 2: Comparacao de Solucoes
echo "2. COMPARACAO DE SOLUCOES (num_arrays=16)"
echo "   ---------------------------------------------------------"
printf "   %-12s | %-15s | %-20s\n" "Solucao" "Tempo (ms)" "Tempo/Acesso (ns)"
echo "   ---------------------------------------------------------"
for i in "${!SOLUCOES_NAMES[@]}"; do
    nome=${SOLUCOES_NAMES[$i]}
    tempo=${SOLUCOES_TEMPO[$i]}
    tempo_acesso=${SOLUCOES_TEMPO_ACESSO[$i]}
    printf "   %-12s | %-15s | %-20s\n" "$nome" "$tempo" "$tempo_acesso"
done
echo "   ---------------------------------------------------------"
echo ""

# Encontra melhor e pior (usando awk para comparacao de floats)
if [ ${#SOLUCOES_NAMES[@]} -gt 0 ]; then
    MIN_TEMPO=${SOLUCOES_TEMPO[0]}
    MIN_IDX=0
    MAX_TEMPO=${SOLUCOES_TEMPO[0]}
    MAX_IDX=0
    
    for i in "${!SOLUCOES_TEMPO[@]}"; do
        tempo=${SOLUCOES_TEMPO[$i]}
        # Compara floats usando awk
        if [ $(echo "$tempo $MIN_TEMPO" | awk '{print ($1 < $2)}') -eq 1 ]; then
            MIN_TEMPO=$tempo
            MIN_IDX=$i
        fi
        if [ $(echo "$tempo $MAX_TEMPO" | awk '{print ($1 > $2)}') -eq 1 ]; then
            MAX_TEMPO=$tempo
            MAX_IDX=$i
        fi
    done
    
    echo "3. DESTAQUES:"
    echo "   Melhor solucao: ${SOLUCOES_NAMES[$MIN_IDX]} (${SOLUCOES_TEMPO[$MIN_IDX]} ms)"
    echo "   Pior solucao:   ${SOLUCOES_NAMES[$MAX_IDX]} (${SOLUCOES_TEMPO[$MAX_IDX]} ms)"
    if [ $MIN_IDX -ne $MAX_IDX ]; then
        MELHORIA=$(echo "$MAX_TEMPO $MIN_TEMPO" | awk '{printf "%.1f", ($1 - $2) * 100 / $1}')
        echo "   Melhoria:       ${MELHORIA}% mais rapido"
    fi
    echo ""
fi

echo "=========================================="
echo "Para ver todos os dados detalhados:"
echo "  cat $OUTPUT_CSV | column -t -s,"
echo "=========================================="


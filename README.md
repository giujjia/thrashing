# Simulação de Thrashing em Cache

Este projeto implementa uma simulação em C que demonstra o problema de **thrashing** em cache de memória, utilizando múltiplos arrays com medição real de tempo e três soluções práticas.

## Nota

A tarefa original solicitava a simulação de thrashing com **2 arrays distantes 32 KB**. Porém, em processadores modernos com caches associativos (tipicamente 8-way), apenas 2 arrays não são suficientes para causar thrashing significativo, pois ambos podem caber simultaneamente nas 8 vias disponíveis do mesmo conjunto de cache.

Para demonstrar thrashing real em hardware moderno, utilizamos **múltiplos arrays** (padrão: 16) que excedem a associatividade do cache. Isso força substituições constantes e permite observar o impacto real do problema. O código suporta qualquer número de arrays via argumentos de linha de comando, mantendo a flexibilidade para testar diferentes configurações.

## Descrição

O **thrashing** ocorre quando múltiplos arrays mapeiam para os mesmos sets de cache, causando constantes misses e degradação significativa de performance. Em caches associativos modernos (8-way), é necessário usar mais de 8 arrays para forçar thrashing real.

## Como Compilar

```bash
gcc -o thrashing thrashing.c -lm -O2
```

## Como Executar

### Execução Básica

```bash
./thrashing
```

### Com Parâmetros

```bash
./thrashing [iterations] [num_arrays] [stride_bytes] [num_runs]
```

**Exemplo** (para thrashing real em cache 8-way):
```bash
./thrashing 1000000 16 32768 5
```

**Parâmetros**:
- `iterations`: Número de iterações (padrão: 1000000)
- `num_arrays`: Número de arrays (padrão: 2, use 16+ para thrashing real)
- `stride_bytes`: Distância entre arrays em bytes (padrão: 32768 = 32 KB)
- `num_runs`: Número de execuções para calcular média/DP (padrão: 5)

### Executar Experimentos Automatizados

```bash
./run_experiments.sh
```

O script executa múltiplos testes e gera um resumo dos resultados.

## O que o Programa Faz

O programa executa os seguintes testes:

### 1. Teste de Thrashing
Demonstra o problema de thrashing com múltiplos arrays (padrão: 16) que mapeiam para os mesmos sets de cache, causando substituições constantes quando o número de arrays excede a associatividade (8-way).

### 2. Solução 1: Padding
Adiciona espaçamento (padding) entre os arrays para garantir que eles mapeiem para sets diferentes de cache, reduzindo conflitos.

### 3. Solução 2: Array Fusion
Combina os arrays em uma estrutura única, melhorando a localidade espacial e reduzindo o número de acessos à memória.

### 4. Solução 3: Loop Blocking/Tiling
Processa os arrays em blocos menores que cabem no cache, melhorando a localidade temporal e reduzindo thrashing.

## Medição de Tempo

O programa utiliza `clock_gettime()` (Linux) ou `QueryPerformanceCounter` (Windows) para medir o tempo real de execução em nanosegundos, fornecendo:
- Tempo total de execução (média e desvio padrão)
- Tempo médio por acesso
- Comparação entre diferentes abordagens

## Resultados Esperados

Em sistemas com cache L1 de 32 KB e 8-way associativo:
- **Thrashing** (16 arrays): ~10-15 ns por acesso (degradação significativa)
- **Padding/Fusion**: ~2-4 ns por acesso (melhoria de 4x ou mais)
- **Loop Blocking**: ~7-12 ns por acesso (melhoria moderada)

A diferença de performance varia dependendo da arquitetura do processador, mas geralmente as soluções mostram melhoria de 50-80% ou mais.

## Requisitos

- Compilador C (GCC ou Clang)
- Sistema operacional Linux ou Windows
- Biblioteca matemática (`-lm` para compilação)

## Notas Técnicas

- **Cache assumido**: 32 KB L1, 8-way associativo, 64 bytes por linha
- **Tamanho dos arrays**: 1024 elementos (quando num_arrays > 2) ou 8192 elementos (padrão)
- **Número de iterações**: 1.000.000 (padrão)
- **Distância crítica**: 32 KB garante que arrays mapeiam para o mesmo set index
- **Múltiplas execuções**: 5 execuções para calcular média e desvio padrão

## Compatibilidade

O código é compatível com Windows e Linux:
- **Windows**: Usa `QueryPerformanceCounter` e `_aligned_malloc`
- **Linux**: Usa `clock_gettime` e `aligned_alloc`
- **Linux**: Suporta fixar processo numa CPU específica (reduz ruído)


Desenvolvido para demonstração de thrashing em cache de memória.

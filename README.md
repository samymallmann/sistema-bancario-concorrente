# 🏦 Sistema Bancário Concorrente

[![C](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://gcc.gnu.org/)
[![Threads](https://img.shields.io/badge/Parallelism-POSIX_Threads-007ACC?style=for-the-badge)](https://man7.org/linux/man-pages/man7/pthreads.7.html)
[![Academic](https://img.shields.io/badge/UFAM-ICOMP-red?style=for-the-badge)](https://icomp.ufam.edu.br/)

Projeto desenvolvido como Trabalho Prático da disciplina de Sistemas Operacionais da Universidade Federal do Amazonas (UFAM), com foco em programação concorrente utilizando threads, mutexes e semáforos.

O sistema simula operações bancárias concorrentes para demonstrar problemas clássicos de sincronização, como:

- Race Conditions
- Leitura Suja (*Dirty Read*)
- Exclusão Mútua
- Regiões Críticas
- Corrupção de Buffer Compartilhado

---

# 📋 Sumário

- [🎯 Objetivo](#-objetivo)
- [🏗️ Estruturas Principais](#️-estruturas-principais)
- [🧩 Explicação do Sistema](#-explicação-do-sistema)
  - [Leitores × Escritores](#leitores--escritores)
  - [Produtores × Consumidores](#produtores--consumidores)
- [🚀 Compilação e Execução](#-compilação-e-execução)
- [📌 Observações](#-observações)

---

# 🎯 Objetivo

O objetivo do projeto é demonstrar, na prática, como múltiplas threads acessando recursos compartilhados simultaneamente podem causar inconsistências quando não existe sincronização adequada.

O programa implementa dois problemas clássicos de Sistemas Operacionais:

- Leitores × Escritores
- Produtores × Consumidores

Cada problema possui versões:
- com sincronização parcial;
- com prioridade;
- e sem qualquer controle de concorrência.

Isso permite visualizar claramente os efeitos do acesso simultâneo aos dados.

---

# 🏗️ Estruturas Principais

O sistema utiliza algumas estruturas principais para representar os dados compartilhados.

## `ContaBancaria`

Representa a conta utilizada no problema de Leitores × Escritores.

```c
typedef struct {
    int numero;
    char titular[64];
    double saldo;
    int num_transacoes;
} ContaBancaria;
```

A estrutura armazena:
- número da conta;
- titular;
- saldo atual;
- quantidade de transações realizadas.

---

## `Transacao`

Representa uma operação bancária no sistema de Produtores × Consumidores.

```c
typedef struct {
    int id;
    int conta_destino;
    double valor;
    char tipo[16];
} Transacao;
```

As transações podem ser:
- depósito;
- saque;
- transferência.

---

## `BufferCircular`

Fila circular compartilhada entre produtores e consumidores.

```c
typedef struct {
    Transacao dados[BUFFER_MAX];
    int in, out, count;
} BufferCircular;
```

O buffer possui:
- posição de entrada;
- posição de saída;
- quantidade atual de itens.

---

# 🧩 Explicação do Sistema

# Leitores × Escritores

Neste cenário, múltiplas threads acessam simultaneamente uma mesma conta bancária.

## Threads leitoras

As threads leitoras:
- consultam o saldo;
- exibem os valores lidos;
- entram e saem da região crítica.

Exemplo:

```txt
Leitor 3 ENTROU na RC
Leitor 3 leu saldo: R$ 1000.00
Leitor 3 SAIU da RC
```

---

## Threads escritoras

As escritoras:
- realizam depósitos e saques;
- modificam o saldo;
- podem bloquear leitores dependendo da versão.

Exemplo:

```txt
Escritor 1: SAQUE R$ 500.00
1000.00 → 500.00
```

---

## 🔹 Versão 1 — Sem prioridade

Nesta versão:
- leitores não bloqueiam escritores;
- escritores também não bloqueiam leitores.

Isso permite que leitores acessem a conta enquanto um escritor ainda está atualizando os dados.

O resultado pode ser uma:

# Leitura Suja (*Dirty Read*)

Exemplo:

```txt
Leitor 3 leu saldo: R$ 1000.00  *** LEITURA SUJA! ***
```

Nesse caso:
- o saldo já estava sendo alterado;
- mas o leitor ainda visualizou o valor antigo.

Para aumentar a chance desse erro ocorrer, o código utiliza atrasos artificiais (`sleep`) durante a escrita.

---

## 🔹 Versão 2 — Escritores com prioridade

Nesta implementação:
- escritores possuem prioridade sobre leitores;
- leitores aguardam enquanto houver escritores ativos;
- não ocorre leitura suja.

O sistema utiliza:
- semáforos;
- mutexes;
- controle de fila;
- exclusão mútua.

Semáforos utilizados:

```c
static sem_t v2_mutex_rc, v2_mutex_wc, v2_db, v2_fila;
```

Essa abordagem impede acessos inconsistentes à conta bancária.

---

## 🔹 Versão 3 — Sem controle de concorrência

Nesta versão:
- não existe sincronização;
- múltiplos escritores acessam o saldo simultaneamente.

Isso provoca:

# Race Condition (Condição de Corrida)

Exemplo real:

```txt
Saldo esperado: R$ 2000.00
Saldo final:    R$ 1100.00
```

O erro acontece porque:
- várias threads leem o mesmo saldo antigo;
- calculam novos valores simultaneamente;
- e sobrescrevem as atualizações umas das outras.

---

# Produtores × Consumidores

Neste cenário, o sistema utiliza um buffer compartilhado para armazenar transações bancárias.

---

## Threads produtoras

As produtoras:
- geram transações aleatórias;
- inserem dados no buffer.

Exemplo:

```txt
Produtor 1 gerou TX#5
Produtor 1 inseriu TX#5
```

---

## Threads consumidoras

As consumidoras:
- retiram transações do buffer;
- processam as operações em ordem.

Exemplo:

```txt
Consumidor 1 processou TX#5
```

---

## Controle do Buffer

O sistema utiliza os semáforos clássicos do problema Produtor × Consumidor:

```c
static sem_t pc_empty, pc_full, pc_mutex;
```

### `pc_empty`
Controla quantas posições vazias existem no buffer.

### `pc_full`
Controla quantos itens estão disponíveis para consumo.

### `pc_mutex`
Garante exclusão mútua durante alterações no buffer.

---

## 🔹 Buffer cheio

Quando não existem posições livres:

```txt
Produtor BLOQUEADO (buffer cheio)
```

O produtor entra em espera até que um consumidor remova algum item.

---

## 🔹 Buffer vazio

Quando não existem itens disponíveis:

```txt
Consumidor DORMINDO (buffer vazio)
```

O consumidor fica bloqueado aguardando novas transações.

---

## 🔹 Versão sem controle

Na versão sem sincronização:
- produtores escrevem simultaneamente;
- consumidores acessam posições inválidas;
- o buffer pode ser corrompido.

Exemplo:

```txt
*** possível colisão! ***
```

---

# 🚀 Compilação e Execução

## Requisitos

O projeto utiliza bibliotecas POSIX (`pthread` e `semaphore`), sendo recomendado executar em:

- Linux
ou
- WSL (Windows Subsystem for Linux)

---

## Compilação

Abra o terminal na pasta do projeto e execute:

```bash
gcc -o banco banco_concorrente.c -lpthread -lm
```

---

## Execução

Após compilar:

```bash
./banco
```

---

# 📌 Observações

- O sistema utiliza atrasos artificiais (`sleep` e `nanosleep`) para aumentar a concorrência entre as threads.
- Os resultados variam a cada execução devido ao escalonamento do sistema operacional.
- Algumas versões foram propositalmente implementadas sem sincronização para demonstrar erros clássicos de concorrência.

---

# 👨‍💻 Autor

Projeto desenvolvido para a disciplina de Sistemas Operacionais — UFAM.

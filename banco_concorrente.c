/*
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  Trabalho Prático 1 – Sistemas Operacionais – UFAM                      ║
 * ║  Tema: Sistema Bancário Concorrente                                      ║
 * ║                                                                          ║
 * ║  Compilar:  gcc -o banco banco_concorrente.c -lpthread -lm              ║
 * ║  Executar:  ./banco                                                      ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * Questão 1 – Leitores × Escritores
 *   V1: sem prioridade        (leitura suja possível)
 *   V2: escritores prioritários (sem leitura suja)
 *   V3: sem controle          (demonstra race condition)
 *
 * Questão 2 – Produtores × Consumidores (fila de transações bancárias)
 *   V1: vários produtores, 1 consumidor
 *   V2: vários produtores, vários consumidores
 *   V3: sem controle          (demonstra corrida no buffer)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CORES
 * ═══════════════════════════════════════════════════════════════════════════ */
#define RST   "\033[0m"
#define CYAN  "\033[0;36m"
#define YEL   "\033[0;33m"
#define GRN   "\033[0;32m"
#define MAG   "\033[0;35m"
#define RED   "\033[0;31m"
#define GRY   "\033[0;37m"
#define WHT   "\033[1;37m"

/* ═══════════════════════════════════════════════════════════════════════════
 * ESTRUTURAS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Conta bancária compartilhada (Leitores × Escritores) */
typedef struct {
    int    numero;
    char   titular[64];
    double saldo;
    int    num_transacoes;
} ContaBancaria;

/* Transação bancária (Produtores × Consumidores) */
typedef struct {
    int    id;
    int    conta_destino;
    double valor;
    char   tipo[16];
} Transacao;

/* Parâmetros das threads de leitores/escritores */
typedef struct {
    int    id;
    int    delay_ms;
    double valor;
    char   operacao[16];
} ParamLE;

/* Parâmetros das threads de produtores/consumidores */
typedef struct {
    int id;
    int delay_ms;
    int n_itens;
} ParamPC;

/* Buffer circular */
#define BUFFER_MAX 8
typedef struct {
    Transacao dados[BUFFER_MAX];
    int        in, out, count;
} BufferCircular;

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITÁRIOS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void timestamp(char *buf, size_t sz) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *t = localtime(&ts.tv_sec);
    snprintf(buf, sz, "%02d:%02d:%02d.%03ld",
             t->tm_hour, t->tm_min, t->tm_sec, ts.tv_nsec / 1000000);
}

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void exibir_conta(const ContaBancaria *c) {
    printf(WHT
           "  ┌──────────────────────────────────┐\n"
           "  │  Conta: %04d  |  %-16s           │\n"
           "  │  Saldo: R$ %10.2f                │\n"
           "  │  Transações: %-3d                │\n"
           "  └──────────────────────────────────┘\n"
           RST,
           c->numero, c->titular, c->saldo, c->num_transacoes);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * QUESTÃO 1 – LEITORES × ESCRITORES
 * ═══════════════════════════════════════════════════════════════════════════ */

static ContaBancaria conta;

/* ── Semáforos Versão 1 ──
 * Escritor NÃO usa semáforo → leitura suja é possível.
 * Leitores só se coordenam entre si via mutex + read_count. */
static sem_t v1_mutex;
static int   v1_rc;

/* ── Semáforos Versão 2 ── */
static sem_t v2_mutex_rc, v2_mutex_wc, v2_db, v2_fila;
static int   v2_rc, v2_wc;

static void init_conta(void) {
    conta.numero         = 1001;
    strncpy(conta.titular, "Carlos Silva", sizeof(conta.titular));
    conta.saldo          = 1000.00;
    conta.num_transacoes = 0;
}

/* ─────────────────────────────────────────────────────────────────────────
 * V1 – SEM PRIORIDADE
 * ───────────────────────────────────────────────────────────────────────── */
static void *leitor_v1(void *arg) {
    ParamLE *p = (ParamLE *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d CRIADO\n" RST, ts, p->id);
    msleep(p->delay_ms);

    /* Leitores se coordenam entre si, mas não bloqueiam o escritor */
    sem_wait(&v1_mutex);
    v1_rc++;
    sem_post(&v1_mutex);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d ENTROU na RC  (leitores ativos: %d)\n" RST,
           ts, p->id, v1_rc);

    /* Lê o saldo — pode estar no meio de uma atualização do escritor */
    double saldo_lido = conta.saldo;
    msleep(100 + rand() % 150);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d leu saldo: R$ %.2f%s\n" RST,
           ts, p->id, saldo_lido,
           (conta.saldo != saldo_lido) ? RED "  *** LEITURA SUJA! ***" RST : "");

    sem_wait(&v1_mutex);
    v1_rc--;
    sem_post(&v1_mutex);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d SAIU da RC  (leitores restantes: %d)\n" RST,
           ts, p->id, v1_rc);
    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Leitor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void *escritor_v1(void *arg) {
    ParamLE *p = (ParamLE *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d CRIADO  (%s  R$ %.2f)\n" RST,
           ts, p->id, p->operacao, p->valor);
    msleep(p->delay_ms);

    /* Sem semáforo — acessa direto, sem bloquear leitores */
    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d INICIANDO escrita (sem exclusão com leitores)\n" RST,
           ts, p->id);

    double ant = conta.saldo;
    /* Delay no meio da escrita: janela onde leitor pode ler valor inconsistente */
    conta.num_transacoes++;
    msleep(300 + rand() % 300);   /* <-- leitor pode entrar aqui antes do saldo ser atualizado */
    if (strcmp(p->operacao, "SAQUE") == 0) conta.saldo -= p->valor;
    else                                    conta.saldo += p->valor;

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d: %s R$ %.2f  |  %.2f → %.2f  (tx: %d)\n" RST,
           ts, p->id, p->operacao, p->valor, ant, conta.saldo, conta.num_transacoes);

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Escritor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void run_le_v1(int nl, ParamLE *escritores, int ne, int dl) {
    printf("\n" WHT "══════  V1 – SEM PRIORIDADE  (leitura suja possível)  ══════\n"
           "Escritores acessam a conta sem exclusão mútua com leitores.\n" RST);
    init_conta();
    sem_init(&v1_mutex, 0, 1);
    v1_rc = 0;

    int total = nl + ne;
    pthread_t *thr = malloc(total * sizeof(pthread_t));

    for (int i = 0; i < nl; i++) {
        ParamLE *p = malloc(sizeof(ParamLE));
        p->id = i+1; p->delay_ms = dl + rand() % 200;
        pthread_create(&thr[i], NULL, leitor_v1, p);
    }
    for (int i = 0; i < ne; i++) {
        ParamLE *p = malloc(sizeof(ParamLE));
        *p = escritores[i];
        pthread_create(&thr[nl+i], NULL, escritor_v1, p);
    }
    for (int i = 0; i < total; i++) pthread_join(thr[i], NULL);

    printf("\nEstado final:\n");
    exibir_conta(&conta);
    sem_destroy(&v1_mutex);
    free(thr);
}

/* ─────────────────────────────────────────────────────────────────────────
 * V2 – ESCRITORES COM PRIORIDADE
 * ───────────────────────────────────────────────────────────────────────── */
static void *leitor_v2(void *arg) {
    ParamLE *p = (ParamLE *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d CRIADO\n" RST, ts, p->id);
    msleep(p->delay_ms);

    timestamp(ts, sizeof(ts));
    printf(RED "[%s] Leitor %d aguarda fila...\n" RST, ts, p->id);
    sem_wait(&v2_fila);     /* bloqueado aqui se escritor estiver na fila */

    sem_wait(&v2_mutex_rc);
    v2_rc++;
    if (v2_rc == 1) sem_wait(&v2_db);
    sem_post(&v2_mutex_rc);
    sem_post(&v2_fila);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d ENTROU na RC  (leitores: %d)\n" RST,
           ts, p->id, v2_rc);
    msleep(200 + rand() % 200);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d leu saldo: R$ %.2f\n" RST,
           ts, p->id, conta.saldo);

    sem_wait(&v2_mutex_rc);
    v2_rc--;
    if (v2_rc == 0) sem_post(&v2_db);
    sem_post(&v2_mutex_rc);

    timestamp(ts, sizeof(ts));
    printf(CYAN "[%s] Leitor %d SAIU da RC\n" RST, ts, p->id);
    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Leitor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void *escritor_v2(void *arg) {
    ParamLE *p = (ParamLE *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d CRIADO  (%s  R$ %.2f)\n" RST,
           ts, p->id, p->operacao, p->valor);
    msleep(p->delay_ms);

    sem_wait(&v2_mutex_wc);
    v2_wc++;
    if (v2_wc == 1) sem_wait(&v2_fila);  /* 1º escritor trava a fila p/ leitores */
    sem_post(&v2_mutex_wc);

    timestamp(ts, sizeof(ts));
    printf(RED "[%s] Escritor %d aguarda acesso exclusivo  (escritores na fila: %d)...\n"
           RST, ts, p->id, v2_wc);
    sem_wait(&v2_db);

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d ENTROU na RC\n" RST, ts, p->id);
    msleep(400 + rand() % 300);

    double ant = conta.saldo;
    if (strcmp(p->operacao, "SAQUE") == 0) conta.saldo -= p->valor;
    else                                    conta.saldo += p->valor;
    conta.num_transacoes++;

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d: %s R$ %.2f  |  %.2f → %.2f  (tx: %d)\n" RST,
           ts, p->id, p->operacao, p->valor, ant, conta.saldo, conta.num_transacoes);

    sem_post(&v2_db);

    sem_wait(&v2_mutex_wc);
    v2_wc--;
    if (v2_wc == 0) sem_post(&v2_fila); /* último escritor libera leitores */
    sem_post(&v2_mutex_wc);

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d SAIU da RC\n" RST, ts, p->id);
    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Escritor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void run_le_v2(int nl, ParamLE *escritores, int ne, int dl) {
    printf("\n" WHT "══════  V2 – ESCRITORES COM PRIORIDADE  (sem leitura suja)  ══════\n"
           "Quando escritor aguarda, novos leitores ficam bloqueados na fila.\n" RST);
    init_conta();
    sem_init(&v2_mutex_rc, 0, 1);
    sem_init(&v2_mutex_wc, 0, 1);
    sem_init(&v2_db,       0, 1);
    sem_init(&v2_fila,     0, 1);
    v2_rc = v2_wc = 0;

    int total = nl + ne;
    pthread_t *thr = malloc(total * sizeof(pthread_t));

    for (int i = 0; i < nl; i++) {
        ParamLE *p = malloc(sizeof(ParamLE));
        p->id = i+1; p->delay_ms = dl + rand() % 300;
        pthread_create(&thr[i], NULL, leitor_v2, p);
    }
    for (int i = 0; i < ne; i++) {
        ParamLE *p = malloc(sizeof(ParamLE));
        *p = escritores[i];
        pthread_create(&thr[nl+i], NULL, escritor_v2, p);
    }
    for (int i = 0; i < total; i++) pthread_join(thr[i], NULL);

    printf("\nEstado final:\n");
    exibir_conta(&conta);
    sem_destroy(&v2_mutex_rc); sem_destroy(&v2_mutex_wc);
    sem_destroy(&v2_db);       sem_destroy(&v2_fila);
    free(thr);
}

/* ─────────────────────────────────────────────────────────────────────────
 * V3 – SEM CONTROLE (race condition)
 * ───────────────────────────────────────────────────────────────────────── */
static void *escritor_v3(void *arg) {
    ParamLE *p = (ParamLE *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(YEL "[%s] Escritor %d CRIADO (SEM CONTROLE) – %s R$ %.2f\n" RST,
           ts, p->id, p->operacao, p->valor);
    msleep(p->delay_ms);

    double lido = conta.saldo;       /* lê sem proteção          */
    msleep(50 + rand() % 100);       /* delay → aumenta colisão  */
    double novo = (strcmp(p->operacao,"SAQUE")==0) ? lido - p->valor
                                                    : lido + p->valor;
    conta.saldo = novo;              /* escreve sem proteção      */
    conta.num_transacoes++;

    timestamp(ts, sizeof(ts));
    printf(RED "[%s] *** RACE CONDITION! Escritor %d: lido=%.2f  escrito=%.2f"
           "  saldo_atual=%.2f ***\n" RST,
           ts, p->id, lido, novo, conta.saldo);

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Escritor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void run_le_v3(int ne, int de) {
    printf("\n" WHT "══════  V3 – SEM CONTROLE DE CONCORRÊNCIA  ══════\n"
           "Todos fazem DEPOSITO de R$100. Saldo esperado = 1000 + %d*100\n" RST, ne);
    init_conta();
    printf("Saldo inicial: R$ %.2f\n\n", conta.saldo);

    pthread_t *thr = malloc(ne * sizeof(pthread_t));
    for (int i = 0; i < ne; i++) {
        ParamLE *p = malloc(sizeof(ParamLE));
        p->id = i+1; p->delay_ms = de; p->valor = 100.0;
        strncpy(p->operacao, "DEPOSITO", sizeof(p->operacao));
        pthread_create(&thr[i], NULL, escritor_v3, p);
    }
    for (int i = 0; i < ne; i++) pthread_join(thr[i], NULL);

    double esperado = 1000.0 + ne * 100.0;
    printf("\nSaldo final   : R$ %.2f\n", conta.saldo);
    printf("Saldo esperado: R$ %.2f\n",   esperado);
    if (conta.saldo != esperado)
        printf(RED "⚠  INCONSISTÊNCIA! Diferença: R$ %.2f\n" RST, esperado - conta.saldo);
    else
        printf(GRY "(Sem colisão desta vez — tente de novo ou reduza o delay)\n" RST);

    exibir_conta(&conta);
    free(thr);
}

/* ─────────────────────────────────────────────────────────────────────────
 * MENU – Leitores × Escritores
 * ───────────────────────────────────────────────────────────────────────── */
static void menu_leitores_escritores(void) {
    int op;
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║        LEITORES × ESCRITORES             ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  1. Sem prioridade (leitura suja poss.)  ║\n");
    printf("║  2. Escritores com prioridade            ║\n");
    printf("║  3. Sem controle  (race condition)       ║\n");
    printf("║  0. Voltar                               ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Opção: "); scanf("%d", &op);
    if (op == 0) return;

    if (op == 3) {
        int ne, de;
        printf("Nº de escritores (recomendado >= 4): "); scanf("%d", &ne);
        printf("Delay (ms, recomendado 50): ");          scanf("%d", &de);
        run_le_v3(ne, de);
        return;
    }

    int nl, ne, dl, de_base;
    printf("\nSaldo inicial da conta: 1000.00 (fixo)\n");
    printf("Quantidade de leitores : "); scanf("%d", &nl);
    printf("Quantidade de escritores: "); scanf("%d", &ne);
    printf("Delay leitores  (ms, ex 100): "); scanf("%d", &dl);
    printf("Delay escritores (ms, ex 300): "); scanf("%d", &de_base);

    /* Coleta operações manualmente para cada escritor */
    ParamLE *escritores = malloc(ne * sizeof(ParamLE));
    printf("\nOperações dos escritores:\n");
    for (int i = 0; i < ne; i++) {
        char op_str[16];
        double val;
        printf("  Escritor %d - operacao (saque/deposito): ", i+1);
        scanf("%15s", op_str);
        printf("  Escritor %d - valor: ", i+1);
        scanf("%lf", &val);

        escritores[i].id = i+1;
        escritores[i].delay_ms = de_base + rand() % 300;
        escritores[i].valor = val;
        if (op_str[0] == 's' || op_str[0] == 'S')
            strncpy(escritores[i].operacao, "SAQUE",    sizeof(escritores[i].operacao));
        else
            strncpy(escritores[i].operacao, "DEPOSITO", sizeof(escritores[i].operacao));
    }

    if (op == 1) run_le_v1(nl, escritores, ne, dl);
    else         run_le_v2(nl, escritores, ne, dl);

    free(escritores);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * QUESTÃO 2 – PRODUTORES × CONSUMIDORES
 * ═══════════════════════════════════════════════════════════════════════════ */

static BufferCircular buf;
static int tx_id_global = 0;

static sem_t pc_empty, pc_full, pc_mutex;
static sem_t pc_mutex_pa;
static volatile int produtores_ativos;

static const char *tipos[] = { "DEPOSITO", "SAQUE", "TRANSF" };

static void exibir_buffer(void) {
    printf(GRY "  Buffer [%d/%d]: [", buf.count, BUFFER_MAX);
    for (int i = 0; i < BUFFER_MAX; i++) {
        int idx = (buf.out + i) % BUFFER_MAX;
        if (i < buf.count)
            printf(" %s:R$%.0f ", buf.dados[idx].tipo, buf.dados[idx].valor);
        else
            printf(" ___ ");
    }
    printf("]\n" RST);
}

static Transacao nova_transacao(void) {
    Transacao t;
    sem_wait(&pc_mutex_pa);
    t.id = ++tx_id_global;
    sem_post(&pc_mutex_pa);
    t.conta_destino = 1000 + rand() % 50;
    t.valor = 50.0 + rand() % 950;
    strncpy(t.tipo, tipos[rand() % 3], sizeof(t.tipo) - 1);
    t.tipo[sizeof(t.tipo)-1] = '\0';
    return t;
}

/* ─────────────────────────────────────────────────────────────────────────
 * V1 & V2 – COM CONTROLE
 * ───────────────────────────────────────────────────────────────────────── */
static void *produtor(void *arg) {
    ParamPC *p = (ParamPC *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(GRN "[%s] Produtor %d CRIADO  (%d itens)\n" RST, ts, p->id, p->n_itens);

    for (int i = 0; i < p->n_itens; i++) {
        msleep(p->delay_ms + rand() % 300);
        Transacao t = nova_transacao();

        timestamp(ts, sizeof(ts));
        printf(GRN "[%s] Produtor %d gerou TX#%d (%s R$%.2f)\n" RST,
               ts, p->id, t.id, t.tipo, t.valor);

        int val; sem_getvalue(&pc_empty, &val);
        if (val == 0) {
            timestamp(ts, sizeof(ts));
            printf(RED "[%s] Produtor %d BLOQUEADO (buffer cheio)\n" RST, ts, p->id);
        }
        sem_wait(&pc_empty);
        sem_wait(&pc_mutex);

        buf.dados[buf.in] = t;
        buf.in = (buf.in + 1) % BUFFER_MAX;
        buf.count++;

        timestamp(ts, sizeof(ts));
        printf(GRN "[%s] Produtor %d inseriu TX#%d\n" RST, ts, p->id, t.id);
        exibir_buffer();

        sem_post(&pc_mutex);
        sem_post(&pc_full);
    }

    sem_wait(&pc_mutex_pa);
    produtores_ativos--;
    int pa = produtores_ativos;
    sem_post(&pc_mutex_pa);

    if (pa == 0)
        for (int k = 0; k < 16; k++) sem_post(&pc_full); /* acorda consumidores */

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Produtor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void *consumidor(void *arg) {
    ParamPC *p = (ParamPC *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(MAG "[%s] Consumidor %d CRIADO\n" RST, ts, p->id);

    while (1) {
        int val; sem_getvalue(&pc_full, &val);
        if (val == 0) {
            sem_wait(&pc_mutex_pa);
            int pa = produtores_ativos;
            sem_post(&pc_mutex_pa);
            if (pa == 0 && buf.count == 0) break;

            timestamp(ts, sizeof(ts));
            printf(RED "[%s] Consumidor %d DORMINDO (buffer vazio)\n" RST, ts, p->id);
        }
        sem_wait(&pc_full);

        sem_wait(&pc_mutex_pa);
        int pa = produtores_ativos;
        sem_post(&pc_mutex_pa);
        if (pa == 0 && buf.count == 0) break;

        sem_wait(&pc_mutex);
        if (buf.count == 0) { sem_post(&pc_mutex); sem_post(&pc_full); break; }

        Transacao t = buf.dados[buf.out];
        buf.out = (buf.out + 1) % BUFFER_MAX;
        buf.count--;

        timestamp(ts, sizeof(ts));
        printf(MAG "[%s] Consumidor %d processou TX#%d (%s R$%.2f → conta %d)\n" RST,
               ts, p->id, t.id, t.tipo, t.valor, t.conta_destino);
        exibir_buffer();

        sem_post(&pc_mutex);
        sem_post(&pc_empty);

        msleep(p->delay_ms + rand() % 400);
    }

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Consumidor %d FINALIZADO\n" RST, ts, p->id);

    free(p);
    return NULL;
}

static void run_pc(int np, int nc, int ni, int dp, int dc, const char *label) {
    printf("\n" WHT "══════  %s  ══════\n" RST, label);

    memset(&buf, 0, sizeof(buf));
    tx_id_global = 0;
    sem_init(&pc_empty,    0, BUFFER_MAX);
    sem_init(&pc_full,     0, 0);
    sem_init(&pc_mutex,    0, 1);
    sem_init(&pc_mutex_pa, 0, 1);
    produtores_ativos = np;

    int total = np + nc;
    pthread_t *thr = malloc(total * sizeof(pthread_t));

    for (int i = 0; i < np; i++) {
        ParamPC *p = malloc(sizeof(ParamPC));
        p->id = i+1; p->delay_ms = dp; p->n_itens = ni;
        pthread_create(&thr[i], NULL, produtor, p);
    }
    for (int i = 0; i < nc; i++) {
        ParamPC *p = malloc(sizeof(ParamPC));
        p->id = i+1; p->delay_ms = dc; p->n_itens = -1;
        pthread_create(&thr[np+i], NULL, consumidor, p);
    }
    for (int i = 0; i < total; i++) pthread_join(thr[i], NULL);

    sem_destroy(&pc_empty); sem_destroy(&pc_full);
    sem_destroy(&pc_mutex); sem_destroy(&pc_mutex_pa);
    free(thr);
    printf("\nTodas as transações foram processadas.\n");
}

/* ─────────────────────────────────────────────────────────────────────────
 * V3 – SEM CONTROLE
 * ───────────────────────────────────────────────────────────────────────── */
static void *produtor_v3(void *arg) {
    ParamPC *p = (ParamPC *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(GRN "[%s] Produtor %d CRIADO (SEM CONTROLE)\n" RST, ts, p->id);

    for (int i = 0; i < p->n_itens; i++) {
        msleep(p->delay_ms);
        if (buf.count >= BUFFER_MAX) {
            timestamp(ts, sizeof(ts));
            printf(RED "[%s] Produtor %d: buffer cheio — não espera!\n" RST, ts, p->id);
            continue;
        }
        Transacao t = nova_transacao();
        int pos = buf.in;
        msleep(30);                /* delay artificial → força colisão */
        buf.dados[pos] = t;
        buf.in = (buf.in + 1) % BUFFER_MAX;
        buf.count++;

        timestamp(ts, sizeof(ts));
        printf(RED "[%s] *** Produtor %d escreveu TX#%d na pos %d"
               "  (in=%d count=%d) – possível colisão! ***\n" RST,
               ts, p->id, t.id, pos, buf.in, buf.count);
        exibir_buffer();
    }

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Produtor %d FINALIZADO\n" RST, ts, p->id);
    free(p);
    return NULL;
}

static void *consumidor_v3(void *arg) {
    ParamPC *p = (ParamPC *)arg;
    char ts[32];

    timestamp(ts, sizeof(ts));
    printf(MAG "[%s] Consumidor %d CRIADO (SEM CONTROLE)\n" RST, ts, p->id);
    msleep(500);

    for (int i = 0; i < p->n_itens; i++) {
        msleep(p->delay_ms);
        if (buf.count == 0) {
            timestamp(ts, sizeof(ts));
            printf(RED "[%s] Consumidor %d: buffer vazio — não espera!\n" RST, ts, p->id);
            continue;
        }
        int pos = buf.out;
        buf.out = (buf.out + 1) % BUFFER_MAX;
        buf.count--;

        timestamp(ts, sizeof(ts));
        printf(MAG "[%s] Consumidor %d consumiu pos %d  (count=%d)\n" RST,
               ts, p->id, pos, buf.count);
        exibir_buffer();
    }

    timestamp(ts, sizeof(ts));
    printf(GRY "[%s] Consumidor %d FINALIZADO\n" RST, ts, p->id);
    free(p);
    return NULL;
}

static void run_pc_v3(int np, int nc, int ni, int dp, int dc) {
    printf("\n" WHT "══════  V3 – SEM CONTROLE DE CONCORRÊNCIA  ══════\n" RST);
    memset(&buf, 0, sizeof(buf));
    tx_id_global = 0;
    sem_init(&pc_mutex_pa, 0, 1);

    int total = np + nc;
    pthread_t *thr = malloc(total * sizeof(pthread_t));
    for (int i = 0; i < np; i++) {
        ParamPC *p = malloc(sizeof(ParamPC));
        p->id = i+1; p->delay_ms = dp; p->n_itens = ni;
        pthread_create(&thr[i], NULL, produtor_v3, p);
    }
    for (int i = 0; i < nc; i++) {
        ParamPC *p = malloc(sizeof(ParamPC));
        p->id = i+1; p->delay_ms = dc; p->n_itens = ni;
        pthread_create(&thr[np+i], NULL, consumidor_v3, p);
    }
    for (int i = 0; i < total; i++) pthread_join(thr[i], NULL);
    sem_destroy(&pc_mutex_pa);
    free(thr);
}

/* ─────────────────────────────────────────────────────────────────────────
 * MENU – Produtores × Consumidores
 * ───────────────────────────────────────────────────────────────────────── */
static void menu_produtor_consumidor(void) {
    int op;
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║      PRODUTORES × CONSUMIDORES           ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  1. Vários produtores, 1 consumidor      ║\n");
    printf("║  2. Vários produtores, vários consumidores║\n");
    printf("║  3. Sem controle  (race condition)        ║\n");
    printf("║  0. Voltar                                ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("Opção: "); scanf("%d", &op);
    if (op == 0) return;

    int np, nc=1, ni, dp, dc;
    printf("Nº de produtores : "); scanf("%d", &np);
    if (op != 1) { printf("Nº de consumidores: "); scanf("%d", &nc); }
    printf("Itens por produtor: ");  scanf("%d", &ni);
    printf("Delay produtores  (ms, ex 300): "); scanf("%d", &dp);
    printf("Delay consumidores (ms, ex 500): "); scanf("%d", &dc);

    if (op == 1) run_pc(np, 1,  ni, dp, dc, "V1 – VÁRIOS PRODUTORES, 1 CONSUMIDOR");
    else if (op == 2) run_pc(np, nc, ni, dp, dc, "V2 – VÁRIOS PRODUTORES, VÁRIOS CONSUMIDORES");
    else         run_pc_v3(np, nc, ni, dp, dc);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    srand((unsigned)time(NULL));
    int op;

    while (1) {
        printf("\n╔══════════════════════════════════════════╗\n");
        printf("║     SISTEMA BANCÁRIO CONCORRENTE         ║\n");
        printf("║     SO – UFAM  |  Trabalho Prático 1     ║\n");
        printf("╠══════════════════════════════════════════╣\n");
        printf("║  1. Leitores × Escritores                ║\n");
        printf("║  2. Produtores × Consumidores            ║\n");
        printf("║  0. Sair                                 ║\n");
        printf("╚══════════════════════════════════════════╝\n");
        printf("Opção: "); scanf("%d", &op);

        switch (op) {
            case 1: menu_leitores_escritores(); break;
            case 2: menu_produtor_consumidor(); break;
            case 0: printf("\nAté logo!\n\n"); return 0;
            default: printf("Opção inválida.\n");
        }
    }
}
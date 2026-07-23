#include <math.h>
#include <stdio.h>
#include <iostream>
#include <random>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <chrono>
#include <ctime>

double RAIO = 10.0;

const double PI = 3.14159265358979323846;

// gerador global, usado por gera_double_aleatorio; por padrao inicializado
// com random_device (comportamento aleatorio de sempre)
std::mt19937 gerador_aleatorio(std::random_device{}());

// permite fixar a semente do gerador global, para tornar as execucoes
// reproduziveis (necessario para rodar os mesmos testes com sementes fixas)
void define_semente(unsigned int seed){
    gerador_aleatorio.seed(seed);
}

using namespace std;

// contadores globais da execucao inteira (secao 12: conexoes testadas /
// rejeitadas). A struct EstatisticasOtimizacao ja contava por bifurcacao;
// aqui acumulamos o total da arvore toda.
long long total_candidatos_testados = 0;
long long total_candidatos_rejeitados = 0;

typedef struct {
    double x;
    double y;
} Ponto;


typedef struct {
    Ponto a;
    Ponto b;
} Segmento;


typedef struct No {
    static int proximoId;

    struct No * esq;
    struct No * dir;
    struct No * pai;
    Ponto ponto;
    int id;

    double raio;
    double comprimento;
    double fluxo;
    double resistencia;
    double volume;
    int qtd_term_distal;

    No() : esq(nullptr), dir(nullptr), pai(nullptr), ponto(), id(proximoId++),
           raio(0.0), comprimento(0.0), fluxo(0.0),
           resistencia(0.0), volume(0.0), qtd_term_distal(0) {}
} No;

int No::proximoId = 0;

typedef No* ptrNo;

double dist_euclidiana(Ponto p1, Ponto p2) {
    return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
}


// Lembrando: o segmento de um no vai do PAI dele ate ele.
// A raiz nao tem pai, entao nao representa um segmento (comprimento 0)
double calcula_comprimento(ptrNo seg){
    if (seg == nullptr || seg->pai == nullptr){
        return 0.0;
    }
    return dist_euclidiana(seg->ponto, seg->pai->ponto);
}

// Resistencia hidraulica pela Lei de Poiseuille: R = 8*mu*l / (pi * r^4)
double calcula_resistencia(double mu, double comprimento, double raio){
    if (raio <= 0.0){
        return 0.0; //? sem raio definido ainda nao da pra calcular
    }
    return (8.0 * mu * comprimento) / (PI * pow(raio, 4));
}

// Volume de um segmento cilindrico: V = pi * r^2 * l
double calcula_volume(double comprimento, double raio){
    return PI * pow(raio, 2) * comprimento;
}


// Percorre em pos-ordem: visita os filhos primeiro, depois preenche o no atual.
// Folha (sem filhos) conta como 1; no interno eh a soma dos dois filhos.
int atualiza_qtd_terminais_distais(ptrNo no){
    if (no == nullptr){
        return 0;
    }

    // no terminal: nao tem filhos
    if (no->esq == nullptr && no->dir == nullptr){
        no->qtd_term_distal = 1;
        return 1;
    }

    // pos-ordem de verdade: primeiro desce nos filhos, depois soma
    int total = atualiza_qtd_terminais_distais(no->esq)
              + atualiza_qtd_terminais_distais(no->dir);

    no->qtd_term_distal = total;
    return total;
}


// Fluxos terminais iguais: o fluxo de um segmento eh proporcional ao numero
// de terminais que ele alimenta. Qj = qtd_term_distal(j) * Qterm
void atualiza_fluxos(ptrNo no, double Qterm){
    if (no == nullptr){
        return;
    }
    no->fluxo = no->qtd_term_distal * Qterm;
    atualiza_fluxos(no->esq, Qterm);
    atualiza_fluxos(no->dir, Qterm);
}


// MiniCCO-1 Parte D: lei de bifurcacao e escala dos raios

// Raio bruto de cada segmento a partir do fluxo: r = C * Q^(1/gamma), com C = 1
void atualiza_raios_por_fluxo(ptrNo no, double gamma){
    if (no == nullptr){
        return;
    }
    no->raio = pow(no->fluxo, 1.0 / gamma);
    atualiza_raios_por_fluxo(no->esq, gamma);
    atualiza_raios_por_fluxo(no->dir, gamma);
}

// Normaliza todos os raios dividindo pelo raio da raiz (a raiz fica com raio 1)
void normaliza_raios(ptrNo no, double raio_raiz){
    if (no == nullptr || raio_raiz <= 0.0){
        return;
    }
    no->raio = no->raio / raio_raiz;
    normaliza_raios(no->esq, raio_raiz);
    normaliza_raios(no->dir, raio_raiz);
}

// Preenche comprimento, resistencia e volume de cada no usando as funcoes da Parte A
void atualiza_geometria_segmentos(ptrNo no, double mu){
    if (no == nullptr){
        return;
    }
    no->comprimento = calcula_comprimento(no);
    no->resistencia = calcula_resistencia(mu, no->comprimento, no->raio);
    no->volume      = calcula_volume(no->comprimento, no->raio);
    atualiza_geometria_segmentos(no->esq, mu);
    atualiza_geometria_segmentos(no->dir, mu);
}

// Orquestrador: transforma uma arvore geometrica em arvore fisica completa.
// A ordem aqui NAO pode mudar: cada passo depende do anterior.
void atualiza_geometria_fisica(ptrNo raiz, double Qterm, double gamma, double mu){
    // 1. quantidade de terminais distais (pos-ordem)
    atualiza_qtd_terminais_distais(raiz);

    // 2. fluxos
    atualiza_fluxos(raiz, Qterm);

    // 3. raios: primeiro o bruto (Q^(1/gamma)), depois normaliza pela raiz
    atualiza_raios_por_fluxo(raiz, gamma);
    if (raiz != nullptr){
        normaliza_raios(raiz, raiz->raio);
    }

    atualiza_geometria_segmentos(raiz, mu);
}


double calcula_volume_total(ptrNo raiz){
    if (raiz == nullptr){
        return 0.0;
    }
    return raiz->volume
         + calcula_volume_total(raiz->esq)
         + calcula_volume_total(raiz->dir);
}

double funcao_custo_volume(ptrNo raiz){
    return calcula_volume_total(raiz);
}


// ===================== METRICAS GLOBAIS DA ARVORE =====================
// Bloco novo (CCO-X, secoes 9 e 12). Todas as funcoes aqui apenas LEEM a
// arvore ja construida; nenhuma delas altera a geometria ou a fisica.

// Profundidade maxima medida em SEGMENTOS (arestas), nao em nos.
// A raiz nao representa um segmento, entao ela esta no nivel 0 e uma folha
// ligada diretamente a ela tem profundidade 1.
int calcula_profundidade_maxima(ptrNo no){
    if (no == nullptr){
        return 0;
    }

    //? folha: nao ha mais nenhuma aresta abaixo dela
    if (no->esq == nullptr && no->dir == nullptr){
        return 0;
    }

    int prof_esq = calcula_profundidade_maxima(no->esq);
    int prof_dir = calcula_profundidade_maxima(no->dir);

    return 1 + max(prof_esq, prof_dir);
}

// Numero total de nos da arvore (inclui a raiz)
int conta_nos(ptrNo no){
    if (no == nullptr) return 0;
    return 1 + conta_nos(no->esq) + conta_nos(no->dir);
}

// Numero de segmentos: todo no que possui pai representa um segmento
int conta_segmentos(ptrNo no){
    if (no == nullptr) return 0;
    int atual = (no->pai != nullptr) ? 1 : 0;
    return atual + conta_segmentos(no->esq) + conta_segmentos(no->dir);
}

// Numero de terminais (folhas da arvore)
int conta_terminais(ptrNo no){
    if (no == nullptr) return 0;
    if (no->esq == nullptr && no->dir == nullptr) return 1;
    return conta_terminais(no->esq) + conta_terminais(no->dir);
}

// Numero de bifurcacoes: nos que possuem os DOIS filhos
int conta_bifurcacoes(ptrNo no){
    if (no == nullptr) return 0;
    int atual = (no->esq != nullptr && no->dir != nullptr) ? 1 : 0;
    return atual + conta_bifurcacoes(no->esq) + conta_bifurcacoes(no->dir);
}

// Soma dos comprimentos de todos os segmentos
double calcula_comprimento_total(ptrNo no){
    if (no == nullptr) return 0.0;
    return no->comprimento
         + calcula_comprimento_total(no->esq)
         + calcula_comprimento_total(no->dir);
}

// Soma dos raios de todos os SEGMENTOS (a raiz nao entra: nao eh segmento)
double soma_raios_segmentos(ptrNo no){
    if (no == nullptr) return 0.0;
    double atual = (no->pai != nullptr) ? no->raio : 0.0;
    return atual + soma_raios_segmentos(no->esq) + soma_raios_segmentos(no->dir);
}

// Resistencia hidraulica equivalente da subarvore que comeca em 'no',
// combinando serie (o proprio segmento) e paralelo (os dois filhos),
// conforme Karch et al. (1999), Eq. 7.
double calcula_resistencia_subarvore(ptrNo no){
    if (no == nullptr) return 0.0;

    double r_filhos = 0.0;

    if (no->esq != nullptr && no->dir != nullptr){
        double r_e = calcula_resistencia_subarvore(no->esq);
        double r_d = calcula_resistencia_subarvore(no->dir);

        //? associacao em paralelo dos dois ramos
        if (r_e > 0.0 && r_d > 0.0){
            r_filhos = 1.0 / (1.0 / r_e + 1.0 / r_d);
        }
    } else if (no->esq != nullptr){
        r_filhos = calcula_resistencia_subarvore(no->esq);
    } else if (no->dir != nullptr){
        r_filhos = calcula_resistencia_subarvore(no->dir);
    }

    //? em serie: o segmento do proprio no mais o conjunto dos filhos
    return no->resistencia + r_filhos;
}

// Raio ABSOLUTO da raiz (Karch et al. 1999, Eq. 12).
// Como os raios da arvore estao normalizados (raiz = 1), o valor de
// 'raio_raiz' seria sempre 1.0 e nao diria nada. Aqui recuperamos o valor
// fisico impondo que a arvore inteira produza a queda de pressao desejada:
//     Qperf = (pperf - pterm) / R_total   =>   k = (R_norm * Qperf / dp)^(1/4)
// Como a resistencia varia com r^-4, basta multiplicar todos os raios por k.
//! Este calculo eh feito apenas no FIM, para o relatorio. Ele nao entra na
//! otimizacao geometrica, para nao alterar qual posicao minimiza o volume.
double calcula_raio_raiz_absoluto(ptrNo raiz, double Qperf, double dp){
    if (raiz == nullptr || dp <= 0.0) return 0.0;

    double r_norm = calcula_resistencia_subarvore(raiz);
    if (r_norm <= 0.0) return 0.0;

    return pow(r_norm * Qperf / dp, 0.25);
}

// Pacote com tudo que o enunciado pede nas secoes 9 e 12
typedef struct {
    int Nterm_pedido;
    int n_nos;
    int n_segmentos;
    int n_terminais;
    int n_bifurcacoes;
    double comprimento_total;
    double volume_total;
    double raio_raiz;
    double raio_medio;
    int profundidade_maxima;
    double tempo_execucao;
    long long candidatos_testados;
    long long candidatos_rejeitados;
} MetricasGlobais;

MetricasGlobais calcula_metricas(ptrNo raiz, int Nterm_pedido, double tempo_execucao,
                                  double Qperf, double dp){
    MetricasGlobais m;

    m.Nterm_pedido       = Nterm_pedido;
    m.n_nos              = conta_nos(raiz);
    m.n_segmentos        = conta_segmentos(raiz);
    m.n_terminais        = conta_terminais(raiz);
    m.n_bifurcacoes      = conta_bifurcacoes(raiz);
    m.comprimento_total  = calcula_comprimento_total(raiz);
    m.volume_total       = calcula_volume_total(raiz);
    m.profundidade_maxima = calcula_profundidade_maxima(raiz);
    m.tempo_execucao     = tempo_execucao;
    m.candidatos_testados   = total_candidatos_testados;
    m.candidatos_rejeitados = total_candidatos_rejeitados;

    // raio absoluto da raiz e raio medio na mesma escala fisica
    double k = calcula_raio_raiz_absoluto(raiz, Qperf, dp);
    m.raio_raiz = k;

    double soma = soma_raios_segmentos(raiz);
    m.raio_medio = (m.n_segmentos > 0) ? (soma / m.n_segmentos) * k : 0.0;

    return m;
}

// Imprime no terminal as estatisticas pedidas (numero de nos, folhas,
// comprimento total, conexoes rejeitadas etc.)
void imprime_metricas(const MetricasGlobais& m){
    cout << "\n===================== METRICAS DA ARVORE =====================" << endl;
    cout << "Nterm (pedido)............: " << m.Nterm_pedido << endl;
    cout << "Numero total de nos.......: " << m.n_nos << endl;
    cout << "Numero de segmentos.......: " << m.n_segmentos << endl;
    cout << "Numero de terminais.......: " << m.n_terminais << endl;
    cout << "Numero de bifurcacoes.....: " << m.n_bifurcacoes << endl;
    cout << "Profundidade maxima.......: " << m.profundidade_maxima << endl;
    cout << "Comprimento total.........: " << m.comprimento_total << endl;
    cout << "Volume intravascular total: " << m.volume_total << endl;
    cout << "Raio da raiz (absoluto)...: " << m.raio_raiz << endl;
    cout << "Raio medio (absoluto).....: " << m.raio_medio << endl;
    cout << "Conexoes testadas.........: " << m.candidatos_testados << endl;
    cout << "Conexoes rejeitadas.......: " << m.candidatos_rejeitados << endl;
    cout << "Tempo de execucao (s).....: " << m.tempo_execucao << endl;
    cout << "==============================================================" << endl;
}

// Salva as metricas em CSV. O arquivo eh aberto em modo APPEND, entao varias
// execucoes (as 5 configuracoes x 3 sementes) se acumulam no mesmo arquivo,
// prontas para calcular media e desvio padrao depois.
void salva_metricas_em_csv(const MetricasGlobais& m, const string& nome_arquivo,
                            double R, double gamma, int M, unsigned int seed){
    //? checa se o arquivo ja existe, para so escrever o cabecalho uma vez
    bool existe = false;
    {
        ifstream teste(nome_arquivo);
        existe = teste.good();
    }

    ofstream out(nome_arquivo, ios::app);
    if (!out.is_open()){
        throw runtime_error("Nao foi possivel abrir o arquivo de metricas.");
    }

    if (!existe){
        out << "Nterm,R,gamma,M,seed,Nseg,n_nos,n_terminais,n_bifurcacoes,"
            << "comprimento_total,volume_total,raio_raiz,raio_medio,"
            << "profundidade_maxima,conexoes_testadas,conexoes_rejeitadas,"
            << "tempo_execucao\n";
    }

    out << m.Nterm_pedido << ","
        << R << ","
        << gamma << ","
        << M << ","
        << seed << ","
        << m.n_segmentos << ","
        << m.n_nos << ","
        << m.n_terminais << ","
        << m.n_bifurcacoes << ","
        << m.comprimento_total << ","
        << m.volume_total << ","
        << m.raio_raiz << ","
        << m.raio_medio << ","
        << m.profundidade_maxima << ","
        << m.candidatos_testados << ","
        << m.candidatos_rejeitados << ","
        << m.tempo_execucao << "\n";
}

// Devolve o horario atual formatado, usado no log
string horario_atual(){
    time_t agora = time(nullptr);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&agora));
    return string(buffer);
}

// Log da execucao (secao 9): registra parametros, semente e resultado.
// Tambem eh aberto em modo APPEND, virando o historico de todas as rodadas.
void salva_log_execucao(const MetricasGlobais& m, const string& nome_arquivo,
                         double R, double gamma, int M, unsigned int seed,
                         const string& inicio, const string& fim){
    ofstream out(nome_arquivo, ios::app);
    if (!out.is_open()){
        throw runtime_error("Nao foi possivel abrir o arquivo de log.");
    }

    out << "--------------------------------------------------------------\n";
    out << "[" << inicio << "] inicio da execucao\n";
    out << "  parametros: Nterm=" << m.Nterm_pedido
        << " R=" << R
        << " gamma=" << gamma
        << " M=" << M
        << " seed=" << seed << "\n";
    out << "  arvore gerada: " << m.n_nos << " nos, "
        << m.n_segmentos << " segmentos, "
        << m.n_terminais << " terminais, "
        << m.n_bifurcacoes << " bifurcacoes\n";
    out << "  profundidade maxima: " << m.profundidade_maxima << "\n";
    out << "  comprimento total: " << m.comprimento_total << "\n";
    out << "  volume intravascular total: " << m.volume_total << "\n";
    out << "  raio da raiz (absoluto): " << m.raio_raiz << "\n";
    out << "  raio medio (absoluto): " << m.raio_medio << "\n";
    out << "  conexoes testadas: " << m.candidatos_testados
        << " | rejeitadas por intersecao: " << m.candidatos_rejeitados << "\n";
    out << "  tempo de execucao: " << m.tempo_execucao << " s\n";
    out << "[" << fim << "] fim da execucao\n";
}


// alpha + beta + lambda = 1, devolve o ponto cartesiano correspondente:
// X = alpha*A + beta*B + lambda*C
Ponto ponto_baricentrico(Ponto A, Ponto B, Ponto C, double alpha, double beta, double lambda){
    Ponto X;
    X.x = alpha * A.x + beta * B.x + lambda * C.x;
    X.y = alpha * A.y + beta * B.y + lambda * C.y;
    return X;
}

// Verifica se um ponto P esta dentro (ou na borda) do triangulo ABC.
// um dos 3 lados do triangulo. Se todos os sinais forem iguais (ou zero),
// o ponto esta dentro; se houver mistura de sinal positivo e negativo,
// o ponto esta fora.
int ponto_dentro_triangulo(Ponto A, Ponto B, Ponto C, Ponto P){
    auto sinal = [](Ponto p1, Ponto p2, Ponto p3){
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };

    double d1 = sinal(P, A, B);
    double d2 = sinal(P, B, C);
    double d3 = sinal(P, C, A);

    bool tem_negativo = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool tem_positivo = (d1 > 0) || (d2 > 0) || (d3 > 0);

    // se nao ha mistura de sinais, P esta dentro (ou em cima da borda)
    return !(tem_negativo && tem_positivo);
}

// Orientacao de 3 pontos: 0 = colineares, 1 = sentido horario, 2 = anti-horario
int orientacao(Ponto p, Ponto q, Ponto r){
    double val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
    if (fabs(val) < 1e-12) return 0;
    return (val > 0.0) ? 1 : 2;
}

// Assumindo que p, q, r sao colineares, verifica se q esta dentro do
// retangulo (bounding box) definido por p e r
bool ponto_no_segmento_colinear(Ponto p, Ponto q, Ponto r){
    return (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
            q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y));
}

// Testa se os segmentos (p1,q1) e (p2,q2) se intersectam (caso geral +
// casos degenerados/colineares)
bool segmentos_se_intersectam(Ponto p1, Ponto q1, Ponto p2, Ponto q2){
    int o1 = orientacao(p1, q1, p2);
    int o2 = orientacao(p1, q1, q2);
    int o3 = orientacao(p2, q2, p1);
    int o4 = orientacao(p2, q2, q1);

    if (o1 != o2 && o3 != o4){
        return true; //* caso geral: as orientacoes se alternam -> ha cruzamento
    }

    //* casos especiais de colinearidade (pontos sobre o mesmo segmento)
    if (o1 == 0 && ponto_no_segmento_colinear(p1, p2, q1)) return true;
    if (o2 == 0 && ponto_no_segmento_colinear(p1, q2, q1)) return true;
    if (o3 == 0 && ponto_no_segmento_colinear(p2, p1, q2)) return true;
    if (o4 == 0 && ponto_no_segmento_colinear(p2, q1, q2)) return true;

    return false;
}

// Percorre a arvore toda verificando se o segmento (pa,pb) cruza algum
// segmento ja existente. 'ignorar1', 'ignorar2' e 'ignorar3' sao os TRES nos
// envolvidos na bifurcacao candidata (bifurcacao, no_segmento e novo).
bool segmento_intersecta_arvore(No* no, Ponto pa, Ponto pb, No* ignorar1, No* ignorar2, No* ignorar3){
    if (no == nullptr) return false;

    if (no->pai != nullptr && no != ignorar1 && no != ignorar2 && no != ignorar3){
        if (segmentos_se_intersectam(pa, pb, no->ponto, no->pai->ponto)){
            return true;
        }
    }

    return segmento_intersecta_arvore(no->esq, pa, pb, ignorar1, ignorar2, ignorar3)
        || segmento_intersecta_arvore(no->dir, pa, pb, ignorar1, ignorar2, ignorar3);
}


// Estatisticas da busca em grade, usadas para os relatorios pedidos na
// Parte H / secao 14 (numero de conexoes testadas / rejeitadas por intersecao)
typedef struct {
    int candidatos_testados;
    int candidatos_rejeitados_intersecao;
} EstatisticasOtimizacao;

// --- Parte G: avaliacao de UMA posicao candidata para a bifurcacao ------
// Passos do enunciado (secao 7 / "Parte G"):
//   1. altera temporariamente a posicao da bifurcacao          -> feito aqui
//   2-4. atualiza comprimentos, fluxos e raios da arvore inteira -> atualiza_geometria_fisica
//   5. calcula o volume total                                   -> funcao_custo_volume
//   6. verifica intersecao geometrica                           -> segmento_intersecta_arvore
//   7. quem chama esta funcao eh quem guarda o melhor valor de J (feito em
//      otimiza_bifurcacao_por_grade)
double avalia_bifurcacao(No* raiz, No* bifurcacao, Ponto X, No* no_segmento, No* novo,
                          double Qterm, double gamma, double mu, bool* houve_intersecao){
    // 1. reposiciona temporariamente a bifurcacao no candidato X
    bifurcacao->ponto = X;

    // 2-4. recalcula terminais/fluxos/raios/comprimentos/resistencias/volumes
    atualiza_geometria_fisica(raiz, Qterm, gamma, mu);

    // 6. verifica se os DOIS segmentos novos (bifurcacao-no_segmento e
    // bifurcacao-novo) cruzam algum segmento ja existente na arvore
    if (houve_intersecao != nullptr){
        *houve_intersecao =
            segmento_intersecta_arvore(raiz, bifurcacao->ponto, no_segmento->ponto, bifurcacao, no_segmento, novo)
         || segmento_intersecta_arvore(raiz, bifurcacao->ponto, novo->ponto,        bifurcacao, no_segmento, novo);
    }

    // 5. o "custo" da configuracao candidata eh o volume intravascular total (Parte E)
    return funcao_custo_volume(raiz);
}

// --- Parte F: busca exaustiva em grade dentro do triangulo ABC ----------
// A = ponto proximal do segmento antigo (pai de no_segmento, ANTES da rewire)
// B = ponto distal do segmento antigo   (posicao de no_segmento)
// C = novo ponto terminal               (posicao de 'novo')
// Discretiza alpha e beta em uma grade de resolucao M (secao 10.2 do
// enunciado) e testa cada X = alpha*A + beta*B + lambda*C, mantendo o melhor
// (menor volume total, dentre os candidatos que nao geram intersecao).
// Ao final, deixa a bifurcacao posicionada no melhor ponto encontrado
// (ou, se nenhum candidato valido foi achado, mantem a posicao original).
Ponto otimiza_bifurcacao_por_grade(No* raiz, No* bifurcacao, No* no_segmento, No* novo,
                                    Ponto A, Ponto B, Ponto C, int M,
                                    double Qterm, double gamma, double mu,
                                    double* melhor_custo,
                                    EstatisticasOtimizacao* estat = nullptr){
    Ponto melhor_ponto = bifurcacao->ponto; // fallback = posicao atual (ponto medio)
    double custo_min = numeric_limits<double>::infinity();

    if (estat != nullptr){
        estat->candidatos_testados = 0;
        estat->candidatos_rejeitados_intersecao = 0;
    }

    for (int i = 0; i <= M; i++){
        for (int j = 0; j <= M - i; j++){
            double alpha  = i / (double) M;
            double beta   = j / (double) M;
            double lambda = 1.0 - alpha - beta; // por construcao, sempre >= 0

            Ponto X = ponto_baricentrico(A, B, C, alpha, beta, lambda);

            //? checagem redundante (a construcao de i,j ja garante X dentro do
            //? triangulo), mas mantida como "cinto e suspensorio" pedido na Parte F
            if (!ponto_dentro_triangulo(A, B, C, X)) continue;

            if (estat != nullptr) estat->candidatos_testados++;

            bool intersecta = false;
            double custo = avalia_bifurcacao(raiz, bifurcacao, X, no_segmento, novo,
                                              Qterm, gamma, mu, &intersecta);

            if (intersecta){
                // Parte G, item 6/7: candidato invalido -> descarta e nao entra
                // na disputa pelo menor volume
                if (estat != nullptr) estat->candidatos_rejeitados_intersecao++;
                continue;
            }

            if (custo < custo_min){
                custo_min    = custo;
                melhor_ponto = X;
            }
        }
    }

    if (melhor_custo != nullptr) *melhor_custo = custo_min;

    // deixa a arvore de fato configurada com a melhor posicao encontrada (X*)
    bifurcacao->ponto = melhor_ponto;
    atualiza_geometria_fisica(raiz, Qterm, gamma, mu);

    return melhor_ponto;
}


double gera_double_aleatorio(double min = -RAIO, double max = RAIO) {
    uniform_real_distribution<double> dist(min, max);
    return dist(gerador_aleatorio);
}


// Gera um ponto aleatorio dentro do circulo
Ponto gera_ponto_aleatorio(){
    Ponto p;

    do{
        p = {gera_double_aleatorio(), gera_double_aleatorio()};
    } while (dist_euclidiana(p, {0,0}) > RAIO);
    
    return p;
}

void salva_segmentos_em_txt(No* raiz, const string& nome_arquivo) {
    ofstream out(nome_arquivo);
    if (!out.is_open()) {
        throw runtime_error("Nao foi possivel abrir o arquivo de saida.");
    }

    function<void(No*)> dfs = [&](No* no) {
        if (no == nullptr) return;

        if (no->esq != nullptr) {
            out << no->ponto.x << " " << no->ponto.y << " "
                << no->esq->ponto.x << " " << no->esq->ponto.y << "\n";
            dfs(no->esq);
        }

        if (no->dir != nullptr) {
            out << no->ponto.x << " " << no->ponto.y << " "
                << no->dir->ponto.x << " " << no->dir->ponto.y << "\n";
            dfs(no->dir);
        }
    };

    dfs(raiz);
}

// Exporta os segmentos da arvore em CSV (formato pedido no enunciado do MiniCCO-1).
// Cada segmento vai do PAI (proximal, x0/y0) ate o no (distal, x1/y1).
// A raiz nao vira linha porque nao representa um segmento (nao tem pai).
// No CCO-X a secao 9 pede tambem as colunas z0/z1; como o modelo eh 2D, elas
// sao escritas com valor zero, mantendo o arquivo compativel com a versao 3D.
void salva_segmentos_em_csv(ptrNo raiz, const string& nome_arquivo){
    ofstream out(nome_arquivo);
    if (!out.is_open()){
        throw runtime_error("Nao foi possivel abrir o arquivo CSV de saida.");
    }

    // cabecalho com os campos pedidos no enunciado
    out << "id,pai,x0,y0,z0,x1,y1,z1,raio,comprimento,fluxo,resistencia,volume\n";

    function<void(ptrNo)> dfs = [&](ptrNo no){
        if (no == nullptr) return;

        if (no->pai != nullptr){ //? so vira linha quem tem segmento (tem pai)
            out << no->id << ","
                << no->pai->id << ","
                << no->pai->ponto.x << "," << no->pai->ponto.y << ",0,"
                << no->ponto.x << "," << no->ponto.y << ",0,"
                << no->raio << ","
                << no->comprimento << ","
                << no->fluxo << ","
                << no->resistencia << ","
                << no->volume << "\n";
        }

        dfs(no->esq);
        dfs(no->dir);
    };

    dfs(raiz);
}

No* inicializa_arvore(){
    No *raiz = new No;
    raiz->ponto = {0.0, RAIO};

    No* filho = new No;

    do{
        filho->ponto = gera_ponto_aleatorio();
    }while(dist_euclidiana(raiz->ponto, filho->ponto) < 1.5 * RAIO);

    filho->pai = raiz;

    if (filho->ponto.x < 0){
        raiz->esq = filho;
    }else{
        raiz->dir = filho;
    }


    return raiz;
}


//* Função que calcula a distancia entre um ponto e um segmento
double dist_ponto_seg(Ponto p0, No* seg){
    //! a raiz nao tem pai, entao nao forma segmento; sem essa guarda o
    //! seg->pai->ponto abaixo estoura em ponteiro nulo
    if (seg == nullptr || seg->pai == nullptr)
    {
        return 9999;
    }   


    Ponto seg0 = seg->ponto;
    Ponto seg1 = seg->pai->ponto;

    // Vetor do segmento
    double dx = seg1.x - seg0.x;
    double dy = seg1.y - seg0.y;
    double len2 = dx*dx + dy*dy;

    // Caso o segmento seja degenerado (um ponto)
    if (len2 == 0.0) {
        dx = p0.x - seg0.x;
        dy = p0.y - seg0.y;
        return sqrt(dx*dx + dy*dy);
    }

    // Parâmetro de projeção do ponto sobre a reta suporte do segmento
    double t = ((p0.x - seg0.x)*dx + (p0.y - seg0.y)*dy) / len2;

    // Ponto do segmento mais próximo de p0
    double prox_x, prox_y;
    if (t < 0.0) {
        prox_x = seg0.x;
        prox_y = seg0.y;
    } else if (t > 1.0) {
        prox_x = seg1.x;
        prox_y = seg1.y;
    } else {
        prox_x = seg0.x + t * dx;
        prox_y = seg0.y + t * dy;
    }

    // Distância entre p0 e o ponto mais próximo
    dx = p0.x - prox_x;
    dy = p0.y - prox_y;
    return sqrt(dx*dx + dy*dy);
}

No* melhor_segmento(No* raiz, Ponto p){

    if (raiz == nullptr)
        return nullptr;
    
    No* menor = nullptr;
    
    
    No* menor_dir = melhor_segmento(raiz->dir, p);

    if (menor_dir){ //? checando se não é nulo

        double dist_atual = dist_ponto_seg(p, menor);

        if (dist_ponto_seg(p, menor_dir) < dist_atual) menor = menor_dir;
    }


    No* menor_esq = melhor_segmento(raiz->esq, p);

    if (menor_esq){ //? checando se não é nulo

        double dist_atual = dist_ponto_seg(p, menor);

        if (dist_ponto_seg(p, menor_esq) < dist_atual) menor = menor_esq;
        
    }


    if (raiz->pai){ //? Quando checamos o nó raiz devemos tambem checaar se o no pai não é nulo
        double dist_atual = dist_ponto_seg(p, menor);

        if (dist_ponto_seg(p, raiz) < dist_atual) menor = raiz;
    }
    
    return menor;
}

// Função auxiliar para varrer a árvore inteira e descobrir qual a menor distância do novo ponto para QUALQUER segmento existente
void calcula_dist_minima_arvore(No* no, Ponto p, double &min_dist) {
    if (no == nullptr) return;
    
    // Só calcula se o nó tiver pai (pois o segmento é formado entre o nó e seu pai)
    if (no->pai != nullptr) {
        double d = dist_ponto_seg(p, no);
        if (d < min_dist) {
            min_dist = d;
        }
    }
    
    calcula_dist_minima_arvore(no->esq, p, min_dist);
    calcula_dist_minima_arvore(no->dir, p, min_dist);
}

// Função que encapsula a lógica de tentativas e decaimento do critério
Ponto gera_ponto_valido(No* raiz) {
    double d_thresh = RAIO / 2.0; // Começa com um valor alto para o critério
    Ponto p;
    
    while (true) {
        // Tenta 10 vezes achar um ponto que respeite o critério atual
        for (int i = 0; i < 10; i++) {
            p = gera_ponto_aleatorio();
            double min_dist = 99999.0;
            
            calcula_dist_minima_arvore(raiz, p, min_dist);
            
            // Se o ponto ficou a uma distância maior ou igual ao limiar de TODOS os segmentos, ele é válido
            if (min_dist >= d_thresh) {
                return p;
            }
        }
        // Se as 10 tentativas falharem, abaixa o critério em 10% e volta para o loop
        d_thresh *= 0.9;
    }
}
// 


// raiz_arvore: no raiz global da arvore (necessario para recalcular a geometria
//              fisica da arvore inteira durante a otimizacao)
// Qterm, gamma, mu: parametros fisicos (Partes A-D)
// M: resolucao da grade de busca da bifurcacao (Parte F, secao 10.2)
void adiciona_no(No* raiz, No* raiz_arvore, double Qterm, double gamma, double mu, int M){

    No* novo = new No;
    novo->ponto = gera_ponto_valido(raiz); // <-- Substituído o gera_ponto_aleatorio() pela nova lógica
    
    cout << "Adicionando ponto " << novo->id << ": X = " << novo->ponto.x << ", Y = " << novo->ponto.y << endl;
    
    No* no_segmento = melhor_segmento(raiz, novo->ponto);

    //* Pontos do triangulo ABC usados na otimizacao geometrica (Parte F):
    //*   A = ponto proximal do segmento antigo (o "pai" de no_segmento, ANTES
    //*       de qualquer religacao de ponteiros abaixo)
    //*   B = ponto distal do segmento antigo (posicao atual de no_segmento)
    //*   C = o novo ponto terminal
    Ponto A = no_segmento->pai->ponto;
    Ponto B = no_segmento->ponto;
    Ponto C = novo->ponto;

    No* bifurcacao = new No;
    // posicao inicial = ponto medio do segmento antigo (igual ao MiniCCO-0);
    // essa posicao sera refinada logo abaixo pela busca em grade (Parte F)
    bifurcacao->ponto = {(no_segmento->ponto.x + no_segmento->pai->ponto.x) / 2, (no_segmento->ponto.y + no_segmento->pai->ponto.y) / 2};
    
    novo->pai = bifurcacao;

    //* descobrir de qual lado é o filho do melhor segmento
    if (no_segmento->pai->dir == no_segmento){
        //* no_segmento é o filho da direita

        bifurcacao->dir = no_segmento;
        bifurcacao->pai = no_segmento->pai;
        no_segmento->pai->dir = bifurcacao;
        no_segmento->pai = bifurcacao;

        bifurcacao->esq = novo;

    }else if (no_segmento->pai->esq == no_segmento){
        //* no_segmento é o filho da esquerda
        bifurcacao->esq = no_segmento;
        bifurcacao->pai = no_segmento->pai;
        no_segmento->pai->esq = bifurcacao;
        no_segmento->pai = bifurcacao;

        bifurcacao->dir = novo;

    }else{
        //! caso de erro: nao achou de qual lado esta o filho do melhor segmento
        cout << "Erro: no_segmento sem relacao de parentesco valida" << endl;
        return;
    }

    //* PARTE F/G: com a topologia ja fixada acima (quem eh filho de quem),
    //* falta so achar a MELHOR POSICAO (x,y) para 'bifurcacao' dentro do
    //* triangulo ABC. Isso e feito recalculando a arvore fisica inteira
    //* (Partes A-E) para cada candidato da grade e ficando com o que
    //* minimiza o volume intravascular total (Parte E), descartando
    //* candidatos que geram intersecao geometrica (Parte G, item 6).
    double melhor_custo = 0.0;
    EstatisticasOtimizacao estat;

    otimiza_bifurcacao_por_grade(raiz_arvore, bifurcacao, no_segmento, novo,
                                  A, B, C, M, Qterm, gamma, mu,
                                  &melhor_custo, &estat);

    // acumula nos contadores globais da execucao (secao 12)
    total_candidatos_testados   += estat.candidatos_testados;
    total_candidatos_rejeitados += estat.candidatos_rejeitados_intersecao;

    cout << "  -> bifurcacao otimizada em (" << bifurcacao->ponto.x << ", " << bifurcacao->ponto.y
         << ") | custo (volume) = " << melhor_custo
         << " | candidatos testados = " << estat.candidatos_testados
         << " | rejeitados por intersecao = " << estat.candidatos_rejeitados_intersecao << endl;
}

int main(int argc, char *argv[]) {


    if (argc != 5 && argc != 6 && argc != 1) {
        cerr << "Uso: " << argv[0] << " <Nterm> <R> <gamma> <M> [seed]" << endl;
        return 1;
    }

    int N_term   = 0;
    double raio_entrada = 0;
    double gamma = 0;
    int M = 0;

    if (argc == 1) {
        cout << "Nenhum argumento fornecido. A simulação será executada com valores padrão: Nterm=50, R=10.0, gamma=3.0, M=10." << endl;
        N_term = 50;
        raio_entrada = 10.0;
        gamma = 3.0;
        M = 10;
    } else {
        N_term   = atoi(argv[1]);
        raio_entrada = atof(argv[2]);
        gamma = atof(argv[3]);
        M        = atoi(argv[4]);
    }

    //! a arvore inicial ja nasce com 1 terminal, entao Nterm < 1 nao faz sentido
    if (N_term < 1){
        cerr << "Nterm deve ser no minimo 1." << endl;
        return 1;
    }

    // A semente eh SEMPRE definida e registrada. Se o usuario nao passar uma,
    // sorteamos uma e a gravamos no log/metricas, de modo que qualquer
    // execucao possa ser reproduzida depois.
    unsigned int seed = 0;

    if (argc == 6){
        seed = strtoul(argv[5], nullptr, 10);
    } else {
        random_device rd;
        seed = rd();
    }

    define_semente(seed);
    cout << "Semente fixada: " << seed << endl;

    RAIO = raio_entrada;

    // parametros fisicos sugeridos no enunciado (secao 4)
    double Qperf = 8.33e-6;
    double pperf = 1.33e4;
    double pterm = 7.98e3;
    double mu    = 3.6e-3;
    double Qterm = Qperf / N_term;
    double dp    = pperf - pterm;

    // marca o inicio para medir o tempo de execucao (secao 12)
    string hora_inicio = horario_atual();
    auto t_inicio = chrono::high_resolution_clock::now();

    No* raiz = inicializa_arvore();

    // A arvore ja nasce com 1 terminal (o filho criado em inicializa_arvore) e
    // cada adiciona_no acrescenta exatamente mais 1. Por isso o laco roda
    // N_term - 1 vezes: assim a arvore final tem exatamente N_term terminais,
    //? antes saiam N_term + 1, o que desalinhava a metrica e o Qterm.
    for (int i = 0; i < N_term - 1; i++){
        adiciona_no(raiz, raiz, Qterm, gamma, mu, M);
    }

    // garante que a arvore final esta com a geometria fisica 100% atualizada
    atualiza_geometria_fisica(raiz, Qterm, gamma, mu);

    auto t_fim = chrono::high_resolution_clock::now();
    double tempo_execucao = chrono::duration<double>(t_fim - t_inicio).count();
    string hora_fim = horario_atual();

    salva_segmentos_em_csv(raiz, "arvore.csv");

    // metricas globais: terminal, arquivo CSV acumulativo e log
    MetricasGlobais m = calcula_metricas(raiz, N_term, tempo_execucao, Qperf, dp);

    imprime_metricas(m);
    salva_metricas_em_csv(m, "metricas.csv", RAIO, gamma, M, seed);
    salva_log_execucao(m, "log_execucao.txt", RAIO, gamma, M, seed, hora_inicio, hora_fim);

    cout << "\nArquivos gerados: arvore.csv, metricas.csv, log_execucao.txt" << endl;

    return 0;
}

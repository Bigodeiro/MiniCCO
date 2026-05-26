#include <math.h>
#include <stdio.h>
#include <iostream>
#include <random>
#include <fstream>
#include <functional>
#include <stdexcept>

#define RAIO 10.0

using namespace std;

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

    No() : esq(nullptr), dir(nullptr), pai(nullptr), ponto(), id(proximoId++) {}
} No;

int No::proximoId = 0;

double dist_euclidiana(Ponto p1, Ponto p2) {
    return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
}

// Isso aqui é uma solução que tava no stack overflow e so funciona e é isso 
double gera_double_aleatorio(double min = -10, double max = RAIO) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_real_distribution<double> dist(min, max);
    return dist(gen);
}


// Gera um ponto aleatorio dentro do circulo
Ponto gera_ponto_aleatorio(){
    Ponto p;

    do{
        p = {gera_double_aleatorio(), gera_double_aleatorio()};
    } while (dist_euclidiana(p, {0,0}) > 10);
    
    return p;
}


// Essa função 100% gepetada mas ta funcionando, depois temo q ver ela certinho
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

No* inicializa_arvore(){
    No *raiz = new No;
    raiz->ponto = {0.0, RAIO};

    No* filho = new No;


    //TODO Fazer esse primeiro Nó seguir aa distancia la que ele quer
    do{
        filho->ponto = gera_ponto_aleatorio();
    }while(dist_euclidiana(raiz->ponto, filho->ponto) < 15);

    filho->pai = raiz;

    if (filho->ponto.x < 0){
        raiz->esq = filho;
    }else{
        raiz->dir = filho;
    }


    return raiz;
}


//TODO Sessãao 6 do PDF, tem varios jeitos de decidir o melhor segmento e a gente tem q implementar um deles
//* Essa funçãao vai retornar algum nó da arvore, e o melhor segmento vai ser entre esse nó e o pai dele
No* melhor_segmento(No* raiz){

    if (raiz->dir != nullptr)
        return raiz->dir;
    else
        return raiz->esq;
}

//TODO talvez a gente tenha que fazer uma função pra qual dos nós vaai ficaar de qual lado da bifurcação
bool decide_lado(){

    return true;
}


void adiciona_no(No* raiz){

    No* novo = new No;
    novo->ponto = gera_ponto_aleatorio();
    
    cout << "Adicionando ponto " << novo->id << ": X = " << novo->ponto.x << ", Y = " << novo->ponto.y << endl;
    
    No* no_segmento = melhor_segmento(raiz);
    
    
    No* bifurcacao = new No;
    bifurcacao->ponto = {(no_segmento->ponto.x + no_segmento->pai->ponto.x) / 2, (no_segmento->ponto.y + no_segmento->pai->ponto.y) / 2};
    
    novo->pai = bifurcacao;

    //TODO Nessas atribuições aqui eu acho que nao deve poder so botar no mesmo lado a moda caralha
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
        //! ai fudeu
        cout << "fudeu" << endl;
        return;
    }
}


int main(int argc, char *argv[]) {

    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <arg1> <arg2>" << endl;
        return 1;
    }

    No* raiz = inicializa_arvore();

    adiciona_no(raiz);
    adiciona_no(raiz);
    adiciona_no(raiz);

    salva_segmentos_em_txt(raiz, "arvore.txt");

    return 0;
}
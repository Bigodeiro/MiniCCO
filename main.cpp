#include <math.h>
#include <stdio.h>
#include <iostream>
#include <random>
#include <fstream>
#include <functional>
#include <stdexcept>

double RAIO = 10.0; // Agora é uma variável global que o main pode alterar

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


//TODO função gepetada que precisa ser revisada e entendida 
//* Função que calcula a distancia entre um ponto e um segmento
double dist_ponto_seg(Ponto p0, No* seg){
    if (seg == nullptr)
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

//TODO Sessãao 6 do PDF, tem varios jeitos de decidir o melhor segmento e todos dependem do score que dá pra cada segmento, esse é o maais básico e da pra melhorar
//* Essa funçãao vai retornar algum nó da arvore, e o melhor segmento vai ser entre esse nó e o pai dele
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


// Parte que Caua mudou: Critério de Distância 

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


void adiciona_no(No* raiz){

    No* novo = new No;
    novo->ponto = gera_ponto_valido(raiz); // <-- Substituído o gera_ponto_aleatorio() pela nova lógica
    
    cout << "Adicionando ponto " << novo->id << ": X = " << novo->ponto.x << ", Y = " << novo->ponto.y << endl;
    
    No* no_segmento = melhor_segmento(raiz, novo->ponto);
    
    
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

    // Verifica se passou 2 argumentos
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <N_term> <R>" << endl;
        return 1;
    }

    // Convertendo os argumentos de texto para números
    int N_term = atoi(argv[1]);
    double raio_entrada = atof(argv[2]);

    RAIO = raio_entrada;

    No* raiz = inicializa_arvore();

    for (int i = 0; i < N_term; i++){
        adiciona_no(raiz);
    }

    salva_segmentos_em_txt(raiz, "arvore.txt");

    return 0;
}

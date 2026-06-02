import pyvista as pv
import numpy as np
import sys
from collections import defaultdict # <-- Adicionado para a lógica de cores


def ler_segmentos(path):
    segmentos = []
    pontos = []

    with open(path, "r", encoding="utf-8") as f:
        for linha in f:
            linha = linha.strip()
            if not linha:
                continue

            x1, y1, x2, y2 = map(float, linha.split())
            p1 = (x1, y1, 0.0)
            p2 = (x2, y2, 0.0)

            segmentos.append((p1, p2))
            pontos.extend([p1, p2])

    return segmentos, pontos


def gerar_png(arquivo_txt, arquivo_png="arvore.png"):
    segmentos, pontos = ler_segmentos(arquivo_txt)

    if not segmentos:
        raise ValueError("O arquivo não contém segmentos para desenhar.")

    # --- INÍCIO DA ADIÇÃO: Lógica para colorir galhos irmãos ---
    adj = defaultdict(list)
    for p1, p2 in segmentos:
        adj[p1].append(p2)
        adj[p2].append(p1)

    # Encontra a raiz no topo (0, 10)
    raiz = min(adj.keys(), key=lambda p: p[0]**2 + (p[1] - 10.0)**2)

    lados = {raiz: 0}
    fila = [raiz]
    visitados = {raiz}
    while fila:
        atual = fila.pop(0)
        filhos = [n for n in adj[atual] if n not in visitados]
        for i, filho in enumerate(filhos):
            visitados.add(filho)
            lados[filho] = (i % 2) + 1
            fila.append(filho)
            
    cores_map = {0: "black", 1: "#1f77b4", 2: "#d62728"} # Tronco, Azul, Vermelho
    # --- FIM DA ADIÇÃO ---

    # Remove pontos duplicados
    pontos_unicos = []
    indice = {}
    conexoes = []

    for p1, p2 in segmentos:
        for p in (p1, p2):
            if p not in indice:
                indice[p] = len(pontos_unicos)
                pontos_unicos.append(p)

        conexoes.extend([2, indice[p1], indice[p2]])

    pontos_unicos = np.array(pontos_unicos, dtype=float)
    linhas = np.array(conexoes, dtype=np.int64)

    malha = pv.PolyData()
    malha.points = pontos_unicos
    malha.lines = linhas

    # --- Criar o círculo de raio 10 centrado em (0,0) ---
    raio = 10.0
    n_pontos = 100  # número de pontos para aproximar o círculo
    angulos = np.linspace(0, 2 * np.pi, n_pontos, endpoint=False)
    pontos_circulo = np.array([[raio * np.cos(a), raio * np.sin(a), 0.0] for a in angulos])
    # Fechar o círculo: repetir o primeiro ponto no final
    pontos_circulo_fechado = np.vstack([pontos_circulo, pontos_circulo[0]])
    
    # Criar uma linha conectando todos os pontos sequencialmente
    n = len(pontos_circulo_fechado)
    conexoes_circulo = [n] + list(range(n))  # polyline com n pontos conectados
    conexoes_circulo = np.array(conexoes_circulo, dtype=np.int64)
    
    malha_circulo = pv.PolyData()
    malha_circulo.points = pontos_circulo_fechado
    malha_circulo.lines = conexoes_circulo

    # --- Plotagem ---
    plotter = pv.Plotter(off_screen=True, window_size=(1200, 900))
    plotter.set_background("white")
    
    # Adicionar os segmentos da árvore com suas respectivas cores
    for p1, p2 in segmentos:
        lado = lados[p2] if lados.get(p2, 0) != 0 else lados.get(p1, 0)
        plotter.add_mesh(
            pv.Line(p1, p2),
            color=cores_map.get(lado, "black"),
            line_width=2,
        )
    
    # Adicionar o círculo (cor cinza, mesma espessura)
    plotter.add_mesh(
        malha_circulo,
        color="gray",
        line_width=3,
    )

    # Adicionar labels com coordenadas dos pontos (apenas 1 casa decimal)
    if (False):
        for i, ponto in enumerate(pontos_unicos):
            label = f"({ponto[0]:.1f}, {ponto[1]:.1f})"
            
            # Adicionar ponto como um pequeno marcador
            point_cloud = pv.PolyData([ponto])
            plotter.add_mesh(point_cloud, color="red", point_size=8, render_points_as_spheres=True)
            
            # Adicionar texto ao lado do ponto
            plotter.add_point_labels(
                [ponto],
                [label],
                point_size=0,
                font_size=12,
                text_color="blue",
                shadow=True,
                always_visible=True,
                show_points=False
            )

    # --- Configuração fixa para coordenadas entre -10 e 10 ---
    plotter.view_xy()
    plotter.camera.parallel_projection = True
    plotter.camera.position = (0.0, 0.0, 25.0)
    plotter.camera.focal_point = (0.0, 0.0, 0.0)
    plotter.camera.up = (0.0, 1.0, 0.0)
    plotter.camera.parallel_scale = 12.0
    plotter.camera.clipping_range = (1.0, 100.0)

    # Removida a chamada show_grid para evitar erro de API
    # Se quiser adicionar uma grade simples, use:
    # plotter.add_mesh(pv.Box(bounds=(-11,11,-11,11,-0.1,0.1)), style='wireframe', color='gray')

    plotter.show(screenshot=arquivo_png)
    plotter.close()

    print(f"PNG salvo em: {arquivo_png}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python plot.py arvore.txt [saida.png]")
        sys.exit(1)

    arquivo_txt = sys.argv[1]
    arquivo_png = sys.argv[2] if len(sys.argv) > 2 else "arvore.png"
    gerar_png(arquivo_txt, arquivo_png)

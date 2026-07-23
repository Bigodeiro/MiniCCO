import pyvista as pv
import numpy as np
import pandas as pd
import sys
from collections import defaultdict


def ler_segmentos(path):
    df = pd.read_csv(path)

    segmentos = []

    for _, row in df.iterrows():
        seg = {
            "id": int(row["id"]),
            "pai": int(row["pai"]),
            "p1": (float(row["x0"]), float(row["y0"]), 0.0),
            "p2": (float(row["x1"]), float(row["y1"]), 0.0),
            "raio": float(row["raio"]),
        }
        segmentos.append(seg)

    return segmentos


def descobre_raio_dominio(segmentos):
    # O raio do dominio nao vem no CSV, entao inferimos a partir dos dados:
    # pegamos a maior distancia da origem entre todos os pontos e arredondamos
    # um pouco pra cima, garantindo que o circulo envolva a arvore inteira.
    maior = 0.0

    for seg in segmentos:
        for p in (seg["p1"], seg["p2"]):
            d = np.sqrt(p[0] ** 2 + p[1] ** 2)
            if d > maior:
                maior = d

    return maior if maior > 0 else 1.0


def descobre_raiz_e_terminais(segmentos):
    # A raiz eh o unico 'pai' que nunca aparece como 'id' (ela nao tem segmento
    # proprio, entao nao vira linha no CSV).
    # Os terminais sao os 'id' que nunca aparecem como 'pai' de ninguem.
    ids = {seg["id"] for seg in segmentos}
    pais = {seg["pai"] for seg in segmentos}

    id_raiz = None
    for p in pais:
        if p not in ids:
            id_raiz = p
            break

    ids_terminais = ids - pais

    ponto_raiz = None
    pontos_terminais = []

    for seg in segmentos:
        if seg["pai"] == id_raiz:
            ponto_raiz = seg["p1"]
        if seg["id"] in ids_terminais:
            pontos_terminais.append(seg["p2"])

    return ponto_raiz, pontos_terminais


def descobre_lados(segmentos):
    # Mesma ideia do MiniCCO-0: percorremos a arvore em BFS e alternamos a cor
    # entre galhos irmaos, o que ajuda a validar visualmente a topologia.
    filhos = defaultdict(list)

    for seg in segmentos:
        filhos[seg["pai"]].append(seg["id"])

    ids = {seg["id"] for seg in segmentos}
    pais = {seg["pai"] for seg in segmentos}

    id_raiz = None
    for p in pais:
        if p not in ids:
            id_raiz = p
            break

    lados = {id_raiz: 0}
    fila = [id_raiz]

    while fila:
        atual = fila.pop(0)

        for i, filho in enumerate(filhos[atual]):
            lados[filho] = (i % 2) + 1
            fila.append(filho)

    return lados


def gerar_png(arquivo_csv, arquivo_png="arvore.png", raio_dominio=None):
    segmentos = ler_segmentos(arquivo_csv)

    if not segmentos:
        raise ValueError("O arquivo nao contem segmentos para desenhar.")

    # Se o raio nao foi passado por argumento, inferimos a partir do CSV.
    # Assim o script funciona tanto para R = 10 quanto para R = 0.05 (SI).
    if raio_dominio is None:
        raio_dominio = descobre_raio_dominio(segmentos)

    lados = descobre_lados(segmentos)
    ponto_raiz, pontos_terminais = descobre_raiz_e_terminais(segmentos)

    cores_map = {
        0: "black",
        1: "#1f77b4",
        2: "#d62728",
    }

    # ---------- Escala dos tubos ----------
    # Os raios do CSV vem normalizados (a raiz tem raio 1), entao precisamos
    # converte-los para a escala do desenho. Fixamos o tubo mais grosso em uma
    # fracao do raio do dominio para o resultado ficar legivel em qualquer escala.
    raio_max = max(seg["raio"] for seg in segmentos)
    escala_tubo = (raio_dominio * 0.025) / raio_max if raio_max > 0 else 1.0

    # ---------- Circulo do dominio ----------
    n_pontos = 100

    angulos = np.linspace(0, 2 * np.pi, n_pontos, endpoint=False)

    pontos_circulo = np.array(
        [
            [raio_dominio * np.cos(a), raio_dominio * np.sin(a), 0.0]
            for a in angulos
        ]
    )

    pontos_circulo = np.vstack([pontos_circulo, pontos_circulo[0]])

    conexoes_circulo = np.array(
        [len(pontos_circulo)] + list(range(len(pontos_circulo))),
        dtype=np.int64,
    )

    malha_circulo = pv.PolyData()
    malha_circulo.points = pontos_circulo
    malha_circulo.lines = conexoes_circulo

    # ---------- Plot ----------
    plotter = pv.Plotter(off_screen=True, window_size=(1200, 900))
    plotter.set_background("white")

    # Cada segmento vira um tubo com espessura proporcional ao seu raio
    for seg in segmentos:
        linha = pv.Line(seg["p1"], seg["p2"])
        tubo = linha.tube(radius=seg["raio"] * escala_tubo)

        lado = lados.get(seg["id"], 0)

        plotter.add_mesh(
            tubo,
            color=cores_map.get(lado, "black"),
            smooth_shading=True,
        )

    plotter.add_mesh(
        malha_circulo,
        color="gray",
        line_width=3,
    )

    # ---------- Raiz e terminais destacados ----------
    if ponto_raiz is not None:
        plotter.add_mesh(
            pv.PolyData(np.array([ponto_raiz])),
            color="green",
            point_size=18,
            render_points_as_spheres=True,
        )

    if pontos_terminais:
        plotter.add_mesh(
            pv.PolyData(np.array(pontos_terminais)),
            color="orange",
            point_size=8,
            render_points_as_spheres=True,
        )

    # ---------- Camera ----------
    # A camera tambem escala com o dominio, senao a arvore some quando R eh pequeno
    plotter.view_xy()
    plotter.camera.parallel_projection = True
    plotter.camera.position = (0.0, 0.0, raio_dominio * 2.5)
    plotter.camera.focal_point = (0.0, 0.0, 0.0)
    plotter.camera.up = (0.0, 1.0, 0.0)
    plotter.camera.parallel_scale = raio_dominio * 1.2
    plotter.camera.clipping_range = (raio_dominio * 0.1, raio_dominio * 10.0)

    plotter.show(screenshot=arquivo_png)
    plotter.close()

    print(f"PNG salvo em: {arquivo_png}")
    print(f"Raio do dominio usado no desenho: {raio_dominio}")
    print(f"Segmentos desenhados: {len(segmentos)} | terminais: {len(pontos_terminais)}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python plot.py arvore.csv [saida.png] [raio_dominio]")
        sys.exit(1)

    arquivo_csv = sys.argv[1]
    arquivo_png = sys.argv[2] if len(sys.argv) > 2 else "arvore.png"
    raio_dominio = float(sys.argv[3]) if len(sys.argv) > 3 else None

    gerar_png(arquivo_csv, arquivo_png, raio_dominio)

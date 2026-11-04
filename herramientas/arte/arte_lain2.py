"""Lleva la imagen de Lain a la placa: recorte, pixelado limpio, caras y capas.

Genera un PNG de control y el header C con la capa base y los parches de cara.
"""
import sys

from PIL import Image, ImageDraw

ORIGEN = "ref_lain.png"
BBOX = (264, 28, 1092, 768)      # donde esta el dibujo dentro de la imagen
ANCHO = 207                      # ancho final en pixeles de la placa
PALETA = 18

CLAVE = (255, 0, 255)
SKIN = None
INK = None


def base():
    im = Image.open(ORIGEN).convert("RGB").crop(BBOX)
    alto = round(im.height * ANCHO / im.width)
    im = im.resize((ANCHO, alto), Image.BOX)
    im = im.quantize(colors=PALETA, method=Image.MEDIANCUT, dither=Image.NONE).convert("RGB")
    return im


def paleta(im):
    """Los dos colores que necesitamos para pintar caras: piel e ink."""
    from collections import Counter
    c = Counter(im.getdata())
    # la piel es el color claro y calido mas frecuente (el fondo es azulado)
    piel = max((col for col in c if col[0] > 195 and col[0] > col[2] + 10), key=lambda col: c[col])
    tinta = min((col for col in c if sum(col) < 180), key=lambda col: sum(col))
    return piel, tinta


def ojos(im, cx_izq, cx_der, cy, ancho, alto):
    """Devuelve la caja de los ojos y pinta encima la cara pedida."""
    return (cx_izq - ancho, cy - alto, cx_der + ancho, cy + alto)


def cara(im, gesto, cajas, piel, tinta):
    """Pinta otra expresion borrando solo lo que es ojo (tinta y su brillo)."""
    out = im.copy()
    d = ImageDraw.Draw(out)
    for (x0, y0, x1, y1) in cajas:
        for y in range(y0, y1):
            for x in range(x0, x1):
                s = sum(out.getpixel((x, y)))
                if s < 200 or s > 690:        # pupila o reflejo blanco
                    out.putpixel((x, y), piel)
        cxi, cy = (x0 + x1) // 2, (y0 + y1) // 2
        w = (x1 - x0) // 3
        if gesto == "cerrada":
            d.line([(cxi - w, cy), (cxi - w // 2, cy + 2), (cxi + w // 2, cy + 2), (cxi + w, cy)],
                   fill=tinta, width=2)
        elif gesto == "feliz":
            d.line([(cxi - w, cy + 2), (cxi, cy - 3), (cxi + w, cy + 2)], fill=tinta, width=2)
        elif gesto == "agobiada":
            d.line([(cxi - w, cy - 5), (cxi + w, cy + 5)], fill=tinta, width=2)
            d.line([(cxi - w, cy + 5), (cxi + w, cy - 5)], fill=tinta, width=2)
        elif gesto == "concentrada":
            d.rectangle([cxi - w, cy - 1, cxi + w, cy + 3], fill=tinta)
    return out


def a565(img):
    w, h = img.size
    out = bytearray()
    for y in range(h):
        for x in range(w):
            p = img.getpixel((x, y))
            if p == (255, 255, 255) or p == (243, 249, 251):
                p = CLAVE
            c = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3)
            out += bytes((c >> 8, c & 0xFF))
    return w, h, bytes(out)


def trozos(data, por_linea=16):
    vals = [f"0x{(data[i] << 8) | data[i + 1]:04X}" for i in range(0, len(data), 2)]
    return "\n".join("  " + ", ".join(vals[i:i + por_linea]) + "," for i in range(0, len(vals), por_linea))


def main():
    im = base()
    piel, tinta = paleta(im)
    print("piel", piel, "tinta", tinta, "tamano", im.size, flush=True)
    # los ojos, medidos sobre el recorte: x 67..137, y 79..109
    cajas = [(69, 81, 90, 108), (115, 81, 136, 108)]
    print("cajas de ojos", cajas, flush=True)

    caras = {"NORMAL": im, "CERRADA": cara(im, "cerrada", cajas, piel, tinta),
             "FELIZ": cara(im, "feliz", cajas, piel, tinta),
             "AGOBIADA": cara(im, "agobiada", cajas, piel, tinta),
             "CONCENTRADA": cara(im, "concentrada", cajas, piel, tinta)}

    recorte = (cajas[0][0] - 4, cajas[0][1] - 4, cajas[1][2] + 4, cajas[1][3] + 4)
    im.save("lain_base.png")
    for nombre, img in caras.items():
        img.crop(recorte).save(f"lain_cara_{nombre.lower()}.png")

    cab = ['// Generado por arte_lain2.py: no editar a mano.',
           '// Pixel art de Lain tomado del diseno aprobado. 0xF81F = transparente.',
           '#pragma once',
           '#include <Arduino.h>',
           '',
           '#define LAIN_CLAVE 0xF81F']
    total = 0
    w, h, data = a565(im)
    total += len(data)
    cab += [f'#define LAIN_BASE_W {w}', f'#define LAIN_BASE_H {h}',
            f'static const uint16_t LAIN_BASE[{w * h}] PROGMEM = {{', trozos(data), '};', '']
    for nombre, img in caras.items():
        if nombre == "NORMAL":
            continue
        parche = img.crop(recorte)
        pw, ph, pdata = a565(parche)
        total += len(pdata)
        cab += [f'#define LAIN_CARA_{nombre}_X {recorte[0]}', f'#define LAIN_CARA_{nombre}_Y {recorte[1]}',
                f'#define LAIN_CARA_{nombre}_W {pw}', f'#define LAIN_CARA_{nombre}_H {ph}',
                f'static const uint16_t LAIN_CARA_{nombre}[{pw * ph}] PROGMEM = {{', trozos(pdata), '};', '']
    open(sys.argv[1] if len(sys.argv) > 1 else "lain_art2.h", "w").write("\n".join(cab))
    print(f"header: {total/1024:.0f} KB", flush=True)


if __name__ == "__main__":
    main()

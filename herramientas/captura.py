"""Guarda en PNG lo que está dibujando la placa. Tarda unos 13 s por captura.

Apaga el monitor antes de usarlo: solo un programa puede abrir el puerto.
Es una herramienta de desarrollo y necesita pyserial (pip install pyserial).

    python captura.py salida.png
    python captura.py salida.png --pagina 1
    python captura.py salida.png --datos "{...}" --esperar 8
    python captura.py salida.png --info
"""

import argparse
import struct
import time
import zlib

import serial
from serial.tools import list_ports

BAUDIOS = 115200
# Chips USB-serie habituales en placas ESP32 (CH340, CP210x, FTDI, Espressif)
VIDS_CONOCIDOS = {0x1A86, 0x10C4, 0x0403, 0x303A}


def abrir(puerto):
    # Sin tocar DTR/RTS para no reiniciar la placa al abrir el puerto
    s = serial.Serial()
    s.port = puerto
    s.baudrate = BAUDIOS
    s.timeout = 0.2
    s.dtr = False
    s.rts = False
    s.open()
    return s


def puerto_de_la_placa():
    for p in list_ports.comports():
        if p.vid in VIDS_CONOCIDOS:
            return p.device
    return None


def guardar_png(ruta, ancho, alto, pixeles):
    """Convierte RGB565 (byte alto primero) a PNG sin librerías externas."""
    filas = bytearray()
    for y in range(alto):
        filas.append(0)
        for x in range(ancho):
            i = (y * ancho + x) * 2
            c = (pixeles[i] << 8) | pixeles[i + 1]
            filas += bytes(((c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8))

    def bloque(tipo, datos):
        return struct.pack(">I", len(datos)) + tipo + datos + struct.pack(">I", zlib.crc32(tipo + datos))

    with open(ruta, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(bloque(b"IHDR", struct.pack(">IIBBBBB", ancho, alto, 8, 2, 0, 0, 0)))
        f.write(bloque(b"IDAT", zlib.compress(bytes(filas))))
        f.write(bloque(b"IEND", b""))


def capturar(s, intentos=3):
    for _ in range(intentos):
        s.reset_input_buffer()
        s.write(b"!shot\n")
        fin = time.monotonic() + 5  # al arrancar, la placa ignora órdenes unos segundos
        while time.monotonic() < fin:
            linea = s.readline().decode(errors="ignore").strip()
            if linea.startswith("SHOT "):
                _, ancho, alto = linea.split()
                ancho, alto = int(ancho), int(alto)
                s.timeout = 30
                pixeles = s.read(ancho * alto * 2)
                s.timeout = 0.2
                if len(pixeles) != ancho * alto * 2:
                    raise RuntimeError(f"Captura incompleta: {len(pixeles)} bytes")
                return ancho, alto, pixeles
    raise RuntimeError("La placa no ha respondido a la captura")


def main():
    ap = argparse.ArgumentParser(description="Captura la pantalla de la mascota por USB.")
    ap.add_argument("salida", nargs="?", default="captura.png")
    ap.add_argument("--puerto", help="puerto serie, p. ej. COM3 (por defecto se busca solo)")
    ap.add_argument("--pagina", type=int, help="página a mostrar antes de capturar")
    ap.add_argument("--tap", nargs=2, type=int, metavar=("X", "Y"), help="simular un toque antes de capturar")
    ap.add_argument("--datos", help="paquete JSON de recursos que enviar antes de capturar")
    ap.add_argument("--esperar", type=float, default=0.5, help="segundos de espera antes de capturar")
    ap.add_argument("--info", action="store_true", help="mostrar el estado de la placa")
    args = ap.parse_args()

    puerto = args.puerto or puerto_de_la_placa()
    if puerto is None:
        raise SystemExit("No se encuentra la placa")
    s = abrir(puerto)
    time.sleep(3.5)  # por si se ha reiniciado al abrir el puerto

    if args.pagina is not None:
        s.write(f"!page {args.pagina}\n".encode())
    if args.tap:
        s.write(f"!tap {args.tap[0]} {args.tap[1]}\n".encode())

    fin = time.monotonic() + args.esperar
    while True:
        if args.datos:
            s.write((args.datos + "\n").encode())
        if time.monotonic() >= fin:
            break
        time.sleep(min(1.0, max(fin - time.monotonic(), 0)))

    if args.info:
        s.reset_input_buffer()
        s.write(b"!info\n")
        print(s.readline().decode(errors="ignore").strip())

    ancho, alto, pixeles = capturar(s)
    guardar_png(args.salida, ancho, alto, pixeles)
    print(f"{args.salida}: {ancho}x{alto}")
    s.close()


if __name__ == "__main__":
    main()

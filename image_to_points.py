#!/usr/bin/env python3
"""
Script para convertir imágenes a puntos espaciales
Uso: python3 image_to_points.py <imagen> <salida.csv>
"""

import sys
from PIL import Image
import numpy as np

def image_to_points(image_path, output_csv, threshold=128, max_size=500):
    """
    Convierte una imagen a una nube de puntos

    Args:
        image_path: Ruta a la imagen
        output_csv: Ruta del archivo CSV de salida
        threshold: Umbral de intensidad (0-255)
        max_size: Tamaño máximo de la imagen (píxeles)
    """
    try:
        # Cargar imagen
        img = Image.open(image_path)
        print(f"Imagen cargada: {img.width}x{img.height}, modo: {img.mode}")

        # Convertir a escala de grises
        img_gray = img.convert('L')

        # Redimensionar si es muy grande
        if img_gray.width > max_size or img_gray.height > max_size:
            original_size = (img_gray.width, img_gray.height)
            img_gray.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)
            print(f"Redimensionada de {original_size} a {img_gray.size}")

        # Convertir a numpy array
        pixels = np.array(img_gray)
        height, width = pixels.shape

        # Generar puntos
        points = []

        for y in range(height):
            for x in range(width):
                # Invertir: píxeles oscuros = puntos
                intensity = 255 - pixels[y, x]
                if intensity > threshold:
                    # Normalizar coordenadas a rango 0-100
                    nx = (x / width) * 100.0
                    ny = (y / height) * 100.0
                    points.append((len(points), nx, ny))

        # Guardar a CSV
        with open(output_csv, 'w') as f:
            f.write("id\tx\ty\n")
            for point_id, px, py in points:
                f.write(f"{point_id}\t{px:.6f}\t{py:.6f}\n")

        print(f"✓ {len(points)} puntos generados")
        print(f"✓ Guardado en: {output_csv}")
        return 0

    except FileNotFoundError:
        print(f"ERROR: Archivo no encontrado: {image_path}")
        return 1
    except Exception as e:
        print(f"ERROR: {str(e)}")
        import traceback
        traceback.print_exc()
        return 1

def main():
    if len(sys.argv) < 3:
        print("Uso: python3 image_to_points.py <imagen> <salida.csv> [threshold] [max_size]")
        print("\nEjemplos:")
        print("  python3 image_to_points.py logo.png logo_points.csv")
        print("  python3 image_to_points.py foto.jpg puntos.csv 100 800")
        print("\nParámetros:")
        print("  threshold: Umbral de intensidad (default: 128)")
        print("  max_size: Tamaño máximo en píxeles (default: 500)")
        return 1

    image_path = sys.argv[1]
    output_csv = sys.argv[2]
    threshold = int(sys.argv[3]) if len(sys.argv) > 3 else 128
    max_size = int(sys.argv[4]) if len(sys.argv) > 4 else 500

    print(f"Convirtiendo: {image_path}")
    print(f"Umbral: {threshold}, Tamaño máx: {max_size}")
    print("-" * 50)

    return image_to_points(image_path, output_csv, threshold, max_size)

if __name__ == "__main__":
    sys.exit(main())

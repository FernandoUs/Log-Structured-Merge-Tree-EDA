# Log-Structured-Merge-Tree-EDA

## Descripción

Implementación de varias variaciones del LSM Tree para experimentos de velocidad y precisión en bases de datos espaciales, basado en el paper "Comparison of LSM indexing techniques for storing spatial data".

## Características Implementadas

### Políticas de Merge

- **Tiered** (Stack-based): Fusiona todos los componentes en un nivel
- **Leveled**: Fusión selectiva basada en tamaño con selección de componentes superpuestos

### Estrategias de Particionamiento

- **Size**: Particiona por tamaño fijo usando ordenamiento Simple o Hilbert
- **STR (Sort-Tile-Recursive)**: Particionamiento espacial recursivo en 2D con MBRs no superpuestos
- **R\*-groove**: K-means clustering en datos muestreados para MBRs óptimos

### Comparadores Espaciales

- **Simple**: Ordenamiento lexicográfico (X, Y, ...)
- **Hilbert**: Curva de Hilbert para mejor localidad espacial

## Compilación

### Windows (MinGW/Ninja)

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux/macOS

```bash
mkdir build
cd build
cmake ..
make
```

El ejecutable `lsm_spatial_db` quedará en la carpeta `build/`.

## Uso

### Ejecutar la aplicación

```powershell
cd build
.\lsm_spatial_db.exe
```

### Sintaxis SQL Extendida

```sql
-- Crear tabla con política y particionamiento
CREATE TABLE <nombre> (<columnas>)
  [WITH POLICY <Tiered|Leveled> <parámetro>]
  [COMPARATOR <Simple|Hilbert>]
  [PARTITION <none|Size|STR|RStarGroove>]

-- Ejemplos:
CREATE TABLE points (id INT, location POINT, value DOUBLE)
  WITH POLICY Leveled 10 COMPARATOR Hilbert PARTITION STR

CREATE TABLE data (id INT, location POINT, value DOUBLE)
  WITH POLICY Tiered 4 COMPARATOR Simple PARTITION Size

-- Insertar datos
INSERT INTO points VALUES (0.5, 0.5, 100)

-- Consultas espaciales
SELECT COUNT(*) FROM points WHERE spatial_intersect(location, 0, 0, 1, 1)

-- Benchmark
benchmark points

-- Métricas
metrics

-- Limpiar tabla
clean points
```

## Arquitectura

### Componentes Principales

```
include/
├── lsm/
│   ├── LSMTree.h              # Árbol LSM principal con MemTable y componentes en disco
│   ├── LSMComponent.h         # Componente en disco con R-tree
│   ├── MergePolicy.h          # Políticas de merge (Tiered, Leveled)
│   └── PartitioningStrategy.h # Estrategias de particionamiento (Size, STR, R*-groove)
├── spatial/
│   ├── Point.h                # Punto N-dimensional
│   ├── MBR.h                  # Minimum Bounding Rectangle
│   ├── RTree.h                # R-tree para indexación espacial
│   └── SpatialComparators.h  # Comparadores Simple y Hilbert
├── sql/
│   └── QueryExecutor.h        # Ejecutor de consultas SQL
└── cli/
    └── CLI.h                  # Interfaz de línea de comandos
```

### Flujo de Datos

```
INSERT → MemTable (ordenado) → flush() → LSMComponent (R-tree) → disco
                                           ↓
                                    checkAndMerge()
                                           ↓
                        Política de Merge selecciona víctimas
                                           ↓
                        mergeComponentsPartitioned() [si hay particionamiento]
                                           ↓
                        Estrategia particiona registros fusionados
                                           ↓
                        Múltiples LSMComponents nuevos → disco
```

### Query Path

```
SELECT → memTable.rangeSearch(MBR)
      → for each diskComponent:
          - MBR intersection test
          - if intersects: load R-tree, rangeSearch()
          - merge results
      → deduplicación y filtrado de tombstones
      → resultados
```

## Detalles de Implementación

### Leveled Merge Policy

- **Trigger**: `sum(tamaños_nivel) > baseSize * (sizeRatio^nivel)`
- **Selección**: Todos los componentes del nivel desbordado + componentes superpuestos (MBR intersection) del siguiente nivel
- **Target Level**: nivel + 1

### Size Partitioning

- Ordena registros por comparador (Simple o Hilbert)
- Divide en chunks de `maxComponentSize` (10,000 registros)
- Simple pero efectivo

### STR Partitioning

- **Algoritmo**: Sort-Tile-Recursive para datos 2D
- **Pasos**:
  1. Ordenar por X, particionar en √(N/S) rodajas
  2. Ordenar cada rodaja por Y
  3. Particionar rodajas en tiles de tamaño S
- **Resultado**: MBRs rectangulares no superpuestos

### R\*-groove Partitioning

- **Algoritmo**: K-means clustering en muestra de datos
- **Pasos**:
  1. Muestrear 10% de registros
  2. Ejecutar k-means (10 iteraciones)
  3. Computar límites MBR de clusters
  4. Asignar todos los registros al límite más cercano (min expansión)
- **Resultado**: Superposición mínima, se adapta a distribución de datos

## Métricas

- **Write Amplification (WA)**: Bytes escritos en disco / Bytes de usuario
- **Read Amplification (RA)**: Componentes verificados por consulta
- **Query Latency**: Latencia promedio de consultas en ms
- **Total Merges**: Número de operaciones de merge ejecutadas

## Pruebas

Ver `TEST_LEVELED_PARTITIONING.md` para casos de prueba detallados y guías de verificación.

### Test Rápido

```sql
CREATE TABLE test (id INT, location POINT, value DOUBLE)
  WITH POLICY Leveled 10 COMPARATOR Hilbert PARTITION STR

benchmark test

metrics

SELECT COUNT(*) FROM test WHERE spatial_intersect(location, 0, 0, 1, 1)
```

## Limpieza

```sql
clean <tabla>  -- Resetear datos y métricas de tabla
clear          -- Limpiar solo métricas
tables         -- Listar todas las tablas
exit           -- Salir de la aplicación
```

## Referencias

- Mao, Q. et al. (2023). "Comparison of LSM indexing techniques for storing spatial data". Journal of Big Data.
- O'Neil, P. et al. (1996). "The Log-Structured Merge-Tree (LSM-Tree)". Acta Informatica.

## Autores

Implementación basada en investigación sobre técnicas de indexación LSM para datos espaciales.

# Log-Structured-Merge-Tree-EDA

Sistema de base de datos espacial basado en LSM-Tree con índices R-Tree, implementado en C++.

## 🚀 Inicio Rápido

```zsh
./setup_imgui.sh
mkdir -p build && cd build && cmake .. && make
./lsm_spatial_db_gui  # GUI
./lsm_spatial_db      # CLI
```

## 📋 Requisitos

**macOS:**
```zsh
brew install glfw
```

**Linux (Ubuntu/Debian):**
```zsh
sudo apt-get install libglfw3-dev
```

## 🎨 Interfaz Gráfica

### Características
- Visualización espacial 2D interactiva
- Consultas mediante selección con mouse
- Métricas en tiempo real (Write/Read Amplification)
- Benchmark integrado
- Panel SQL completo

### Controles
- **Click Izq + Drag**: Consulta espacial
- **Click Der + Drag**: Pan
- **Scroll**: Zoom

## ⚙️ Políticas de Merge

- **Binomial**: Merge basado en binomios
- **Tiered**: Agrupa componentes por nivel
- **Concurrent**: Merge concurrente con parámetros C, D, λ
- **Leveled**: Un componente por nivel (WIP)

## 🧭 Comparadores Espaciales

- **Simple**: Orden lexicográfico
- **Hilbert**: Curva de Hilbert para mejor localidad espacial

## 🔧 Estrategias de Particionamiento

- **SizePartitioning**: Particionamiento por tamaño
- **STRPartitioning**: Sort-Tile-Recursive
- **RStarGrovePartitioning**: Basado en R*-Grove

## 📝 Comandos SQL

```sql
-- Crear tabla
CREATE TABLE test (id INT, location POINT, value DOUBLE)
  with policy Tiered 4 COMPARATOR Hilbert

-- Insertar
INSERT INTO test VALUES (10.5, 20.3, 100)

-- Consultar
SELECT COUNT(*) FROM test
  WHERE spatial_intersect(location, 0, 0, 50, 50)
```

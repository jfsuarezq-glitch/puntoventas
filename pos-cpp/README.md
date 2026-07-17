# POS Quiosco — versión C++ (ejecutable .exe)

Versión de escritorio en **C++** del punto de venta `pos-quiosco`. Sin
dependencias externas, se compila a un `.exe` para Windows y funciona sin
navegador ni internet. Hay dos variantes que **comparten el mismo archivo de
datos** `pos_data.txt`:

- **`gui_win32.cpp` → `PosQuioscoGUI.exe`**: interfaz **gráfica** moderna de
  Windows (cabecera de color, pestañas tipo *pill*, botones planos y listas).
  **Recomendada.**
- **`main.cpp` → `PosQuiosco.exe`**: versión de **consola** (texto).

### Módulos de la versión gráfica

- **Ventas**: buscar/escanear, doble clic para agregar al carrito, cliente,
  fiado, vuelto y cobro (botón o **F2**).
- **Inventario**: alta/edición/borrado e ingreso de mercadería (clave admin).
- **Clientes**: cuenta corriente y cobranzas.
- **Proveedores**: alta/edición/borrado, registro de **compras** (aumenta stock,
  a crédito o pagado) y **pagos** a proveedor, con historial de compras.
- **Canchas**: alta/edición/borrado de canchas y gestión de **reservas /
  alquileres** (con adelanto que entra automáticamente a caja).
- **Caja**: ingresos, gastos, retiros, saldo y cierre.

## Módulos incluidos

- **Ventas**: agregar productos por código de barras, por nombre o por ID;
  carrito, cobro al contado con cálculo de **vuelto**, y venta a **cuenta
  corriente (fiado)** asociada a un cliente.
- **Inventario** (requiere administrador): alta, edición, eliminación y
  ajuste de stock (ingreso de mercadería).
- **Clientes**: alta, listado con saldo de deuda y registro de cobranzas.
- **Caja**: ingresos, gastos, retiros, saldo y cierre de caja.
- **Reportes**: historial de ventas, totales, ventas del día y productos más
  vendidos.
- **Persistencia**: todos los datos se guardan automáticamente en
  `pos_data.txt` (se crea junto al ejecutable).

Clave de administrador por defecto: **1234** (constante `ADMIN_PASS` en `main.cpp`).

## Compilar a `.exe` en Windows

Necesitas un compilador de C++ (por ejemplo **MinGW-w64** o **MSYS2**, que
incluyen `g++`; o **Visual Studio** con `cl`).

### Lo más fácil: ejecuta el script incluido

```bat
build.bat
```

Compila ambas versiones (`PosQuioscoGUI.exe` y `PosQuiosco.exe`).

### Con g++ (MinGW / MSYS2), manualmente

Versión **gráfica** (recomendada):

```bat
g++ -std=c++17 -O2 -municode -mwindows -static -o PosQuioscoGUI.exe gui_win32.cpp -lcomctl32 -lgdi32 -luser32
```

Versión **de consola**:

```bat
g++ -std=c++17 -O2 -static -o PosQuiosco.exe main.cpp
```

El flag `-static` incrusta las librerías para que el `.exe` funcione en
cualquier PC con Windows sin instalar nada más.

### Con Visual Studio (MSVC)

Abre el "Developer Command Prompt" y ejecuta:

```bat
cl /std:c++17 /EHsc /O2 gui_win32.cpp /Fe:PosQuioscoGUI.exe /link comctl32.lib gdi32.lib user32.lib
cl /std:c++17 /EHsc /O2 main.cpp /Fe:PosQuiosco.exe
```

## Compilar y probar en Linux / macOS

```bash
g++ -std=c++17 -O2 -o PosQuiosco main.cpp
./PosQuiosco
```

## Uso rápido — versión gráfica (`PosQuioscoGUI.exe`)

1. Ejecuta `PosQuioscoGUI.exe`. La primera vez se cargan productos de ejemplo.
2. Pestaña **Ventas**: escribe/escanea el código o nombre en la caja de búsqueda
   y pulsa ENTER (o botón *Agregar al carrito*); también puedes hacer **doble
   clic** en un producto de la lista para agregarlo.
3. Elige el **cliente** (o *Mostrador* para contado), marca *Fiado* si es a
   cuenta corriente, escribe el efectivo (calcula el **vuelto**) y pulsa
   **COBRAR** (o la tecla **F2**).
4. Pestañas **Inventario / Clientes / Caja** para gestionar stock, cuentas
   corrientes y movimientos de caja. Inventario pide la clave de administrador
   (`1234`) — botón *Ingresar como admin* arriba a la derecha.

## Uso rápido — versión de consola (`PosQuiosco.exe`)

1. En **Ventas**, escribe el código de barras o el nombre y pulsa ENTER. Usa
   `#3` para agregar por ID y `-1` para quitar la primera línea.
2. Escribe `C` para cobrar: elige cliente (o `0` = mostrador), indica si es
   fiado y el efectivo recibido; el sistema calcula el vuelto.
3. Para **Inventario** ingresa como administrador (opción 6 del menú) con la
   clave `1234`.

## Notas

- El archivo de datos `pos_data.txt` es un texto plano; puedes respaldarlo
  copiándolo a un USB o carpeta externa (equivale a la copia de seguridad de
  la versión web).
- Esta versión de consola cubre los módulos centrales del POS. Los módulos de
  proveedores, canchas/reservas y exportación a Excel de la versión web no
  están portados aquí.

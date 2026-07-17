# POS Quiosco — versión C++ (ejecutable .exe)

Versión de escritorio en **C++ (consola)** del punto de venta `pos-quiosco`.
Es un solo archivo (`main.cpp`), sin dependencias externas, que se compila a un
`.exe` para Windows y funciona sin navegador ni internet.

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

### Con g++ (MinGW / MSYS2)

```bat
g++ -std=c++17 -O2 -static -o PosQuiosco.exe main.cpp
```

El flag `-static` incrusta las librerías para que el `.exe` funcione en
cualquier PC con Windows sin instalar nada más.

O simplemente ejecuta el script incluido:

```bat
build.bat
```

### Con Visual Studio (MSVC)

Abre el "Developer Command Prompt" y ejecuta:

```bat
cl /std:c++17 /EHsc /O2 main.cpp /Fe:PosQuiosco.exe
```

## Compilar y probar en Linux / macOS

```bash
g++ -std=c++17 -O2 -o PosQuiosco main.cpp
./PosQuiosco
```

## Uso rápido

1. Ejecuta `PosQuiosco.exe`. La primera vez se cargan productos de ejemplo.
2. En **Ventas**, escanea o escribe el código de barras (o el nombre) y pulsa
   ENTER para agregarlo al carrito. Usa `#3` para agregar por ID y `-1` para
   quitar la primera línea.
3. Escribe `C` para cobrar: elige cliente (o `0` = mostrador), indica si es
   fiado y el efectivo recibido; el sistema calcula el vuelto.
4. Para **Inventario** ingresa como administrador (opción 6 del menú o al
   entrar a Inventario) con la clave `1234`.

## Notas

- El archivo de datos `pos_data.txt` es un texto plano; puedes respaldarlo
  copiándolo a un USB o carpeta externa (equivale a la copia de seguridad de
  la versión web).
- Esta versión de consola cubre los módulos centrales del POS. Los módulos de
  proveedores, canchas/reservas y exportación a Excel de la versión web no
  están portados aquí.

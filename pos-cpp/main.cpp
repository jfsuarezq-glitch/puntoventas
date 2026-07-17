// ============================================================================
//  POS Quiosco - Punto de Venta en C++ (aplicacion de consola)
//  Version C++ del sistema web pos-quiosco.
//
//  Modulos incluidos:
//    - Ventas por codigo de barras / busqueda, carrito y cobro con vuelto
//    - Venta al contado (efectivo) o a cuenta corriente (fiado) de un cliente
//    - Inventario: alta, edicion, eliminacion y control de stock
//    - Clientes: alta, edicion, saldo (cuenta corriente) y cobranzas
//    - Caja: ingresos, gastos, retiros, saldo y cierre de caja
//    - Reportes: historial de ventas y resumen del dia
//    - Persistencia automatica en el archivo pos_data.txt
//
//  Compilar a .exe (Windows):
//    g++ -std=c++17 -O2 -static -o PosQuiosco.exe main.cpp
//
//  Compilar en Linux/Mac (para probar):
//    g++ -std=c++17 -O2 -o PosQuiosco main.cpp
//
//  Clave de administrador por defecto: 1234
// ============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <cstdint>

#ifdef _WIN32
    #include <windows.h>
#endif

using namespace std;

// ----------------------------------------------------------------------------
//  Configuracion
// ----------------------------------------------------------------------------
static const string DATA_FILE     = "pos_data.txt";
static const string MONEDA        = "S/";
static const string ADMIN_PASS    = "1234";

// ----------------------------------------------------------------------------
//  Modelos de datos
// ----------------------------------------------------------------------------
struct Producto {
    int    id;
    string nombre;
    double precio;
    int    stock;
    string codigo;      // codigo de barras
    string categoria;
};

struct Cliente {
    int    id;
    string nombre;
    string telefono;
    double saldo;       // deuda pendiente (cuenta corriente)
};

struct ItemCarrito {
    int    id;          // id del producto
    string nombre;
    double precio;
    int    cantidad;
    double descuento;
};

struct ItemVenta {
    string nombre;
    double precio;
    int    cantidad;
    double descuento;
};

struct Venta {
    long long        idVenta;
    vector<ItemVenta> items;
    double            total;
    string            formaPago;   // "efectivo" | "cuenta corriente"
    int               clienteId;   // 0 = sin cliente
    string            clienteNombre;
    string            fecha;       // AAAA-MM-DD HH:MM:SS
};

struct MovCaja {
    long long id;
    string    tipo;     // "ingreso" | "gasto" | "retiro"
    double    monto;
    string    desc;
    string    fecha;
};

// ----------------------------------------------------------------------------
//  Estado global
// ----------------------------------------------------------------------------
vector<Producto> productos;
vector<Cliente>  clientes;
vector<Venta>    ventas;
vector<MovCaja>  movsCaja;

int  nextProductoId = 1;
int  nextClienteId  = 1;
double cajaSaldoInicial = 0.0;   // saldo de apertura de caja
bool esAdmin = false;

// ----------------------------------------------------------------------------
//  Utilidades
// ----------------------------------------------------------------------------
void guardarDatos();   // declaracion adelantada (definida mas abajo)

string fechaHoraActual() {
    time_t t = time(nullptr);
    tm* lt = localtime(&t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
    return string(buf);
}

string fechaHoy() {
    time_t t = time(nullptr);
    tm* lt = localtime(&t);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", lt);
    return string(buf);
}

string dinero(double v) {
    ostringstream os;
    os << MONEDA << " " << fixed << setprecision(2) << v;
    return os.str();
}

// Escapa el caracter '|' y saltos de linea para el guardado en archivo.
string escapar(const string& s) {
    string r;
    for (char c : s) {
        if (c == '|') r += "\\p";
        else if (c == '\n') r += "\\n";
        else if (c == '\\') r += "\\\\";
        else r += c;
    }
    return r;
}

string desescapar(const string& s) {
    string r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            if (n == 'p') { r += '|'; ++i; }
            else if (n == 'n') { r += '\n'; ++i; }
            else if (n == '\\') { r += '\\'; ++i; }
            else r += s[i];
        } else {
            r += s[i];
        }
    }
    return r;
}

vector<string> dividir(const string& linea, char sep) {
    vector<string> partes;
    string actual;
    for (char c : linea) {
        if (c == sep) { partes.push_back(actual); actual.clear(); }
        else actual += c;
    }
    partes.push_back(actual);
    return partes;
}

string aMinusculas(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
    return s;
}

// Limpia el buffer de entrada tras leer con >>
void limpiarEntrada() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

string leerLinea(const string& prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) {
        // Entrada agotada (EOF): salimos limpiamente para no entrar en bucle.
        guardarDatos();
        cout << "\n>> Entrada finalizada. Datos guardados. Adios.\n";
        exit(0);
    }
    return s;
}

double leerDouble(const string& prompt, double porDefecto = 0.0) {
    string s = leerLinea(prompt);
    if (s.empty()) return porDefecto;
    try { return stod(s); } catch (...) { return porDefecto; }
}

int leerInt(const string& prompt, int porDefecto = 0) {
    string s = leerLinea(prompt);
    if (s.empty()) return porDefecto;
    try { return stoi(s); } catch (...) { return porDefecto; }
}

void pausar() {
    leerLinea("\nPresiona ENTER para continuar...");
}

void limpiarPantalla() {
#ifdef _WIN32
    system("cls");
#else
    // secuencia ANSI
    cout << "\033[2J\033[1;1H";
#endif
}

// ----------------------------------------------------------------------------
//  Persistencia (guardar / cargar)
// ----------------------------------------------------------------------------
void guardarDatos() {
    ofstream f(DATA_FILE);
    if (!f) return;

    f << "META|" << nextProductoId << "|" << nextClienteId << "|"
      << fixed << setprecision(2) << cajaSaldoInicial << "\n";

    for (const auto& p : productos) {
        f << "PROD|" << p.id << "|" << escapar(p.nombre) << "|"
          << fixed << setprecision(2) << p.precio << "|"
          << p.stock << "|" << escapar(p.codigo) << "|" << escapar(p.categoria) << "\n";
    }
    for (const auto& c : clientes) {
        f << "CLI|" << c.id << "|" << escapar(c.nombre) << "|"
          << escapar(c.telefono) << "|" << fixed << setprecision(2) << c.saldo << "\n";
    }
    for (const auto& v : ventas) {
        f << "VENTA|" << v.idVenta << "|" << fixed << setprecision(2) << v.total << "|"
          << escapar(v.formaPago) << "|" << v.clienteId << "|"
          << escapar(v.clienteNombre) << "|" << escapar(v.fecha) << "|"
          << v.items.size() << "\n";
        for (const auto& it : v.items) {
            f << "VITEM|" << escapar(it.nombre) << "|"
              << fixed << setprecision(2) << it.precio << "|"
              << it.cantidad << "|" << fixed << setprecision(2) << it.descuento << "\n";
        }
    }
    for (const auto& m : movsCaja) {
        f << "CAJA|" << m.id << "|" << escapar(m.tipo) << "|"
          << fixed << setprecision(2) << m.monto << "|"
          << escapar(m.desc) << "|" << escapar(m.fecha) << "\n";
    }
}

void cargarDatos() {
    ifstream f(DATA_FILE);
    if (!f) return;

    productos.clear();
    clientes.clear();
    ventas.clear();
    movsCaja.clear();

    string linea;
    Venta* ventaActual = nullptr;
    while (getline(f, linea)) {
        if (linea.empty()) continue;
        vector<string> c = dividir(linea, '|');
        const string& tipo = c[0];

        if (tipo == "META" && c.size() >= 4) {
            nextProductoId   = stoi(c[1]);
            nextClienteId    = stoi(c[2]);
            cajaSaldoInicial = stod(c[3]);
        } else if (tipo == "PROD" && c.size() >= 7) {
            Producto p;
            p.id        = stoi(c[1]);
            p.nombre    = desescapar(c[2]);
            p.precio    = stod(c[3]);
            p.stock     = stoi(c[4]);
            p.codigo    = desescapar(c[5]);
            p.categoria = desescapar(c[6]);
            productos.push_back(p);
        } else if (tipo == "CLI" && c.size() >= 5) {
            Cliente cl;
            cl.id       = stoi(c[1]);
            cl.nombre   = desescapar(c[2]);
            cl.telefono = desescapar(c[3]);
            cl.saldo    = stod(c[4]);
            clientes.push_back(cl);
        } else if (tipo == "VENTA" && c.size() >= 8) {
            Venta v;
            v.idVenta       = stoll(c[1]);
            v.total         = stod(c[2]);
            v.formaPago     = desescapar(c[3]);
            v.clienteId     = stoi(c[4]);
            v.clienteNombre = desescapar(c[5]);
            v.fecha         = desescapar(c[6]);
            ventas.push_back(v);
            ventaActual = &ventas.back();
        } else if (tipo == "VITEM" && c.size() >= 5 && ventaActual) {
            ItemVenta it;
            it.nombre    = desescapar(c[1]);
            it.precio    = stod(c[2]);
            it.cantidad  = stoi(c[3]);
            it.descuento = stod(c[4]);
            ventaActual->items.push_back(it);
        } else if (tipo == "CAJA" && c.size() >= 6) {
            MovCaja m;
            m.id    = stoll(c[1]);
            m.tipo  = desescapar(c[2]);
            m.monto = stod(c[3]);
            m.desc  = desescapar(c[4]);
            m.fecha = desescapar(c[5]);
            movsCaja.push_back(m);
        }
    }
}

// ----------------------------------------------------------------------------
//  Datos de ejemplo (solo la primera vez)
// ----------------------------------------------------------------------------
void cargarDatosEjemplo() {
    productos = {
        {1, "Agua San Luis 625ml",   1.50, 48, "7751010001234", "Bebidas"},
        {2, "Coca Cola 500ml",       3.00, 24, "7501055300006", "Bebidas"},
        {3, "Galletas Oreo",         2.50, 36, "7622210003232", "Galletas"},
        {4, "Chicles Halls",         1.00, 60, "0040000004096", "Snack"},
        {5, "Papas Lays clasicas",   3.50, 18, "7501012004040", "Snack"},
        {6, "Yogurt Gloria 120g",    2.00, 20, "7750016000789", "Bebidas"},
        {7, "Jugo Pulp naranja",     2.50, 15, "7750016100456", "Bebidas"},
        {8, "Pilas AA Duracell x2",  5.00, 30, "0041333045559", "Otros"},
    };
    nextProductoId = 9;
    nextClienteId  = 1;
}

// ----------------------------------------------------------------------------
//  Busquedas auxiliares
// ----------------------------------------------------------------------------
Producto* buscarProductoPorId(int id) {
    for (auto& p : productos) if (p.id == id) return &p;
    return nullptr;
}

Producto* buscarProductoPorCodigo(const string& codigo) {
    for (auto& p : productos) if (!p.codigo.empty() && p.codigo == codigo) return &p;
    return nullptr;
}

Cliente* buscarClientePorId(int id) {
    for (auto& c : clientes) if (c.id == id) return &c;
    return nullptr;
}

// ----------------------------------------------------------------------------
//  Encabezado
// ----------------------------------------------------------------------------
void encabezado(const string& titulo) {
    limpiarPantalla();
    cout << "============================================================\n";
    cout << "  POS QUIOSCO";
    cout << "   [" << (esAdmin ? "ADMINISTRADOR" : "PERSONAL") << "]";
    cout << "\n  " << titulo << "\n";
    cout << "============================================================\n\n";
}

// ----------------------------------------------------------------------------
//  Modulo: Login / Administrador
// ----------------------------------------------------------------------------
bool pedirAdmin() {
    if (esAdmin) return true;
    string pass = leerLinea("Clave de administrador: ");
    if (pass == ADMIN_PASS) {
        esAdmin = true;
        cout << "\n>> Sesion de administrador iniciada.\n";
        return true;
    }
    cout << "\n>> Clave incorrecta.\n";
    return false;
}

// ----------------------------------------------------------------------------
//  Modulo: Ventas
// ----------------------------------------------------------------------------
double subtotalItem(const ItemCarrito& it) {
    double s = it.precio * it.cantidad - it.descuento;
    return s < 0 ? 0 : s;
}

double totalCarrito(const vector<ItemCarrito>& carrito) {
    double t = 0;
    for (const auto& it : carrito) t += subtotalItem(it);
    return t;
}

void mostrarCarrito(const vector<ItemCarrito>& carrito) {
    cout << "\n---------------------------- CARRITO ----------------------------\n";
    if (carrito.empty()) {
        cout << "  (vacio)  Escanea o escribe un codigo/nombre para agregar.\n";
    } else {
        cout << left << setw(4) << "#" << setw(28) << "Producto"
             << right << setw(6) << "Cant" << setw(11) << "P.Unit" << setw(12) << "Subtotal" << "\n";
        int i = 1;
        for (const auto& it : carrito) {
            cout << left << setw(4) << i++
                 << setw(28) << (it.nombre.size() > 27 ? it.nombre.substr(0, 27) : it.nombre)
                 << right << setw(6) << it.cantidad
                 << setw(11) << fixed << setprecision(2) << it.precio
                 << setw(12) << fixed << setprecision(2) << subtotalItem(it) << "\n";
        }
        cout << "-----------------------------------------------------------------\n";
        cout << right << setw(51) << "TOTAL: " << setw(12) << dinero(totalCarrito(carrito)) << "\n";
    }
    cout << "-----------------------------------------------------------------\n";
}

void agregarAlCarrito(vector<ItemCarrito>& carrito, Producto* prod) {
    if (prod->stock <= 0) {
        cout << ">> Sin stock: " << prod->nombre << "\n";
        return;
    }
    for (auto& it : carrito) {
        if (it.id == prod->id) {
            if (it.cantidad >= prod->stock) {
                cout << ">> Stock maximo alcanzado para " << prod->nombre << "\n";
                return;
            }
            it.cantidad++;
            cout << ">> +1 " << prod->nombre << " (cant: " << it.cantidad << ")\n";
            return;
        }
    }
    carrito.push_back({prod->id, prod->nombre, prod->precio, 1, 0.0});
    cout << ">> Agregado: " << prod->nombre << "\n";
}

void listarProductosBreve(const string& filtro = "") {
    string q = aMinusculas(filtro);
    cout << "\nProductos disponibles:\n";
    cout << left << setw(6) << "ID" << setw(30) << "Nombre"
         << right << setw(10) << "Precio" << setw(8) << "Stock" << "  " << left << "Codigo\n";
    for (const auto& p : productos) {
        if (!q.empty()) {
            string nm = aMinusculas(p.nombre);
            if (nm.find(q) == string::npos && p.codigo.find(filtro) == string::npos) continue;
        }
        cout << left << setw(6) << p.id
             << setw(30) << (p.nombre.size() > 29 ? p.nombre.substr(0, 29) : p.nombre)
             << right << setw(10) << fixed << setprecision(2) << p.precio
             << setw(8) << p.stock << "  " << left << p.codigo << "\n";
    }
}

void cobrar(vector<ItemCarrito>& carrito) {
    if (carrito.empty()) { cout << ">> El carrito esta vacio.\n"; pausar(); return; }
    double total = totalCarrito(carrito);

    encabezado("Cobrar venta");
    mostrarCarrito(carrito);
    cout << "\nTotal a pagar: " << dinero(total) << "\n\n";

    cout << "Cliente:\n";
    cout << "  0) Sin cliente / Mostrador (contado)\n";
    for (const auto& c : clientes) {
        cout << "  " << c.id << ") " << c.nombre;
        if (c.saldo > 0) cout << "  (debe " << dinero(c.saldo) << ")";
        cout << "\n";
    }
    int clienteId = leerInt("Selecciona cliente (0 = mostrador): ", 0);
    Cliente* cliente = clienteId > 0 ? buscarClientePorId(clienteId) : nullptr;

    bool fiado = false;
    if (cliente) {
        string r = aMinusculas(leerLinea("Vender a cuenta corriente (fiado)? (s/n): "));
        fiado = (r == "s" || r == "si");
    }

    if (!fiado) {
        double efectivo = leerDouble("Efectivo recibido " + MONEDA + " (ENTER = exacto): ", total);
        if (efectivo < total) {
            cout << "\n>> Efectivo insuficiente. Venta cancelada.\n";
            pausar();
            return;
        }
        cout << "\n>> VUELTO: " << dinero(efectivo - total) << "\n";
    }

    // Descontar stock y armar la venta
    Venta v;
    v.idVenta = (long long)time(nullptr);
    v.total = total;
    v.formaPago = fiado ? "cuenta corriente" : "efectivo";
    v.clienteId = cliente ? cliente->id : 0;
    v.clienteNombre = cliente ? cliente->nombre : "";
    v.fecha = fechaHoraActual();

    for (const auto& it : carrito) {
        Producto* p = buscarProductoPorId(it.id);
        if (p) p->stock = max(0, p->stock - it.cantidad);
        v.items.push_back({it.nombre, it.precio, it.cantidad, it.descuento});
    }
    ventas.push_back(v);

    if (fiado && cliente) {
        cliente->saldo += total;
        cout << "\n>> Venta a cuenta corriente registrada. Nuevo saldo de "
             << cliente->nombre << ": " << dinero(cliente->saldo) << "\n";
    } else {
        movsCaja.push_back({(long long)time(nullptr), "ingreso", total, "Venta", fechaHoraActual()});
        cout << "\n>> Venta al contado registrada: " << dinero(total) << "\n";
    }

    carrito.clear();
    guardarDatos();
    pausar();
}

void moduloVentas() {
    vector<ItemCarrito> carrito;
    while (true) {
        encabezado("VENTAS");
        listarProductosBreve();
        mostrarCarrito(carrito);
        cout << "\nOpciones:\n";
        cout << "  [codigo/nombre]  Agregar producto al carrito\n";
        cout << "  #<id>            Agregar por ID de producto (ej: #3)\n";
        cout << "  -<n>             Quitar la linea n del carrito (ej: -1)\n";
        cout << "  C  Cobrar   |   L  Limpiar carrito   |   Q  Volver al menu\n";
        string entrada = leerLinea("\n> ");
        if (entrada.empty()) continue;

        string low = aMinusculas(entrada);
        if (low == "q") { break; }
        if (low == "l") { carrito.clear(); continue; }
        if (low == "c") { cobrar(carrito); continue; }

        if (entrada[0] == '-') {
            int n = 0;
            try { n = stoi(entrada.substr(1)); } catch (...) { continue; }
            if (n >= 1 && n <= (int)carrito.size()) carrito.erase(carrito.begin() + (n - 1));
            continue;
        }

        Producto* prod = nullptr;
        if (entrada[0] == '#') {
            int id = 0;
            try { id = stoi(entrada.substr(1)); } catch (...) {}
            prod = buscarProductoPorId(id);
        } else {
            // primero por codigo de barras exacto
            prod = buscarProductoPorCodigo(entrada);
            if (!prod) {
                // luego por nombre exacto
                string q = aMinusculas(entrada);
                for (auto& p : productos) if (aMinusculas(p.nombre) == q) { prod = &p; break; }
            }
            if (!prod) {
                // coincidencias parciales por nombre
                vector<Producto*> matches;
                string q = aMinusculas(entrada);
                for (auto& p : productos) if (aMinusculas(p.nombre).find(q) != string::npos) matches.push_back(&p);
                if (matches.size() == 1) prod = matches[0];
                else if (matches.size() > 1) {
                    cout << "\nVarias coincidencias, elige por ID:\n";
                    for (auto* m : matches)
                        cout << "  #" << m->id << " " << m->nombre << " (" << dinero(m->precio) << ")\n";
                    pausar();
                    continue;
                }
            }
        }

        if (prod) { agregarAlCarrito(carrito, prod); }
        else { cout << ">> No se encontro el producto: " << entrada << "\n"; pausar(); }
    }
}

// ----------------------------------------------------------------------------
//  Modulo: Inventario
// ----------------------------------------------------------------------------
void listarInventario() {
    cout << left << setw(6) << "ID" << setw(30) << "Nombre"
         << right << setw(10) << "Precio" << setw(8) << "Stock"
         << "  " << left << setw(16) << "Codigo" << "Estado\n";
    cout << "-----------------------------------------------------------------------------\n";
    for (const auto& p : productos) {
        string estado = p.stock <= 0 ? "SIN STOCK" : (p.stock <= 5 ? "BAJO" : "OK");
        cout << left << setw(6) << p.id
             << setw(30) << (p.nombre.size() > 29 ? p.nombre.substr(0, 29) : p.nombre)
             << right << setw(10) << fixed << setprecision(2) << p.precio
             << setw(8) << p.stock
             << "  " << left << setw(16) << p.codigo << estado << "\n";
    }
}

void agregarProducto() {
    encabezado("Inventario - Nuevo producto");
    Producto p;
    p.id     = nextProductoId;
    p.nombre = leerLinea("Nombre: ");
    if (p.nombre.empty()) { cout << ">> Nombre requerido.\n"; pausar(); return; }
    p.precio    = leerDouble("Precio " + MONEDA + ": ");
    p.stock     = leerInt("Stock inicial: ");
    p.codigo    = leerLinea("Codigo de barras: ");
    p.categoria = leerLinea("Categoria (opcional): ");
    nextProductoId++;
    productos.push_back(p);
    guardarDatos();
    cout << "\n>> Producto agregado (ID " << p.id << ").\n";
    pausar();
}

void editarProducto() {
    int id = leerInt("ID del producto a editar: ");
    Producto* p = buscarProductoPorId(id);
    if (!p) { cout << ">> No existe ese producto.\n"; pausar(); return; }
    cout << "\nDeja en blanco para conservar el valor actual.\n";
    string nombre = leerLinea("Nombre [" + p->nombre + "]: ");
    if (!nombre.empty()) p->nombre = nombre;
    string precio = leerLinea("Precio [" + to_string(p->precio) + "]: ");
    if (!precio.empty()) { try { p->precio = stod(precio); } catch (...) {} }
    string stock = leerLinea("Stock [" + to_string(p->stock) + "]: ");
    if (!stock.empty()) { try { p->stock = stoi(stock); } catch (...) {} }
    string codigo = leerLinea("Codigo [" + p->codigo + "]: ");
    if (!codigo.empty()) p->codigo = codigo;
    string cat = leerLinea("Categoria [" + p->categoria + "]: ");
    if (!cat.empty()) p->categoria = cat;
    guardarDatos();
    cout << "\n>> Producto actualizado.\n";
    pausar();
}

void eliminarProducto() {
    int id = leerInt("ID del producto a eliminar: ");
    Producto* p = buscarProductoPorId(id);
    if (!p) { cout << ">> No existe ese producto.\n"; pausar(); return; }
    string r = aMinusculas(leerLinea("Eliminar \"" + p->nombre + "\"? (s/n): "));
    if (r == "s" || r == "si") {
        productos.erase(remove_if(productos.begin(), productos.end(),
            [id](const Producto& x){ return x.id == id; }), productos.end());
        guardarDatos();
        cout << "\n>> Producto eliminado.\n";
    }
    pausar();
}

void ajustarStock() {
    int id = leerInt("ID del producto: ");
    Producto* p = buscarProductoPorId(id);
    if (!p) { cout << ">> No existe ese producto.\n"; pausar(); return; }
    cout << "Stock actual de " << p->nombre << ": " << p->stock << "\n";
    int delta = leerInt("Cantidad a sumar (usa negativo para restar): ");
    p->stock = max(0, p->stock + delta);
    guardarDatos();
    cout << "\n>> Nuevo stock: " << p->stock << "\n";
    pausar();
}

void moduloInventario() {
    if (!pedirAdmin()) { pausar(); return; }
    while (true) {
        encabezado("INVENTARIO");
        listarInventario();
        cout << "\n  1) Agregar producto\n";
        cout << "  2) Editar producto\n";
        cout << "  3) Eliminar producto\n";
        cout << "  4) Ingresar mercaderia (ajustar stock)\n";
        cout << "  0) Volver\n";
        int op = leerInt("\nOpcion: ", 0);
        switch (op) {
            case 1: agregarProducto(); break;
            case 2: editarProducto(); break;
            case 3: eliminarProducto(); break;
            case 4: ajustarStock(); break;
            case 0: return;
            default: break;
        }
    }
}

// ----------------------------------------------------------------------------
//  Modulo: Clientes
// ----------------------------------------------------------------------------
void listarClientes() {
    if (clientes.empty()) { cout << "  (no hay clientes registrados)\n"; return; }
    cout << left << setw(6) << "ID" << setw(28) << "Nombre"
         << setw(16) << "Telefono" << right << setw(14) << "Saldo/Deuda" << "\n";
    cout << "----------------------------------------------------------------\n";
    for (const auto& c : clientes) {
        cout << left << setw(6) << c.id
             << setw(28) << (c.nombre.size() > 27 ? c.nombre.substr(0, 27) : c.nombre)
             << setw(16) << c.telefono
             << right << setw(14) << dinero(c.saldo) << "\n";
    }
}

void agregarCliente() {
    encabezado("Clientes - Nuevo cliente");
    Cliente c;
    c.id     = nextClienteId;
    c.nombre = leerLinea("Nombre: ");
    if (c.nombre.empty()) { cout << ">> Nombre requerido.\n"; pausar(); return; }
    c.telefono = leerLinea("Telefono: ");
    c.saldo    = 0.0;
    nextClienteId++;
    clientes.push_back(c);
    guardarDatos();
    cout << "\n>> Cliente agregado (ID " << c.id << ").\n";
    pausar();
}

void registrarCobranza() {
    int id = leerInt("ID del cliente que paga: ");
    Cliente* c = buscarClientePorId(id);
    if (!c) { cout << ">> No existe ese cliente.\n"; pausar(); return; }
    if (c->saldo <= 0) { cout << ">> " << c->nombre << " no tiene deuda.\n"; pausar(); return; }
    cout << "Deuda actual: " << dinero(c->saldo) << "\n";
    double monto = leerDouble("Monto a cobrar " + MONEDA + ": ");
    if (monto <= 0) { cout << ">> Monto invalido.\n"; pausar(); return; }
    c->saldo = max(0.0, c->saldo - monto);
    movsCaja.push_back({(long long)time(nullptr), "ingreso", monto, "Cobranza - " + c->nombre, fechaHoraActual()});
    guardarDatos();
    cout << "\n>> Pago registrado. Saldo restante: " << dinero(c->saldo) << "\n";
    pausar();
}

void moduloClientes() {
    while (true) {
        encabezado("CLIENTES Y CUENTA CORRIENTE");
        listarClientes();
        cout << "\n  1) Nuevo cliente\n";
        cout << "  2) Registrar cobranza (pago de deuda)\n";
        cout << "  0) Volver\n";
        int op = leerInt("\nOpcion: ", 0);
        switch (op) {
            case 1: agregarCliente(); break;
            case 2: registrarCobranza(); break;
            case 0: return;
            default: break;
        }
    }
}

// ----------------------------------------------------------------------------
//  Modulo: Caja
// ----------------------------------------------------------------------------
void resumenCaja(double& ingresos, double& gastos, double& retiros) {
    ingresos = gastos = retiros = 0;
    for (const auto& m : movsCaja) {
        if (m.tipo == "ingreso") ingresos += m.monto;
        else if (m.tipo == "gasto") gastos += m.monto;
        else if (m.tipo == "retiro") retiros += m.monto;
    }
}

void mostrarCaja() {
    double ing, gas, ret;
    resumenCaja(ing, gas, ret);
    double saldo = cajaSaldoInicial + ing - gas - ret;

    cout << "Saldo de apertura : " << dinero(cajaSaldoInicial) << "\n";
    cout << "Ingresos          : " << dinero(ing) << "\n";
    cout << "Gastos            : " << dinero(gas) << "\n";
    cout << "Retiros           : " << dinero(ret) << "\n";
    cout << "-----------------------------------------\n";
    cout << "SALDO EN CAJA     : " << dinero(saldo) << "\n";
    cout << "-----------------------------------------\n";

    cout << "\nUltimos movimientos:\n";
    int mostrados = 0;
    for (auto it = movsCaja.rbegin(); it != movsCaja.rend() && mostrados < 12; ++it, ++mostrados) {
        char signo = (it->tipo == "ingreso") ? '+' : '-';
        cout << "  " << it->fecha.substr(11, 5) << "  " << signo << " "
             << dinero(it->monto) << "   " << it->desc << "\n";
    }
    if (movsCaja.empty()) cout << "  (sin movimientos)\n";
}

void registrarMovCaja(const string& tipo) {
    encabezado("Caja - Registrar " + tipo);
    double monto = leerDouble("Monto " + MONEDA + ": ");
    if (monto <= 0) { cout << ">> Monto invalido.\n"; pausar(); return; }
    string desc = leerLinea("Descripcion: ");
    if (desc.empty()) desc = tipo;
    movsCaja.push_back({(long long)time(nullptr), tipo, monto, desc, fechaHoraActual()});
    guardarDatos();
    cout << "\n>> Movimiento registrado.\n";
    pausar();
}

void cerrarCaja() {
    double ing, gas, ret;
    resumenCaja(ing, gas, ret);
    double saldo = cajaSaldoInicial + ing - gas - ret;
    string r = aMinusculas(leerLinea("Cerrar caja con saldo " + dinero(saldo) + "? (s/n): "));
    if (r != "s" && r != "si") return;
    // El saldo final pasa a ser el saldo de apertura de la nueva sesion
    cajaSaldoInicial = saldo;
    movsCaja.clear();
    guardarDatos();
    cout << "\n>> Caja cerrada. Saldo trasladado: " << dinero(saldo) << "\n";
    pausar();
}

void moduloCaja() {
    while (true) {
        encabezado("CAJA");
        mostrarCaja();
        cout << "\n  1) Registrar ingreso\n";
        cout << "  2) Registrar gasto\n";
        cout << "  3) Registrar retiro\n";
        cout << "  4) Cerrar caja\n";
        cout << "  0) Volver\n";
        int op = leerInt("\nOpcion: ", 0);
        switch (op) {
            case 1: registrarMovCaja("ingreso"); break;
            case 2: registrarMovCaja("gasto"); break;
            case 3: registrarMovCaja("retiro"); break;
            case 4: cerrarCaja(); break;
            case 0: return;
            default: break;
        }
    }
}

// ----------------------------------------------------------------------------
//  Modulo: Reportes
// ----------------------------------------------------------------------------
void moduloReportes() {
    encabezado("REPORTES");

    if (ventas.empty()) {
        cout << "  No hay ventas registradas.\n";
        pausar();
        return;
    }

    double totalGeneral = 0, totalContado = 0, totalFiado = 0, totalHoy = 0;
    string hoy = fechaHoy();
    int ventasHoy = 0;

    cout << "Historial de ventas:\n";
    cout << left << setw(21) << "Fecha/Hora" << setw(18) << "Forma pago"
         << setw(18) << "Cliente" << right << setw(12) << "Total" << "\n";
    cout << "-----------------------------------------------------------------------\n";
    for (const auto& v : ventas) {
        cout << left << setw(21) << v.fecha
             << setw(18) << v.formaPago
             << setw(18) << (v.clienteNombre.empty() ? "Mostrador" : v.clienteNombre)
             << right << setw(12) << dinero(v.total) << "\n";
        totalGeneral += v.total;
        if (v.formaPago == "efectivo") totalContado += v.total; else totalFiado += v.total;
        if (v.fecha.substr(0, 10) == hoy) { totalHoy += v.total; ventasHoy++; }
    }

    cout << "-----------------------------------------------------------------------\n";
    cout << "Ventas totales      : " << ventas.size() << "  (" << dinero(totalGeneral) << ")\n";
    cout << "  Al contado        : " << dinero(totalContado) << "\n";
    cout << "  A cuenta corriente: " << dinero(totalFiado) << "\n";
    cout << "Ventas de hoy (" << hoy << "): " << ventasHoy << "  (" << dinero(totalHoy) << ")\n";

    // Productos mas vendidos
    struct Agg { string nombre; int cant; double monto; };
    vector<Agg> agg;
    for (const auto& v : ventas) {
        for (const auto& it : v.items) {
            bool found = false;
            for (auto& a : agg) if (a.nombre == it.nombre) {
                a.cant += it.cantidad;
                a.monto += it.precio * it.cantidad - it.descuento;
                found = true; break;
            }
            if (!found) agg.push_back({it.nombre, it.cantidad, it.precio * it.cantidad - it.descuento});
        }
    }
    sort(agg.begin(), agg.end(), [](const Agg& a, const Agg& b){ return a.cant > b.cant; });
    cout << "\nProductos mas vendidos:\n";
    int n = 0;
    for (const auto& a : agg) {
        if (n++ >= 10) break;
        cout << "  " << left << setw(30)
             << (a.nombre.size() > 29 ? a.nombre.substr(0, 29) : a.nombre)
             << right << setw(5) << a.cant << " uds   " << dinero(a.monto) << "\n";
    }

    pausar();
}

// ----------------------------------------------------------------------------
//  Menu principal
// ----------------------------------------------------------------------------
void menuPrincipal() {
    while (true) {
        encabezado("MENU PRINCIPAL");
        cout << "  1) Ventas\n";
        cout << "  2) Inventario        " << (esAdmin ? "" : "(requiere administrador)") << "\n";
        cout << "  3) Clientes\n";
        cout << "  4) Caja\n";
        cout << "  5) Reportes\n";
        cout << "  6) " << (esAdmin ? "Cerrar sesion de administrador" : "Iniciar sesion de administrador") << "\n";
        cout << "  0) Salir\n";
        int op = leerInt("\nOpcion: ", -1);
        switch (op) {
            case 1: moduloVentas(); break;
            case 2: moduloInventario(); break;
            case 3: moduloClientes(); break;
            case 4: moduloCaja(); break;
            case 5: moduloReportes(); break;
            case 6:
                if (esAdmin) { esAdmin = false; cout << "\n>> Sesion de administrador cerrada.\n"; pausar(); }
                else { pedirAdmin(); pausar(); }
                break;
            case 0:
                guardarDatos();
                encabezado("Hasta pronto");
                cout << "  Datos guardados en " << DATA_FILE << "\n\n";
                return;
            default: break;
        }
    }
}

// ----------------------------------------------------------------------------
//  main
// ----------------------------------------------------------------------------
int main() {
#ifdef _WIN32
    // Permite acentos basicos en la consola de Windows (UTF-8)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    ifstream test(DATA_FILE);
    if (test.good()) {
        test.close();
        cargarDatos();
    } else {
        cargarDatosEjemplo();
        guardarDatos();
    }

    menuPrincipal();
    return 0;
}

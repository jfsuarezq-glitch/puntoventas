// ============================================================================
//  POS Quiosco - Punto de Venta con interfaz grafica nativa (Win32) en C++
//
//  Ventana de Windows real con pestanas, listas y botones. Es UN solo .exe
//  sin dependencias externas. Comparte el archivo de datos pos_data.txt con
//  la version de consola (main.cpp).
//
//  Modulos: Ventas (carrito + cobro con vuelto + fiado), Inventario,
//  Clientes (cuenta corriente), Caja.
//
//  Compilar a .exe (Windows, MinGW):
//    g++ -std=c++17 -O2 -mwindows -static -o PosQuioscoGUI.exe gui_win32.cpp -lcomctl32 -lgdi32 -luser32
//
//  Clave de administrador: 1234
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "comctl32.lib")

using namespace std;

// ----------------------------------------------------------------------------
//  Configuracion
// ----------------------------------------------------------------------------
static const string  DATA_FILE  = "pos_data.txt";
static const wstring MONEDA      = L"S/";
static const string  ADMIN_PASS  = "1234";

// ----------------------------------------------------------------------------
//  Conversion UTF-8 <-> UTF-16 (para mostrar acentos correctamente)
// ----------------------------------------------------------------------------
wstring toW(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
string toU8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// ----------------------------------------------------------------------------
//  Modelos de datos
// ----------------------------------------------------------------------------
struct Producto { int id; string nombre; double precio; int stock; string codigo; string categoria; };
struct Cliente  { int id; string nombre; string telefono; double saldo; };
struct ItemCarrito { int id; string nombre; double precio; int cantidad; };
struct ItemVenta { string nombre; double precio; int cantidad; double descuento; };
struct Venta { long long idVenta; vector<ItemVenta> items; double total; string formaPago; int clienteId; string clienteNombre; string fecha; };
struct MovCaja { long long id; string tipo; double monto; string desc; string fecha; };

// ----------------------------------------------------------------------------
//  Estado global
// ----------------------------------------------------------------------------
vector<Producto> productos;
vector<Cliente>  clientes;
vector<Venta>    ventas;
vector<MovCaja>  movsCaja;
vector<ItemCarrito> carrito;

int    nextProductoId = 1;
int    nextClienteId  = 1;
double cajaSaldoInicial = 0.0;
bool   esAdmin = false;

// ----------------------------------------------------------------------------
//  Utilidades de texto / dinero / fecha
// ----------------------------------------------------------------------------
string fechaHoraActual() {
    time_t t = time(nullptr); tm* lt = localtime(&t);
    char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt); return buf;
}
string fechaHoy() {
    time_t t = time(nullptr); tm* lt = localtime(&t);
    char buf[16]; strftime(buf, sizeof(buf), "%Y-%m-%d", lt); return buf;
}
wstring dinero(double v) {
    wostringstream os; os << MONEDA << L" " << fixed << setprecision(2) << v; return os.str();
}
wstring numW(double v, int dec = 2) {
    wostringstream os; os << fixed << setprecision(dec) << v; return os.str();
}
wstring numW(int v) { return to_wstring(v); }

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
        } else r += s[i];
    }
    return r;
}
vector<string> dividir(const string& linea, char sep) {
    vector<string> p; string a;
    for (char c : linea) { if (c == sep) { p.push_back(a); a.clear(); } else a += c; }
    p.push_back(a); return p;
}
string aMin(string s) { transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); }); return s; }

// ----------------------------------------------------------------------------
//  Persistencia (comparte formato con la version de consola)
// ----------------------------------------------------------------------------
void guardarDatos() {
    ofstream f(DATA_FILE);
    if (!f) return;
    f << "META|" << nextProductoId << "|" << nextClienteId << "|" << fixed << setprecision(2) << cajaSaldoInicial << "\n";
    for (const auto& p : productos)
        f << "PROD|" << p.id << "|" << escapar(p.nombre) << "|" << fixed << setprecision(2) << p.precio << "|"
          << p.stock << "|" << escapar(p.codigo) << "|" << escapar(p.categoria) << "\n";
    for (const auto& c : clientes)
        f << "CLI|" << c.id << "|" << escapar(c.nombre) << "|" << escapar(c.telefono) << "|"
          << fixed << setprecision(2) << c.saldo << "\n";
    for (const auto& v : ventas) {
        f << "VENTA|" << v.idVenta << "|" << fixed << setprecision(2) << v.total << "|" << escapar(v.formaPago) << "|"
          << v.clienteId << "|" << escapar(v.clienteNombre) << "|" << escapar(v.fecha) << "|" << v.items.size() << "\n";
        for (const auto& it : v.items)
            f << "VITEM|" << escapar(it.nombre) << "|" << fixed << setprecision(2) << it.precio << "|"
              << it.cantidad << "|" << fixed << setprecision(2) << it.descuento << "\n";
    }
    for (const auto& m : movsCaja)
        f << "CAJA|" << m.id << "|" << escapar(m.tipo) << "|" << fixed << setprecision(2) << m.monto << "|"
          << escapar(m.desc) << "|" << escapar(m.fecha) << "\n";
}

void cargarDatos() {
    ifstream f(DATA_FILE);
    if (!f) return;
    productos.clear(); clientes.clear(); ventas.clear(); movsCaja.clear();
    string linea; Venta* va = nullptr;
    while (getline(f, linea)) {
        if (linea.empty()) continue;
        vector<string> c = dividir(linea, '|');
        const string& t = c[0];
        if (t == "META" && c.size() >= 4) {
            nextProductoId = stoi(c[1]); nextClienteId = stoi(c[2]); cajaSaldoInicial = stod(c[3]);
        } else if (t == "PROD" && c.size() >= 7) {
            productos.push_back({stoi(c[1]), desescapar(c[2]), stod(c[3]), stoi(c[4]), desescapar(c[5]), desescapar(c[6])});
        } else if (t == "CLI" && c.size() >= 5) {
            clientes.push_back({stoi(c[1]), desescapar(c[2]), desescapar(c[3]), stod(c[4])});
        } else if (t == "VENTA" && c.size() >= 8) {
            Venta v; v.idVenta = stoll(c[1]); v.total = stod(c[2]); v.formaPago = desescapar(c[3]);
            v.clienteId = stoi(c[4]); v.clienteNombre = desescapar(c[5]); v.fecha = desescapar(c[6]);
            ventas.push_back(v); va = &ventas.back();
        } else if (t == "VITEM" && c.size() >= 5 && va) {
            va->items.push_back({desescapar(c[1]), stod(c[2]), stoi(c[3]), stod(c[4])});
        } else if (t == "CAJA" && c.size() >= 6) {
            movsCaja.push_back({stoll(c[1]), desescapar(c[2]), stod(c[3]), desescapar(c[4]), desescapar(c[5])});
        }
    }
}

void cargarDatosEjemplo() {
    productos = {
        {1, "Agua San Luis 625ml",  1.50, 48, "7751010001234", "Bebidas"},
        {2, "Coca Cola 500ml",      3.00, 24, "7501055300006", "Bebidas"},
        {3, "Galletas Oreo",        2.50, 36, "7622210003232", "Galletas"},
        {4, "Chicles Halls",        1.00, 60, "0040000004096", "Snack"},
        {5, "Papas Lays clasicas",  3.50, 18, "7501012004040", "Snack"},
        {6, "Yogurt Gloria 120g",   2.00, 20, "7750016000789", "Bebidas"},
        {7, "Jugo Pulp naranja",    2.50, 15, "7750016100456", "Bebidas"},
        {8, "Pilas AA Duracell x2", 5.00, 30, "0041333045559", "Otros"},
    };
    nextProductoId = 9; nextClienteId = 1;
}

// Busquedas
Producto* prodPorId(int id) { for (auto& p : productos) if (p.id == id) return &p; return nullptr; }
Producto* prodPorCodigo(const string& cod) { for (auto& p : productos) if (!p.codigo.empty() && p.codigo == cod) return &p; return nullptr; }
Cliente*  cliPorId(int id) { for (auto& c : clientes) if (c.id == id) return &c; return nullptr; }

double totalCarrito() { double t = 0; for (auto& it : carrito) t += it.precio * it.cantidad; return t; }

// ----------------------------------------------------------------------------
//  IDs de controles
// ----------------------------------------------------------------------------
enum {
    ID_TAB = 1000,
    // Ventas
    ID_V_BUSCAR, ID_V_PRODLIST, ID_V_ADD, ID_V_CARTLIST, ID_V_QUITAR, ID_V_LIMPIAR,
    ID_V_TOTAL, ID_V_CLIENTE, ID_V_FIADO, ID_V_EFECTIVO, ID_V_VUELTO, ID_V_COBRAR,
    // Inventario
    ID_I_LIST, ID_I_NUEVO, ID_I_EDITAR, ID_I_ELIMINAR, ID_I_STOCK,
    // Clientes
    ID_C_LIST, ID_C_NUEVO, ID_C_COBRANZA,
    // Caja
    ID_K_INFO, ID_K_ING, ID_K_GAS, ID_K_RET, ID_K_CERRAR,
    // Admin
    ID_ADMIN
};

// Handles globales de controles
HWND hMain, hTab, hStatus;
HFONT hFont, hFontBold, hFontBig;

// Ventas
HWND hVBuscar, hVProd, hVAdd, hVCart, hVQuitar, hVLimpiar, hVTotalLbl, hVTotal,
     hVClienteLbl, hVCliente, hVFiado, hVEfectivoLbl, hVEfectivo, hVVuelto, hVCobrar;
// Inventario
HWND hILabel, hIList, hINuevo, hIEditar, hIEliminar, hIStock;
// Clientes
HWND hCLabel, hCList, hCNuevo, hCCobranza;
// Caja
HWND hKLabel, hKInfo, hKIng, hKGas, hKRet, hKCerrar;
// Admin
HWND hAdminBtn;

int currentTab = 0;

// ----------------------------------------------------------------------------
//  Dialogo de entrada generico (modal, campos dinamicos)
// ----------------------------------------------------------------------------
struct InputField { wstring label; wstring value; };

struct InputCtx { vector<InputField>* fields; wstring title; bool ok; vector<HWND> edits; };

LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    InputCtx* ctx = (InputCtx*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        ctx = (InputCtx*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);
        int y = 14;
        for (size_t i = 0; i < ctx->fields->size(); ++i) {
            CreateWindowW(L"STATIC", (*ctx->fields)[i].label.c_str(), WS_CHILD | WS_VISIBLE,
                          16, y, 300, 18, hwnd, nullptr, nullptr, nullptr);
            HWND e = CreateWindowW(L"EDIT", (*ctx->fields)[i].value.c_str(),
                          WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                          16, y + 20, 300, 24, hwnd, (HMENU)(INT_PTR)(2000 + i), nullptr, nullptr);
            SendMessageW(e, WM_SETFONT, (WPARAM)hFont, TRUE);
            ctx->edits.push_back(e);
            y += 52;
        }
        HWND ok = CreateWindowW(L"BUTTON", L"Aceptar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                      120, y + 4, 90, 30, hwnd, (HMENU)IDOK, nullptr, nullptr);
        HWND cancel = CreateWindowW(L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                      220, y + 4, 96, 30, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
        SendMessageW(ok, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(cancel, WM_SETFONT, (WPARAM)hFont, TRUE);
        // Ajustar alto de la ventana
        RECT rc; GetWindowRect(hwnd, &rc);
        SetWindowPos(hwnd, nullptr, 0, 0, 350, y + 90, SWP_NOMOVE | SWP_NOZORDER);
        if (!ctx->edits.empty()) SetFocus(ctx->edits[0]);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            for (size_t i = 0; i < ctx->edits.size(); ++i) {
                wchar_t buf[512]; GetWindowTextW(ctx->edits[i], buf, 512);
                (*ctx->fields)[i].value = buf;
            }
            ctx->ok = true;
            DestroyWindow(hwnd);
            return 0;
        } else if (LOWORD(wp) == IDCANCEL) {
            ctx->ok = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        ctx->ok = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool InputDialog(HWND parent, const wstring& title, vector<InputField>& fields) {
    static bool registered = false;
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = InputProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"POSInputDlg";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        registered = true;
    }
    InputCtx ctx; ctx.fields = &fields; ctx.title = title; ctx.ok = false;

    RECT pr; GetWindowRect(parent, &pr);
    int x = pr.left + 120, y = pr.top + 100;

    EnableWindow(parent, FALSE);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"POSInputDlg", title.c_str(),
                  WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                  x, y, 350, 300, parent, nullptr, hInst, &ctx);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_QUIT) break;
        if (IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    return ctx.ok;
}

// Helpers de parseo
double parseD(const wstring& w, double def = 0) { try { return stod(w); } catch (...) { return def; } }
int parseI(const wstring& w, int def = 0) { try { return stoi(w); } catch (...) { return def; } }

// ----------------------------------------------------------------------------
//  Utilidades ListView
// ----------------------------------------------------------------------------
void lvAddCol(HWND lv, int idx, const wchar_t* txt, int width) {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = (LPWSTR)txt; col.cx = width; col.iSubItem = idx;
    ListView_InsertColumn(lv, idx, &col);
}
int lvAddRow(HWND lv, int row, const wstring& txt, LPARAM param) {
    LVITEMW it = {};
    it.mask = LVIF_TEXT | LVIF_PARAM; it.iItem = row; it.iSubItem = 0;
    wstring t = txt; it.pszText = (LPWSTR)t.c_str(); it.lParam = param;
    return ListView_InsertItem(lv, &it);
}
void lvSet(HWND lv, int row, int col, const wstring& txt) {
    wstring t = txt; ListView_SetItemText(lv, row, col, (LPWSTR)t.c_str());
}
int lvSelected(HWND lv) { return ListView_GetNextItem(lv, -1, LVNI_SELECTED); }
LPARAM lvParam(HWND lv, int row) {
    LVITEMW it = {}; it.mask = LVIF_PARAM; it.iItem = row; ListView_GetItem(lv, &it); return it.lParam;
}

// ----------------------------------------------------------------------------
//  Refrescos de vista
// ----------------------------------------------------------------------------
void refrescarProductosVenta() {
    wchar_t buf[128]; GetWindowTextW(hVBuscar, buf, 128);
    string filtro = toU8(buf); string q = aMin(filtro);
    ListView_DeleteAllItems(hVProd);
    int row = 0;
    for (auto& p : productos) {
        if (!q.empty()) {
            if (aMin(p.nombre).find(q) == string::npos && p.codigo.find(filtro) == string::npos) continue;
        }
        lvAddRow(hVProd, row, to_wstring(p.id), p.id);
        lvSet(hVProd, row, 1, toW(p.nombre));
        lvSet(hVProd, row, 2, numW(p.precio));
        lvSet(hVProd, row, 3, numW(p.stock));
        lvSet(hVProd, row, 4, toW(p.codigo));
        row++;
    }
}

void refrescarCarrito() {
    ListView_DeleteAllItems(hVCart);
    int row = 0;
    for (auto& it : carrito) {
        lvAddRow(hVCart, row, toW(it.nombre), it.id);
        lvSet(hVCart, row, 1, numW(it.cantidad));
        lvSet(hVCart, row, 2, numW(it.precio));
        lvSet(hVCart, row, 3, numW(it.precio * it.cantidad));
        row++;
    }
    SetWindowTextW(hVTotal, dinero(totalCarrito()).c_str());
    // recalcular vuelto
    wchar_t buf[64]; GetWindowTextW(hVEfectivo, buf, 64);
    double efec = parseD(buf, 0), tot = totalCarrito();
    if (efec >= tot && efec > 0) SetWindowTextW(hVVuelto, (L"Vuelto: " + dinero(efec - tot)).c_str());
    else SetWindowTextW(hVVuelto, L"Vuelto: -");
}

void refrescarComboClientes() {
    SendMessageW(hVCliente, CB_RESETCONTENT, 0, 0);
    SendMessageW(hVCliente, CB_ADDSTRING, 0, (LPARAM)L"Mostrador (contado)");
    SendMessageW(hVCliente, CB_SETITEMDATA, 0, 0);
    int idx = 1;
    for (auto& c : clientes) {
        wstring et = toW(c.nombre);
        if (c.saldo > 0) et += L" (debe " + dinero(c.saldo) + L")";
        SendMessageW(hVCliente, CB_ADDSTRING, 0, (LPARAM)et.c_str());
        SendMessageW(hVCliente, CB_SETITEMDATA, idx, c.id);
        idx++;
    }
    SendMessageW(hVCliente, CB_SETCURSEL, 0, 0);
}

void refrescarInventario() {
    ListView_DeleteAllItems(hIList);
    int row = 0;
    for (auto& p : productos) {
        wstring estado = p.stock <= 0 ? L"SIN STOCK" : (p.stock <= 5 ? L"BAJO" : L"OK");
        lvAddRow(hIList, row, to_wstring(p.id), p.id);
        lvSet(hIList, row, 1, toW(p.nombre));
        lvSet(hIList, row, 2, numW(p.precio));
        lvSet(hIList, row, 3, numW(p.stock));
        lvSet(hIList, row, 4, toW(p.codigo));
        lvSet(hIList, row, 5, toW(p.categoria));
        lvSet(hIList, row, 6, estado);
        row++;
    }
}

void refrescarClientes() {
    ListView_DeleteAllItems(hCList);
    int row = 0;
    for (auto& c : clientes) {
        lvAddRow(hCList, row, to_wstring(c.id), c.id);
        lvSet(hCList, row, 1, toW(c.nombre));
        lvSet(hCList, row, 2, toW(c.telefono));
        lvSet(hCList, row, 3, dinero(c.saldo));
        lvSet(hCList, row, 4, c.saldo > 0 ? L"Debe" : L"Al dia");
        row++;
    }
}

void refrescarCaja() {
    double ing = 0, gas = 0, ret = 0;
    for (auto& m : movsCaja) {
        if (m.tipo == "ingreso") ing += m.monto;
        else if (m.tipo == "gasto") gas += m.monto;
        else if (m.tipo == "retiro") ret += m.monto;
    }
    double saldo = cajaSaldoInicial + ing - gas - ret;
    wstring s;
    s += L"Saldo de apertura : " + dinero(cajaSaldoInicial) + L"\r\n";
    s += L"Ingresos          : " + dinero(ing) + L"\r\n";
    s += L"Gastos            : " + dinero(gas) + L"\r\n";
    s += L"Retiros           : " + dinero(ret) + L"\r\n";
    s += L"--------------------------------------\r\n";
    s += L"SALDO EN CAJA     : " + dinero(saldo) + L"\r\n\r\n";
    s += L"Ultimos movimientos:\r\n";
    int n = 0;
    for (auto it = movsCaja.rbegin(); it != movsCaja.rend() && n < 20; ++it, ++n) {
        wchar_t signo = (it->tipo == "ingreso") ? L'+' : L'-';
        wstring hora = it->fecha.size() >= 16 ? toW(it->fecha.substr(11, 5)) : L"";
        s += L"  " + hora + L"  " + signo + L" " + dinero(it->monto) + L"   " + toW(it->desc) + L"\r\n";
    }
    if (movsCaja.empty()) s += L"  (sin movimientos)\r\n";
    SetWindowTextW(hKInfo, s.c_str());
}

void setStatus(const wstring& txt) {
    SendMessageW(hStatus, SB_SETTEXT, 0, (LPARAM)txt.c_str());
}

// ----------------------------------------------------------------------------
//  Logica de negocio (acciones)
// ----------------------------------------------------------------------------
void agregarAlCarritoPorId(int id) {
    Producto* p = prodPorId(id);
    if (!p) return;
    if (p->stock <= 0) { setStatus(L"Sin stock: " + toW(p->nombre)); return; }
    for (auto& it : carrito) {
        if (it.id == id) {
            if (it.cantidad >= p->stock) { setStatus(L"Stock maximo: " + toW(p->nombre)); return; }
            it.cantidad++; refrescarCarrito(); setStatus(L"+1 " + toW(p->nombre)); return;
        }
    }
    carrito.push_back({p->id, p->nombre, p->precio, 1});
    refrescarCarrito();
    setStatus(L"Agregado: " + toW(p->nombre));
}

void ventaBuscarYAgregar() {
    wchar_t buf[128]; GetWindowTextW(hVBuscar, buf, 128);
    string entrada = toU8(buf);
    if (entrada.empty()) return;
    Producto* p = prodPorCodigo(entrada);
    if (!p) { string q = aMin(entrada); for (auto& x : productos) if (aMin(x.nombre) == q) { p = &x; break; } }
    if (!p) {
        string q = aMin(entrada); vector<Producto*> m;
        for (auto& x : productos) if (aMin(x.nombre).find(q) != string::npos) m.push_back(&x);
        if (m.size() == 1) p = m[0];
    }
    if (p) { agregarAlCarritoPorId(p->id); SetWindowTextW(hVBuscar, L""); refrescarProductosVenta(); }
    else setStatus(L"No encontrado: " + toW(entrada));
}

void cobrar() {
    if (carrito.empty()) { setStatus(L"El carrito esta vacio"); return; }
    double total = totalCarrito();
    int sel = (int)SendMessageW(hVCliente, CB_GETCURSEL, 0, 0);
    int clienteId = (int)SendMessageW(hVCliente, CB_GETITEMDATA, sel, 0);
    Cliente* cliente = clienteId > 0 ? cliPorId(clienteId) : nullptr;
    bool fiado = (SendMessageW(hVFiado, BM_GETCHECK, 0, 0) == BST_CHECKED) && cliente;

    if (!fiado) {
        wchar_t buf[64]; GetWindowTextW(hVEfectivo, buf, 64);
        double efec = parseD(buf, 0);
        if (efec > 0 && efec < total) {
            MessageBoxW(hMain, L"Efectivo insuficiente.", L"Cobro", MB_ICONWARNING); return;
        }
        wstring msg = L"Total: " + dinero(total);
        if (efec > 0) msg += L"\nEfectivo: " + dinero(efec) + L"\nVUELTO: " + dinero(efec - total);
        msg += L"\n\n¿Confirmar venta al contado?";
        if (MessageBoxW(hMain, msg.c_str(), L"Confirmar cobro", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    } else {
        wstring msg = L"Venta a cuenta corriente de " + toW(cliente->nombre) +
                      L"\nTotal: " + dinero(total) + L"\n\n¿Confirmar?";
        if (MessageBoxW(hMain, msg.c_str(), L"Confirmar fiado", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    }

    Venta v;
    v.idVenta = (long long)time(nullptr);
    v.total = total;
    v.formaPago = fiado ? "cuenta corriente" : "efectivo";
    v.clienteId = cliente ? cliente->id : 0;
    v.clienteNombre = cliente ? cliente->nombre : "";
    v.fecha = fechaHoraActual();
    for (auto& it : carrito) {
        Producto* p = prodPorId(it.id);
        if (p) p->stock = max(0, p->stock - it.cantidad);
        v.items.push_back({it.nombre, it.precio, it.cantidad, 0.0});
    }
    ventas.push_back(v);
    if (fiado && cliente) {
        cliente->saldo += total;
        setStatus(L"Venta fiada. Nuevo saldo " + toW(cliente->nombre) + L": " + dinero(cliente->saldo));
    } else {
        movsCaja.push_back({(long long)time(nullptr), "ingreso", total, "Venta", fechaHoraActual()});
        setStatus(L"Venta registrada: " + dinero(total));
    }
    carrito.clear();
    SetWindowTextW(hVEfectivo, L"");
    SendMessageW(hVFiado, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(hVCliente, CB_SETCURSEL, 0, 0);
    refrescarCarrito();
    refrescarProductosVenta();
    refrescarComboClientes();
    guardarDatos();
}

bool pedirAdmin() {
    if (esAdmin) return true;
    vector<InputField> f = {{L"Clave de administrador:", L""}};
    if (!InputDialog(hMain, L"Ingresar como administrador", f)) return false;
    if (toU8(f[0].value) == ADMIN_PASS) {
        esAdmin = true;
        SetWindowTextW(hAdminBtn, L"Cerrar sesion admin");
        setStatus(L"Sesion de administrador iniciada");
        return true;
    }
    MessageBoxW(hMain, L"Clave incorrecta.", L"Administrador", MB_ICONERROR);
    return false;
}

// Inventario
void invNuevo() {
    if (!pedirAdmin()) return;
    vector<InputField> f = {{L"Nombre:", L""}, {L"Precio:", L""}, {L"Stock inicial:", L""}, {L"Codigo de barras:", L""}, {L"Categoria:", L""}};
    if (!InputDialog(hMain, L"Nuevo producto", f)) return;
    if (f[0].value.empty()) { MessageBoxW(hMain, L"El nombre es obligatorio.", L"Inventario", MB_ICONWARNING); return; }
    Producto p; p.id = nextProductoId++;
    p.nombre = toU8(f[0].value); p.precio = parseD(f[1].value); p.stock = parseI(f[2].value);
    p.codigo = toU8(f[3].value); p.categoria = toU8(f[4].value);
    productos.push_back(p);
    guardarDatos(); refrescarInventario(); refrescarProductosVenta();
    setStatus(L"Producto agregado");
}
void invEditar() {
    if (!pedirAdmin()) return;
    int row = lvSelected(hIList);
    if (row < 0) { MessageBoxW(hMain, L"Selecciona un producto de la lista.", L"Inventario", MB_ICONINFORMATION); return; }
    Producto* p = prodPorId((int)lvParam(hIList, row));
    if (!p) return;
    vector<InputField> f = {{L"Nombre:", toW(p->nombre)}, {L"Precio:", numW(p->precio)}, {L"Stock:", numW(p->stock)},
                            {L"Codigo de barras:", toW(p->codigo)}, {L"Categoria:", toW(p->categoria)}};
    if (!InputDialog(hMain, L"Editar producto", f)) return;
    p->nombre = toU8(f[0].value); p->precio = parseD(f[1].value, p->precio);
    p->stock = parseI(f[2].value, p->stock); p->codigo = toU8(f[3].value); p->categoria = toU8(f[4].value);
    guardarDatos(); refrescarInventario(); refrescarProductosVenta();
    setStatus(L"Producto actualizado");
}
void invEliminar() {
    if (!pedirAdmin()) return;
    int row = lvSelected(hIList);
    if (row < 0) { MessageBoxW(hMain, L"Selecciona un producto.", L"Inventario", MB_ICONINFORMATION); return; }
    int id = (int)lvParam(hIList, row);
    Producto* p = prodPorId(id);
    if (!p) return;
    if (MessageBoxW(hMain, (L"¿Eliminar \"" + toW(p->nombre) + L"\"?").c_str(), L"Eliminar", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    productos.erase(remove_if(productos.begin(), productos.end(), [id](const Producto& x){ return x.id == id; }), productos.end());
    guardarDatos(); refrescarInventario(); refrescarProductosVenta();
    setStatus(L"Producto eliminado");
}
void invStock() {
    if (!pedirAdmin()) return;
    int row = lvSelected(hIList);
    if (row < 0) { MessageBoxW(hMain, L"Selecciona un producto.", L"Inventario", MB_ICONINFORMATION); return; }
    Producto* p = prodPorId((int)lvParam(hIList, row));
    if (!p) return;
    vector<InputField> f = {{L"Cantidad a ingresar (negativo para restar):", L""}};
    if (!InputDialog(hMain, L"Ingreso de mercaderia - " + toW(p->nombre), f)) return;
    p->stock = max(0, p->stock + parseI(f[0].value));
    guardarDatos(); refrescarInventario(); refrescarProductosVenta();
    setStatus(L"Stock actualizado: " + numW(p->stock));
}

// Clientes
void cliNuevo() {
    vector<InputField> f = {{L"Nombre:", L""}, {L"Telefono:", L""}};
    if (!InputDialog(hMain, L"Nuevo cliente", f)) return;
    if (f[0].value.empty()) { MessageBoxW(hMain, L"El nombre es obligatorio.", L"Clientes", MB_ICONWARNING); return; }
    Cliente c; c.id = nextClienteId++; c.nombre = toU8(f[0].value); c.telefono = toU8(f[1].value); c.saldo = 0;
    clientes.push_back(c);
    guardarDatos(); refrescarClientes(); refrescarComboClientes();
    setStatus(L"Cliente agregado");
}
void cliCobranza() {
    int row = lvSelected(hCList);
    if (row < 0) { MessageBoxW(hMain, L"Selecciona un cliente.", L"Clientes", MB_ICONINFORMATION); return; }
    Cliente* c = cliPorId((int)lvParam(hCList, row));
    if (!c) return;
    if (c->saldo <= 0) { MessageBoxW(hMain, (toW(c->nombre) + L" no tiene deuda.").c_str(), L"Cobranza", MB_ICONINFORMATION); return; }
    vector<InputField> f = {{L"Deuda actual: " + dinero(c->saldo) + L"  -  Monto a cobrar:", L""}};
    if (!InputDialog(hMain, L"Registrar cobranza", f)) return;
    double monto = parseD(f[0].value);
    if (monto <= 0) { MessageBoxW(hMain, L"Monto invalido.", L"Cobranza", MB_ICONWARNING); return; }
    c->saldo = max(0.0, c->saldo - monto);
    movsCaja.push_back({(long long)time(nullptr), "ingreso", monto, "Cobranza - " + c->nombre, fechaHoraActual()});
    guardarDatos(); refrescarClientes(); refrescarComboClientes();
    setStatus(L"Pago registrado. Saldo: " + dinero(c->saldo));
}

// Caja
void cajaMov(const string& tipo) {
    vector<InputField> f = {{L"Monto:", L""}, {L"Descripcion:", L""}};
    if (!InputDialog(hMain, toW("Registrar " + tipo), f)) return;
    double monto = parseD(f[0].value);
    if (monto <= 0) { MessageBoxW(hMain, L"Monto invalido.", L"Caja", MB_ICONWARNING); return; }
    string desc = toU8(f[1].value); if (desc.empty()) desc = tipo;
    movsCaja.push_back({(long long)time(nullptr), tipo, monto, desc, fechaHoraActual()});
    guardarDatos(); refrescarCaja();
    setStatus(L"Movimiento registrado");
}
void cajaCerrar() {
    double ing = 0, gas = 0, ret = 0;
    for (auto& m : movsCaja) { if (m.tipo=="ingreso") ing+=m.monto; else if (m.tipo=="gasto") gas+=m.monto; else if (m.tipo=="retiro") ret+=m.monto; }
    double saldo = cajaSaldoInicial + ing - gas - ret;
    if (MessageBoxW(hMain, (L"Cerrar caja con saldo " + dinero(saldo) + L"?\nEl saldo se trasladara a la nueva sesion.").c_str(),
                    L"Cerrar caja", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    cajaSaldoInicial = saldo;
    movsCaja.clear();
    guardarDatos(); refrescarCaja();
    setStatus(L"Caja cerrada. Saldo trasladado: " + dinero(saldo));
}

// ----------------------------------------------------------------------------
//  Mostrar / ocultar controles segun pestana
// ----------------------------------------------------------------------------
void mostrar(HWND h, bool v) { ShowWindow(h, v ? SW_SHOW : SW_HIDE); }

void aplicarTab(int tab) {
    currentTab = tab;
    bool ventas = (tab == 0), inv = (tab == 1), cli = (tab == 2), caja = (tab == 3);
    HWND vs[] = {hVBuscar, hVProd, hVAdd, hVCart, hVQuitar, hVLimpiar, hVTotalLbl, hVTotal,
                 hVClienteLbl, hVCliente, hVFiado, hVEfectivoLbl, hVEfectivo, hVVuelto, hVCobrar};
    for (HWND h : vs) mostrar(h, ventas);
    HWND is[] = {hILabel, hIList, hINuevo, hIEditar, hIEliminar, hIStock};
    for (HWND h : is) mostrar(h, inv);
    HWND cs[] = {hCLabel, hCList, hCNuevo, hCCobranza};
    for (HWND h : cs) mostrar(h, cli);
    HWND ks[] = {hKLabel, hKInfo, hKIng, hKGas, hKRet, hKCerrar};
    for (HWND h : ks) mostrar(h, caja);

    if (inv) refrescarInventario();
    if (cli) refrescarClientes();
    if (caja) refrescarCaja();
    if (ventas) { refrescarProductosVenta(); refrescarCarrito(); refrescarComboClientes(); }
}

// ----------------------------------------------------------------------------
//  Crear controles
// ----------------------------------------------------------------------------
HWND mkBtn(int id, const wchar_t* txt, int x, int y, int w, int h) {
    HWND b = CreateWindowW(L"BUTTON", txt, WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
                           x, y, w, h, hMain, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(b, WM_SETFONT, (WPARAM)hFont, TRUE);
    return b;
}
HWND mkStatic(const wchar_t* txt, int x, int y, int w, int h, HFONT font = nullptr) {
    HWND s = CreateWindowW(L"STATIC", txt, WS_CHILD, x, y, w, h, hMain, nullptr, nullptr, nullptr);
    SendMessageW(s, WM_SETFONT, (WPARAM)(font ? font : hFont), TRUE);
    return s;
}
HWND mkEdit(int id, int x, int y, int w, int h, DWORD extra = 0) {
    HWND e = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | WS_TABSTOP | extra,
                           x, y, w, h, hMain, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(e, WM_SETFONT, (WPARAM)hFont, TRUE);
    return e;
}
HWND mkList(int id, int x, int y, int w, int h) {
    HWND lv = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                            x, y, w, h, hMain, (HMENU)(INT_PTR)id, nullptr, nullptr);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    SendMessageW(lv, WM_SETFONT, (WPARAM)hFont, TRUE);
    return lv;
}

void crearControles() {
    int top = 44;          // debajo del tab control
    int W = 980;

    // ---- VENTAS ----
    mkStatic(L"Buscar / codigo de barras:", 16, top + 6, 200, 20);
    hVBuscar = mkEdit(ID_V_BUSCAR, 220, top + 4, 300, 24);
    hVAdd    = mkBtn(ID_V_ADD, L"Agregar al carrito", 530, top + 3, 160, 26);

    hVProd = mkList(ID_V_PRODLIST, 16, top + 40, 470, 380);
    lvAddCol(hVProd, 0, L"ID", 40); lvAddCol(hVProd, 1, L"Producto", 200);
    lvAddCol(hVProd, 2, L"Precio", 70); lvAddCol(hVProd, 3, L"Stock", 55); lvAddCol(hVProd, 4, L"Codigo", 100);

    mkStatic(L"CARRITO", 510, top + 40, 200, 20, hFontBold);
    hVCart = mkList(ID_V_CARTLIST, 510, top + 62, 460, 250);
    lvAddCol(hVCart, 0, L"Producto", 210); lvAddCol(hVCart, 1, L"Cant", 55);
    lvAddCol(hVCart, 2, L"P.Unit", 85); lvAddCol(hVCart, 3, L"Subtotal", 95);

    hVQuitar  = mkBtn(ID_V_QUITAR, L"Quitar linea", 510, top + 318, 130, 26);
    hVLimpiar = mkBtn(ID_V_LIMPIAR, L"Limpiar carrito", 650, top + 318, 140, 26);

    hVTotalLbl = mkStatic(L"TOTAL:", 510, top + 356, 90, 28, hFontBig);
    hVTotal    = mkStatic(L"S/ 0.00", 610, top + 356, 200, 28, hFontBig);

    hVClienteLbl = mkStatic(L"Cliente:", 510, top + 394, 60, 20);
    hVCliente = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                              575, top + 390, 260, 200, hMain, (HMENU)ID_V_CLIENTE, nullptr, nullptr);
    SendMessageW(hVCliente, WM_SETFONT, (WPARAM)hFont, TRUE);
    hVFiado = CreateWindowW(L"BUTTON", L"Fiado (cuenta corriente)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
                            845, top + 392, 130, 20, hMain, (HMENU)ID_V_FIADO, nullptr, nullptr);
    SendMessageW(hVFiado, WM_SETFONT, (WPARAM)hFont, TRUE);

    hVEfectivoLbl = mkStatic(L"Efectivo S/:", 510, top + 428, 80, 20);
    hVEfectivo = mkEdit(ID_V_EFECTIVO, 595, top + 426, 120, 24);
    hVVuelto   = mkStatic(L"Vuelto: -", 730, top + 428, 240, 20);

    hVCobrar = mkBtn(ID_V_COBRAR, L"COBRAR (F2)", 510, top + 458, 465, 40);
    SendMessageW(hVCobrar, WM_SETFONT, (WPARAM)hFontBig, TRUE);

    // ---- INVENTARIO ----
    hILabel = mkStatic(L"Inventario - stock actual", 16, top + 6, 300, 20, hFontBold);
    hIList = mkList(ID_I_LIST, 16, top + 34, 955, 420);
    lvAddCol(hIList, 0, L"ID", 40); lvAddCol(hIList, 1, L"Producto", 260);
    lvAddCol(hIList, 2, L"Precio", 80); lvAddCol(hIList, 3, L"Stock", 70);
    lvAddCol(hIList, 4, L"Codigo", 150); lvAddCol(hIList, 5, L"Categoria", 140); lvAddCol(hIList, 6, L"Estado", 100);
    hINuevo    = mkBtn(ID_I_NUEVO,    L"Nuevo producto",     16,  top + 464, 150, 30);
    hIEditar   = mkBtn(ID_I_EDITAR,   L"Editar",             176, top + 464, 110, 30);
    hIEliminar = mkBtn(ID_I_ELIMINAR, L"Eliminar",           296, top + 464, 110, 30);
    hIStock    = mkBtn(ID_I_STOCK,    L"Ingresar mercaderia",416, top + 464, 170, 30);

    // ---- CLIENTES ----
    hCLabel = mkStatic(L"Clientes y cuenta corriente", 16, top + 6, 300, 20, hFontBold);
    hCList = mkList(ID_C_LIST, 16, top + 34, 955, 420);
    lvAddCol(hCList, 0, L"ID", 40); lvAddCol(hCList, 1, L"Nombre", 280);
    lvAddCol(hCList, 2, L"Telefono", 160); lvAddCol(hCList, 3, L"Saldo/Deuda", 140); lvAddCol(hCList, 4, L"Estado", 120);
    hCNuevo    = mkBtn(ID_C_NUEVO,    L"Nuevo cliente",             16,  top + 464, 150, 30);
    hCCobranza = mkBtn(ID_C_COBRANZA, L"Registrar cobranza (pago)", 176, top + 464, 210, 30);

    // ---- CAJA ----
    hKLabel = mkStatic(L"Caja - sesion actual", 16, top + 6, 300, 20, hFontBold);
    hKInfo = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                           16, top + 34, 955, 420, hMain, (HMENU)ID_K_INFO, nullptr, nullptr);
    SendMessageW(hKInfo, WM_SETFONT, (WPARAM)hFont, TRUE);
    hKIng    = mkBtn(ID_K_ING,    L"Ingreso",    16,  top + 464, 120, 30);
    hKGas    = mkBtn(ID_K_GAS,    L"Gasto",      146, top + 464, 120, 30);
    hKRet    = mkBtn(ID_K_RET,    L"Retiro",     276, top + 464, 120, 30);
    hKCerrar = mkBtn(ID_K_CERRAR, L"Cerrar caja",406, top + 464, 140, 30);
}

// ----------------------------------------------------------------------------
//  Ventana principal
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        hMain = hwnd;
        // Fuentes
        hFont     = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontBold = CreateFontW(-15, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontBig  = CreateFontW(-22, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        // Tab control
        hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                             8, 8, 984, 32, hwnd, (HMENU)ID_TAB, nullptr, nullptr);
        SendMessageW(hTab, WM_SETFONT, (WPARAM)hFont, TRUE);
        const wchar_t* tabs[] = {L"  Ventas  ", L"  Inventario  ", L"  Clientes  ", L"  Caja  "};
        for (int i = 0; i < 4; ++i) {
            TCITEMW tie = {}; tie.mask = TCIF_TEXT; tie.pszText = (LPWSTR)tabs[i];
            SendMessageW(hTab, TCM_INSERTITEMW, i, (LPARAM)&tie);
        }

        // Boton admin (arriba a la derecha)
        hAdminBtn = CreateWindowW(L"BUTTON", L"Ingresar como admin", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                  800, 8, 190, 28, hwnd, (HMENU)ID_ADMIN, nullptr, nullptr);
        SendMessageW(hAdminBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Barra de estado
        hStatus = CreateWindowW(STATUSCLASSNAMEW, L"Listo", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                0, 0, 0, 0, hwnd, (HMENU)0, nullptr, nullptr);

        crearControles();
        aplicarTab(0);
        return 0;
    }
    case WM_NOTIFY: {
        LPNMHDR nh = (LPNMHDR)lp;
        if (nh->idFrom == ID_TAB && nh->code == TCN_SELCHANGE) {
            int sel = (int)SendMessageW(hTab, TCM_GETCURSEL, 0, 0);
            aplicarTab(sel);
            return 0;
        }
        // Doble clic en lista de productos -> agregar al carrito
        if (nh->idFrom == ID_V_PRODLIST && nh->code == NM_DBLCLK) {
            int row = lvSelected(hVProd);
            if (row >= 0) agregarAlCarritoPorId((int)lvParam(hVProd, row));
            return 0;
        }
        // Doble clic en inventario -> editar
        if (nh->idFrom == ID_I_LIST && nh->code == NM_DBLCLK) { invEditar(); return 0; }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int code = HIWORD(wp);
        switch (id) {
        case ID_V_ADD:      ventaBuscarYAgregar(); break;
        case ID_V_BUSCAR:   if (code == EN_CHANGE) refrescarProductosVenta(); break;
        case ID_V_EFECTIVO: if (code == EN_CHANGE) refrescarCarrito(); break;
        case ID_V_FIADO:    refrescarCarrito(); break;
        case ID_V_QUITAR: {
            int row = lvSelected(hVCart);
            if (row >= 0 && row < (int)carrito.size()) { carrito.erase(carrito.begin() + row); refrescarCarrito(); }
            break;
        }
        case ID_V_LIMPIAR:  carrito.clear(); refrescarCarrito(); break;
        case ID_V_COBRAR:   cobrar(); break;
        case ID_I_NUEVO:    invNuevo(); break;
        case ID_I_EDITAR:   invEditar(); break;
        case ID_I_ELIMINAR: invEliminar(); break;
        case ID_I_STOCK:    invStock(); break;
        case ID_C_NUEVO:    cliNuevo(); break;
        case ID_C_COBRANZA: cliCobranza(); break;
        case ID_K_ING:      cajaMov("ingreso"); break;
        case ID_K_GAS:      cajaMov("gasto"); break;
        case ID_K_RET:      cajaMov("retiro"); break;
        case ID_K_CERRAR:   cajaCerrar(); break;
        case ID_ADMIN:
            if (esAdmin) { esAdmin = false; SetWindowTextW(hAdminBtn, L"Ingresar como admin"); setStatus(L"Sesion admin cerrada"); }
            else pedirAdmin();
            break;
        }
        return 0;
    }
    case WM_SIZE:
        SendMessageW(hStatus, WM_SIZE, 0, 0);
        return 0;
    case WM_DESTROY:
        guardarDatos();
        DeleteObject(hFont); DeleteObject(hFontBold); DeleteObject(hFontBig);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ----------------------------------------------------------------------------
//  WinMain
// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    // Cargar datos
    ifstream test(DATA_FILE);
    if (test.good()) { test.close(); cargarDatos(); }
    else { cargarDatosEjemplo(); guardarDatos(); }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"POSQuioscoMain";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"POSQuioscoMain", L"POS Quiosco - Punto de Venta",
                              WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1016, 620,
                              nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Acelerador simple para F2 = cobrar
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F2 && currentTab == 0) { cobrar(); continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
            GetFocus() == hVBuscar) { ventaBuscarYAgregar(); continue; }
        if (IsDialogMessageW(hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

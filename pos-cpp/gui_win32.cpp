// ============================================================================
//  POS Quiosco - Punto de Venta con interfaz grafica moderna (Win32) en C++
//
//  Ventana de Windows nativa con cabecera de color, pestanas tipo "pill",
//  botones planos y listas limpias. Es UN solo .exe estatico sin dependencias.
//  Comparte el archivo de datos pos_data.txt con la version de consola.
//
//  Modulos: Ventas, Inventario, Clientes, Proveedores (compras y pagos),
//  Canchas (alquiler y reservas) y Caja.
//
//  Compilar a .exe (Windows, MinGW):
//    g++ -std=c++17 -O2 -municode -mwindows -static -o PosQuioscoGUI.exe gui_win32.cpp -lcomctl32 -lgdi32 -luser32
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
#include <map>

#pragma comment(lib, "comctl32.lib")

using namespace std;

// ----------------------------------------------------------------------------
//  Configuracion
// ----------------------------------------------------------------------------
static const string  DATA_FILE  = "pos_data.txt";
static const wstring MONEDA      = L"S/";
static const string  ADMIN_PASS  = "1234";

// ----------------------------------------------------------------------------
//  Paleta de colores (aspecto moderno)
// ----------------------------------------------------------------------------
static const COLORREF C_ACCENT   = RGB(24, 95, 165);    // azul de marca
static const COLORREF C_ACCENT_D = RGB(16, 68, 120);
static const COLORREF C_SUCCESS  = RGB(56, 142, 60);    // verde cobrar
static const COLORREF C_SUCCESS_D= RGB(40, 110, 45);
static const COLORREF C_DANGER   = RGB(200, 62, 56);
static const COLORREF C_BG       = RGB(240, 242, 245);  // fondo general
static const COLORREF C_CARD     = RGB(255, 255, 255);
static const COLORREF C_TEXT     = RGB(28, 30, 34);
static const COLORREF C_MUTED    = RGB(120, 124, 130);
static const COLORREF C_BORDER   = RGB(210, 214, 220);
static const COLORREF C_TABIDLE  = RGB(225, 228, 233);

// ----------------------------------------------------------------------------
//  Conversion UTF-8 <-> UTF-16
// ----------------------------------------------------------------------------
wstring toW(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
string toU8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}

// ----------------------------------------------------------------------------
//  Modelos de datos
// ----------------------------------------------------------------------------
struct Producto  { int id; string nombre; double precio; int stock; string codigo; string categoria; };
struct Cliente   { int id; string nombre; string telefono; double saldo; };
struct Proveedor { int id; string nombre; string telefono; string contacto; double saldo; };
struct CompraItem{ string producto; int cantidad; double costo; };
struct Compra    { long long id; int proveedorId; vector<CompraItem> items; double total; bool pagado; string fecha; };
struct Cancha    { int id; string nombre; double precio; };
struct Reserva   { long long id; int canchaId; string fecha; int hora; int duracion; string cliente;
                   double descuento; double total; double adelanto; bool pagado; long long cajaMovId; };
struct ItemCarrito { int id; string nombre; double precio; int cantidad; };
struct ItemVenta { string nombre; double precio; int cantidad; double descuento; };
struct Venta     { long long idVenta; vector<ItemVenta> items; double total; string formaPago; int clienteId; string clienteNombre; string fecha; };
struct MovCaja   { long long id; string tipo; double monto; string desc; string fecha; };

// ----------------------------------------------------------------------------
//  Estado global
// ----------------------------------------------------------------------------
vector<Producto>  productos;
vector<Cliente>   clientes;
vector<Proveedor> proveedores;
vector<Compra>    compras;
vector<Cancha>    canchas;
vector<Reserva>   reservas;
vector<Venta>     ventas;
vector<MovCaja>   movsCaja;
vector<ItemCarrito> carrito;

int nextProductoId = 1, nextClienteId = 1, nextProveedorId = 1, nextCanchaId = 1;
long long nextReservaId = 1;
double cajaSaldoInicial = 0.0;
bool esAdmin = false;

// ----------------------------------------------------------------------------
//  Utilidades texto / dinero / fecha
// ----------------------------------------------------------------------------
string fechaHoraActual() { time_t t=time(nullptr); tm* lt=localtime(&t); char b[32]; strftime(b,sizeof(b),"%Y-%m-%d %H:%M:%S",lt); return b; }
string fechaHoy()        { time_t t=time(nullptr); tm* lt=localtime(&t); char b[16]; strftime(b,sizeof(b),"%Y-%m-%d",lt); return b; }
wstring dinero(double v) { wostringstream os; os<<MONEDA<<L" "<<fixed<<setprecision(2)<<v; return os.str(); }
wstring numW(double v,int d=2){ wostringstream os; os<<fixed<<setprecision(d)<<v; return os.str(); }
wstring numW(int v){ return to_wstring(v); }
wstring hhmm(int h){ wchar_t b[8]; wsprintfW(b,L"%02d:00",h); return b; }

string escapar(const string& s){ string r; for(char c:s){ if(c=='|')r+="\\p"; else if(c=='\n')r+="\\n"; else if(c=='\\')r+="\\\\"; else r+=c;} return r; }
string desescapar(const string& s){ string r; for(size_t i=0;i<s.size();++i){ if(s[i]=='\\'&&i+1<s.size()){char n=s[i+1]; if(n=='p'){r+='|';++i;} else if(n=='n'){r+='\n';++i;} else if(n=='\\'){r+='\\';++i;} else r+=s[i];} else r+=s[i];} return r; }
vector<string> dividir(const string& l,char sep){ vector<string> p; string a; for(char c:l){ if(c==sep){p.push_back(a);a.clear();} else a+=c;} p.push_back(a); return p; }
string aMin(string s){ transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)tolower(c);}); return s; }

// ----------------------------------------------------------------------------
//  Persistencia (compatible con la version de consola)
// ----------------------------------------------------------------------------
void guardarDatos() {
    ofstream f(DATA_FILE); if (!f) return;
    f << "META|" << nextProductoId << "|" << nextClienteId << "|" << fixed << setprecision(2) << cajaSaldoInicial
      << "|" << nextProveedorId << "|" << nextCanchaId << "|" << nextReservaId << "\n";
    for (auto& p : productos)
        f << "PROD|" << p.id << "|" << escapar(p.nombre) << "|" << fixed << setprecision(2) << p.precio << "|"
          << p.stock << "|" << escapar(p.codigo) << "|" << escapar(p.categoria) << "\n";
    for (auto& c : clientes)
        f << "CLI|" << c.id << "|" << escapar(c.nombre) << "|" << escapar(c.telefono) << "|" << fixed << setprecision(2) << c.saldo << "\n";
    for (auto& p : proveedores)
        f << "PROV|" << p.id << "|" << escapar(p.nombre) << "|" << escapar(p.telefono) << "|" << escapar(p.contacto)
          << "|" << fixed << setprecision(2) << p.saldo << "\n";
    for (auto& c : compras) {
        f << "COMPRA|" << c.id << "|" << c.proveedorId << "|" << fixed << setprecision(2) << c.total << "|"
          << (c.pagado?1:0) << "|" << escapar(c.fecha) << "|" << c.items.size() << "\n";
        for (auto& it : c.items)
            f << "CITEM|" << escapar(it.producto) << "|" << it.cantidad << "|" << fixed << setprecision(2) << it.costo << "\n";
    }
    for (auto& c : canchas)
        f << "CANCHA|" << c.id << "|" << escapar(c.nombre) << "|" << fixed << setprecision(2) << c.precio << "\n";
    for (auto& r : reservas)
        f << "RESERVA|" << r.id << "|" << r.canchaId << "|" << escapar(r.fecha) << "|" << r.hora << "|" << r.duracion
          << "|" << escapar(r.cliente) << "|" << fixed << setprecision(2) << r.descuento << "|" << r.total << "|"
          << r.adelanto << "|" << (r.pagado?1:0) << "|" << r.cajaMovId << "\n";
    for (auto& v : ventas) {
        f << "VENTA|" << v.idVenta << "|" << fixed << setprecision(2) << v.total << "|" << escapar(v.formaPago) << "|"
          << v.clienteId << "|" << escapar(v.clienteNombre) << "|" << escapar(v.fecha) << "|" << v.items.size() << "\n";
        for (auto& it : v.items)
            f << "VITEM|" << escapar(it.nombre) << "|" << fixed << setprecision(2) << it.precio << "|" << it.cantidad << "|" << fixed << setprecision(2) << it.descuento << "\n";
    }
    for (auto& m : movsCaja)
        f << "CAJA|" << m.id << "|" << escapar(m.tipo) << "|" << fixed << setprecision(2) << m.monto << "|" << escapar(m.desc) << "|" << escapar(m.fecha) << "\n";
}

void cargarDatos() {
    ifstream f(DATA_FILE); if (!f) return;
    productos.clear(); clientes.clear(); proveedores.clear(); compras.clear();
    canchas.clear(); reservas.clear(); ventas.clear(); movsCaja.clear();
    string linea; Venta* va = nullptr; Compra* ca = nullptr;
    while (getline(f, linea)) {
        if (linea.empty()) continue;
        vector<string> c = dividir(linea, '|'); const string& t = c[0];
        if (t=="META" && c.size()>=4) {
            nextProductoId=stoi(c[1]); nextClienteId=stoi(c[2]); cajaSaldoInicial=stod(c[3]);
            if (c.size()>=7){ nextProveedorId=stoi(c[4]); nextCanchaId=stoi(c[5]); nextReservaId=stoll(c[6]); }
        } else if (t=="PROD" && c.size()>=7) {
            productos.push_back({stoi(c[1]),desescapar(c[2]),stod(c[3]),stoi(c[4]),desescapar(c[5]),desescapar(c[6])});
        } else if (t=="CLI" && c.size()>=5) {
            clientes.push_back({stoi(c[1]),desescapar(c[2]),desescapar(c[3]),stod(c[4])});
        } else if (t=="PROV" && c.size()>=6) {
            proveedores.push_back({stoi(c[1]),desescapar(c[2]),desescapar(c[3]),desescapar(c[4]),stod(c[5])});
        } else if (t=="COMPRA" && c.size()>=7) {
            Compra x; x.id=stoll(c[1]); x.proveedorId=stoi(c[2]); x.total=stod(c[3]); x.pagado=(c[4]=="1"); x.fecha=desescapar(c[5]);
            compras.push_back(x); ca=&compras.back();
        } else if (t=="CITEM" && c.size()>=4 && ca) {
            ca->items.push_back({desescapar(c[1]),stoi(c[2]),stod(c[3])});
        } else if (t=="CANCHA" && c.size()>=4) {
            canchas.push_back({stoi(c[1]),desescapar(c[2]),stod(c[3])});
        } else if (t=="RESERVA" && c.size()>=12) {
            Reserva r; r.id=stoll(c[1]); r.canchaId=stoi(c[2]); r.fecha=desescapar(c[3]); r.hora=stoi(c[4]);
            r.duracion=stoi(c[5]); r.cliente=desescapar(c[6]); r.descuento=stod(c[7]); r.total=stod(c[8]);
            r.adelanto=stod(c[9]); r.pagado=(c[10]=="1"); r.cajaMovId=stoll(c[11]); reservas.push_back(r);
        } else if (t=="VENTA" && c.size()>=8) {
            Venta v; v.idVenta=stoll(c[1]); v.total=stod(c[2]); v.formaPago=desescapar(c[3]); v.clienteId=stoi(c[4]);
            v.clienteNombre=desescapar(c[5]); v.fecha=desescapar(c[6]); ventas.push_back(v); va=&ventas.back();
        } else if (t=="VITEM" && c.size()>=5 && va) {
            va->items.push_back({desescapar(c[1]),stod(c[2]),stoi(c[3]),stod(c[4])});
        } else if (t=="CAJA" && c.size()>=6) {
            movsCaja.push_back({stoll(c[1]),desescapar(c[2]),stod(c[3]),desescapar(c[4]),desescapar(c[5])});
        }
    }
}

void cargarDatosEjemplo() {
    productos = {
        {1,"Agua San Luis 625ml",1.50,48,"7751010001234","Bebidas"},
        {2,"Coca Cola 500ml",3.00,24,"7501055300006","Bebidas"},
        {3,"Galletas Oreo",2.50,36,"7622210003232","Galletas"},
        {4,"Chicles Halls",1.00,60,"0040000004096","Snack"},
        {5,"Papas Lays clasicas",3.50,18,"7501012004040","Snack"},
        {6,"Yogurt Gloria 120g",2.00,20,"7750016000789","Bebidas"},
        {7,"Jugo Pulp naranja",2.50,15,"7750016100456","Bebidas"},
        {8,"Pilas AA Duracell x2",5.00,30,"0041333045559","Otros"},
    };
    canchas = { {1,"Cancha Voley",40.0}, {2,"Cancha Futbol",60.0} };
    nextProductoId=9; nextClienteId=1; nextProveedorId=1; nextCanchaId=3; nextReservaId=1;
}

// Busquedas
Producto*  prodPorId(int id){ for(auto& p:productos) if(p.id==id) return &p; return nullptr; }
Producto*  prodPorCodigo(const string& c){ for(auto& p:productos) if(!p.codigo.empty()&&p.codigo==c) return &p; return nullptr; }
Cliente*   cliPorId(int id){ for(auto& c:clientes) if(c.id==id) return &c; return nullptr; }
Proveedor* provPorId(int id){ for(auto& p:proveedores) if(p.id==id) return &p; return nullptr; }
Cancha*    canchaPorId(int id){ for(auto& c:canchas) if(c.id==id) return &c; return nullptr; }
double totalCarrito(){ double t=0; for(auto& it:carrito) t+=it.precio*it.cantidad; return t; }

// ----------------------------------------------------------------------------
//  IDs de controles
// ----------------------------------------------------------------------------
enum {
    ID_NAV_BASE = 1100,   // 6 pestanas: 1100..1105
    ID_ADMIN = 1200,
    // Ventas
    ID_V_BUSCAR, ID_V_PRODLIST, ID_V_ADD, ID_V_CARTLIST, ID_V_QUITAR, ID_V_LIMPIAR,
    ID_V_TOTAL, ID_V_CLIENTE, ID_V_FIADO, ID_V_EFECTIVO, ID_V_VUELTO, ID_V_COBRAR,
    // Inventario
    ID_I_LIST, ID_I_NUEVO, ID_I_EDITAR, ID_I_ELIMINAR, ID_I_STOCK,
    // Clientes
    ID_C_LIST, ID_C_NUEVO, ID_C_COBRANZA,
    // Proveedores
    ID_P_LIST, ID_P_NUEVO, ID_P_EDITAR, ID_P_ELIMINAR, ID_P_PAGO, ID_P_COMPRA, ID_P_HIST,
    // Canchas
    ID_A_CLIST, ID_A_NUEVA, ID_A_EDITAR, ID_A_ELIMINAR, ID_A_RLIST, ID_A_RNUEVA, ID_A_REDITAR, ID_A_RELIMINAR,
    // Caja
    ID_K_INFO, ID_K_ING, ID_K_GAS, ID_K_RET, ID_K_CERRAR
};

// Estilos de boton (owner-draw)
enum BtnStyle { BS_PRIMARY, BS_SUCCESS, BS_GHOST, BS_DANGER, BS_NAV };
map<int,int> btnStyle;      // id -> BtnStyle
map<int,int> navIndex;      // id -> indice de pestana

// Handles
HWND hMain, hStatus, hAdminBtn, hNav[6];
HFONT hFont, hFontBold, hFontBig, hFontH1, hFontH2;
HBRUSH hbrBg, hbrCard;
int currentTab = 0;

// Ventas
HWND hVBuscar,hVProd,hVAdd,hVCart,hVQuitar,hVLimpiar,hVTotalLbl,hVTotal,hVClienteLbl,hVCliente,hVFiado,hVEfectivoLbl,hVEfectivo,hVVuelto,hVCobrar;
// Inventario
HWND hILabel,hIList,hINuevo,hIEditar,hIEliminar,hIStock;
// Clientes
HWND hCLabel,hCList,hCNuevo,hCCobranza;
// Proveedores
HWND hPLabel,hPList,hPNuevo,hPEditar,hPEliminar,hPPago,hPCompra,hPHistLabel,hPHist;
// Canchas
HWND hALabel,hAList,hANueva,hAEditar,hAEliminar,hARLabel,hARList,hARNueva,hAREditar,hAREliminar;
// Caja
HWND hKLabel,hKInfo,hKIng,hKGas,hKRet,hKCerrar;

// ----------------------------------------------------------------------------
//  Dialogo de entrada generico (modal)
// ----------------------------------------------------------------------------
struct InputField { wstring label; wstring value; };
struct InputCtx { vector<InputField>* fields; bool ok; vector<HWND> edits; };

LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    InputCtx* ctx = (InputCtx*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs=(CREATESTRUCT*)lp; ctx=(InputCtx*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA,(LONG_PTR)ctx);
        int y=16;
        for (size_t i=0;i<ctx->fields->size();++i){
            HWND lb=CreateWindowW(L"STATIC",(*ctx->fields)[i].label.c_str(),WS_CHILD|WS_VISIBLE,18,y,320,18,hwnd,nullptr,nullptr,nullptr);
            SendMessageW(lb,WM_SETFONT,(WPARAM)hFont,TRUE);
            HWND e=CreateWindowW(L"EDIT",(*ctx->fields)[i].value.c_str(),WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,18,y+20,320,26,hwnd,(HMENU)(INT_PTR)(2000+i),nullptr,nullptr);
            SendMessageW(e,WM_SETFONT,(WPARAM)hFont,TRUE);
            ctx->edits.push_back(e); y+=54;
        }
        HWND ok=CreateWindowW(L"BUTTON",L"Aceptar",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,138,y+6,100,32,hwnd,(HMENU)IDOK,nullptr,nullptr);
        HWND ca=CreateWindowW(L"BUTTON",L"Cancelar",WS_CHILD|WS_VISIBLE|WS_TABSTOP,244,y+6,100,32,hwnd,(HMENU)IDCANCEL,nullptr,nullptr);
        SendMessageW(ok,WM_SETFONT,(WPARAM)hFont,TRUE); SendMessageW(ca,WM_SETFONT,(WPARAM)hFont,TRUE);
        SetWindowPos(hwnd,nullptr,0,0,378,y+96,SWP_NOMOVE|SWP_NOZORDER);
        if(!ctx->edits.empty()) SetFocus(ctx->edits[0]);
        return 0;
    }
    case WM_CTLCOLORSTATIC: { HDC dc=(HDC)wp; SetBkColor(dc,C_BG); SetTextColor(dc,C_TEXT); return (LRESULT)hbrBg; }
    case WM_COMMAND:
        if (LOWORD(wp)==IDOK){
            for(size_t i=0;i<ctx->edits.size();++i){ wchar_t b[512]; GetWindowTextW(ctx->edits[i],b,512); (*ctx->fields)[i].value=b; }
            ctx->ok=true; DestroyWindow(hwnd); return 0;
        } else if (LOWORD(wp)==IDCANCEL){ ctx->ok=false; DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE: ctx->ok=false; DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

bool InputDialog(HWND parent, const wstring& title, vector<InputField>& fields) {
    static bool reg=false; HINSTANCE hi=GetModuleHandleW(nullptr);
    if(!reg){ WNDCLASSW wc={}; wc.lpfnWndProc=InputProc; wc.hInstance=hi; wc.lpszClassName=L"POSInputDlg";
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=hbrBg; RegisterClassW(&wc); reg=true; }
    InputCtx ctx; ctx.fields=&fields; ctx.ok=false;
    RECT pr; GetWindowRect(parent,&pr);
    EnableWindow(parent,FALSE);
    HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME,L"POSInputDlg",title.c_str(),WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
                pr.left+140,pr.top+90,378,300,parent,nullptr,hi,&ctx);
    MSG m;
    while(GetMessageW(&m,nullptr,0,0)){ if(m.message==WM_QUIT) break; if(IsDialogMessageW(dlg,&m)) continue; TranslateMessage(&m); DispatchMessageW(&m); }
    EnableWindow(parent,TRUE); SetForegroundWindow(parent);
    return ctx.ok;
}

double parseD(const wstring& w,double def=0){ try{return stod(w);}catch(...){return def;} }
int    parseI(const wstring& w,int def=0){ try{return stoi(w);}catch(...){return def;} }

// ----------------------------------------------------------------------------
//  ListView helpers
// ----------------------------------------------------------------------------
void lvAddCol(HWND lv,int idx,const wchar_t* txt,int w){ LVCOLUMNW c={}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; c.pszText=(LPWSTR)txt; c.cx=w; c.iSubItem=idx; ListView_InsertColumn(lv,idx,&c); }
int  lvAddRow(HWND lv,int row,const wstring& txt,LPARAM p){ LVITEMW it={}; it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=row; wstring t=txt; it.pszText=(LPWSTR)t.c_str(); it.lParam=p; return ListView_InsertItem(lv,&it); }
void lvSet(HWND lv,int row,int col,const wstring& txt){ wstring t=txt; ListView_SetItemText(lv,row,col,(LPWSTR)t.c_str()); }
int  lvSel(HWND lv){ return ListView_GetNextItem(lv,-1,LVNI_SELECTED); }
LPARAM lvParam(HWND lv,int row){ LVITEMW it={}; it.mask=LVIF_PARAM; it.iItem=row; ListView_GetItem(lv,&it); return it.lParam; }

void setStatus(const wstring& t){ SendMessageW(hStatus,SB_SETTEXT,0,(LPARAM)t.c_str()); }

// ----------------------------------------------------------------------------
//  Refrescos de vista
// ----------------------------------------------------------------------------
void refrescarProductosVenta(){
    wchar_t buf[128]; GetWindowTextW(hVBuscar,buf,128);
    string filtro=toU8(buf), q=aMin(filtro);
    ListView_DeleteAllItems(hVProd); int row=0;
    for(auto& p:productos){
        if(!q.empty() && aMin(p.nombre).find(q)==string::npos && p.codigo.find(filtro)==string::npos) continue;
        lvAddRow(hVProd,row,to_wstring(p.id),p.id);
        lvSet(hVProd,row,1,toW(p.nombre)); lvSet(hVProd,row,2,numW(p.precio));
        lvSet(hVProd,row,3,numW(p.stock)); lvSet(hVProd,row,4,toW(p.codigo)); row++;
    }
}
void refrescarCarrito(){
    ListView_DeleteAllItems(hVCart); int row=0;
    for(auto& it:carrito){
        lvAddRow(hVCart,row,toW(it.nombre),it.id);
        lvSet(hVCart,row,1,numW(it.cantidad)); lvSet(hVCart,row,2,numW(it.precio)); lvSet(hVCart,row,3,numW(it.precio*it.cantidad)); row++;
    }
    SetWindowTextW(hVTotal,dinero(totalCarrito()).c_str());
    wchar_t b[64]; GetWindowTextW(hVEfectivo,b,64); double efec=parseD(b,0),tot=totalCarrito();
    if(efec>=tot&&efec>0) SetWindowTextW(hVVuelto,(L"Vuelto: "+dinero(efec-tot)).c_str());
    else SetWindowTextW(hVVuelto,L"Vuelto: -");
}
void refrescarComboClientes(){
    SendMessageW(hVCliente,CB_RESETCONTENT,0,0);
    SendMessageW(hVCliente,CB_ADDSTRING,0,(LPARAM)L"Mostrador (contado)");
    SendMessageW(hVCliente,CB_SETITEMDATA,0,0);
    int idx=1;
    for(auto& c:clientes){ wstring e=toW(c.nombre); if(c.saldo>0) e+=L" (debe "+dinero(c.saldo)+L")";
        SendMessageW(hVCliente,CB_ADDSTRING,0,(LPARAM)e.c_str()); SendMessageW(hVCliente,CB_SETITEMDATA,idx,c.id); idx++; }
    SendMessageW(hVCliente,CB_SETCURSEL,0,0);
}
void refrescarInventario(){
    ListView_DeleteAllItems(hIList); int row=0;
    for(auto& p:productos){
        wstring est = p.stock<=0?L"SIN STOCK":(p.stock<=5?L"BAJO":L"OK");
        lvAddRow(hIList,row,to_wstring(p.id),p.id);
        lvSet(hIList,row,1,toW(p.nombre)); lvSet(hIList,row,2,numW(p.precio)); lvSet(hIList,row,3,numW(p.stock));
        lvSet(hIList,row,4,toW(p.codigo)); lvSet(hIList,row,5,toW(p.categoria)); lvSet(hIList,row,6,est); row++;
    }
}
void refrescarClientes(){
    ListView_DeleteAllItems(hCList); int row=0;
    for(auto& c:clientes){
        lvAddRow(hCList,row,to_wstring(c.id),c.id);
        lvSet(hCList,row,1,toW(c.nombre)); lvSet(hCList,row,2,toW(c.telefono));
        lvSet(hCList,row,3,dinero(c.saldo)); lvSet(hCList,row,4,c.saldo>0?L"Debe":L"Al dia"); row++;
    }
}
void refrescarProveedores(){
    ListView_DeleteAllItems(hPList); int row=0;
    for(auto& p:proveedores){
        lvAddRow(hPList,row,to_wstring(p.id),p.id);
        lvSet(hPList,row,1,toW(p.nombre)); lvSet(hPList,row,2,toW(p.telefono)); lvSet(hPList,row,3,toW(p.contacto));
        lvSet(hPList,row,4,dinero(p.saldo)); lvSet(hPList,row,5,p.saldo>0?L"Debemos":L"Al dia"); row++;
    }
    ListView_DeleteAllItems(hPHist); row=0;
    for(auto it=compras.rbegin(); it!=compras.rend(); ++it){
        Proveedor* pr=provPorId(it->proveedorId);
        wstring det; for(size_t i=0;i<it->items.size();++i){ if(i)det+=L", "; det+=toW(it->items[i].producto)+L" x"+to_wstring(it->items[i].cantidad); }
        lvAddRow(hPHist,row,it->fecha.size()>=10?toW(it->fecha.substr(0,10)):L"",0);
        lvSet(hPHist,row,1,pr?toW(pr->nombre):L"Proveedor"); lvSet(hPHist,row,2,det);
        lvSet(hPHist,row,3,dinero(it->total)); lvSet(hPHist,row,4,it->pagado?L"Pagado":L"A credito"); row++;
    }
}
void refrescarCanchas(){
    ListView_DeleteAllItems(hAList); int row=0;
    for(auto& c:canchas){ lvAddRow(hAList,row,to_wstring(c.id),c.id); lvSet(hAList,row,1,toW(c.nombre)); lvSet(hAList,row,2,dinero(c.precio)); row++; }
    // reservas ordenadas por fecha+hora
    vector<Reserva*> ord; for(auto& r:reservas) ord.push_back(&r);
    sort(ord.begin(),ord.end(),[](Reserva* a,Reserva* b){ if(a->fecha!=b->fecha) return a->fecha<b->fecha; return a->hora<b->hora; });
    ListView_DeleteAllItems(hARList); row=0;
    for(auto* r:ord){
        Cancha* c=canchaPorId(r->canchaId); double saldo=r->total-r->adelanto;
        wstring est = r->pagado?L"Pagado":((r->adelanto>0)?L"Adelanto":L"Pendiente");
        lvAddRow(hARList,row,toW(r->fecha),(LPARAM)r->id);
        lvSet(hARList,row,1,hhmm(r->hora)); lvSet(hARList,row,2,c?toW(c->nombre):L"Cancha");
        lvSet(hARList,row,3,toW(r->cliente)); lvSet(hARList,row,4,to_wstring(r->duracion)+L"h");
        lvSet(hARList,row,5,dinero(r->total)); lvSet(hARList,row,6,dinero(r->adelanto));
        lvSet(hARList,row,7,dinero(saldo)); lvSet(hARList,row,8,est); row++;
    }
}
void refrescarCaja(){
    double ing=0,gas=0,ret=0;
    for(auto& m:movsCaja){ if(m.tipo=="ingreso")ing+=m.monto; else if(m.tipo=="gasto")gas+=m.monto; else if(m.tipo=="retiro")ret+=m.monto; }
    double saldo=cajaSaldoInicial+ing-gas-ret;
    wstring s;
    s+=L" Saldo de apertura :  "+dinero(cajaSaldoInicial)+L"\r\n";
    s+=L" Ingresos          :  "+dinero(ing)+L"\r\n";
    s+=L" Gastos            :  "+dinero(gas)+L"\r\n";
    s+=L" Retiros           :  "+dinero(ret)+L"\r\n";
    s+=L" ----------------------------------------\r\n";
    s+=L" SALDO EN CAJA     :  "+dinero(saldo)+L"\r\n\r\n";
    s+=L" Ultimos movimientos:\r\n";
    int n=0;
    for(auto it=movsCaja.rbegin(); it!=movsCaja.rend()&&n<20; ++it,++n){
        wchar_t sg=(it->tipo=="ingreso")?L'+':L'-';
        wstring hora=it->fecha.size()>=16?toW(it->fecha.substr(11,5)):L"";
        s+=L"   "+hora+L"   "+sg+L" "+dinero(it->monto)+L"    "+toW(it->desc)+L"\r\n";
    }
    if(movsCaja.empty()) s+=L"   (sin movimientos)\r\n";
    SetWindowTextW(hKInfo,s.c_str());
}

// ----------------------------------------------------------------------------
//  Logica de negocio
// ----------------------------------------------------------------------------
void agregarAlCarritoPorId(int id){
    Producto* p=prodPorId(id); if(!p) return;
    if(p->stock<=0){ setStatus(L"Sin stock: "+toW(p->nombre)); return; }
    for(auto& it:carrito){ if(it.id==id){ if(it.cantidad>=p->stock){ setStatus(L"Stock maximo: "+toW(p->nombre)); return; } it.cantidad++; refrescarCarrito(); setStatus(L"+1 "+toW(p->nombre)); return; } }
    carrito.push_back({p->id,p->nombre,p->precio,1}); refrescarCarrito(); setStatus(L"Agregado: "+toW(p->nombre));
}
void ventaBuscarYAgregar(){
    wchar_t buf[128]; GetWindowTextW(hVBuscar,buf,128); string entrada=toU8(buf);
    if(entrada.empty()) return;
    Producto* p=prodPorCodigo(entrada);
    if(!p){ string q=aMin(entrada); for(auto& x:productos) if(aMin(x.nombre)==q){ p=&x; break; } }
    if(!p){ string q=aMin(entrada); vector<Producto*> m; for(auto& x:productos) if(aMin(x.nombre).find(q)!=string::npos) m.push_back(&x); if(m.size()==1) p=m[0]; }
    if(p){ agregarAlCarritoPorId(p->id); SetWindowTextW(hVBuscar,L""); refrescarProductosVenta(); }
    else setStatus(L"No encontrado: "+toW(entrada));
}
void cobrar(){
    if(carrito.empty()){ setStatus(L"El carrito esta vacio"); return; }
    double total=totalCarrito();
    int sel=(int)SendMessageW(hVCliente,CB_GETCURSEL,0,0);
    int clienteId=(int)SendMessageW(hVCliente,CB_GETITEMDATA,sel,0);
    Cliente* cliente=clienteId>0?cliPorId(clienteId):nullptr;
    bool fiado=(SendMessageW(hVFiado,BM_GETCHECK,0,0)==BST_CHECKED)&&cliente;
    if(!fiado){
        wchar_t b[64]; GetWindowTextW(hVEfectivo,b,64); double efec=parseD(b,0);
        if(efec>0&&efec<total){ MessageBoxW(hMain,L"Efectivo insuficiente.",L"Cobro",MB_ICONWARNING); return; }
        wstring msg=L"Total: "+dinero(total);
        if(efec>0) msg+=L"\nEfectivo: "+dinero(efec)+L"\nVUELTO: "+dinero(efec-total);
        msg+=L"\n\n¿Confirmar venta al contado?";
        if(MessageBoxW(hMain,msg.c_str(),L"Confirmar cobro",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
    } else {
        wstring msg=L"Venta a cuenta corriente de "+toW(cliente->nombre)+L"\nTotal: "+dinero(total)+L"\n\n¿Confirmar?";
        if(MessageBoxW(hMain,msg.c_str(),L"Confirmar fiado",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
    }
    Venta v; v.idVenta=(long long)time(nullptr); v.total=total; v.formaPago=fiado?"cuenta corriente":"efectivo";
    v.clienteId=cliente?cliente->id:0; v.clienteNombre=cliente?cliente->nombre:""; v.fecha=fechaHoraActual();
    for(auto& it:carrito){ Producto* p=prodPorId(it.id); if(p) p->stock=max(0,p->stock-it.cantidad); v.items.push_back({it.nombre,it.precio,it.cantidad,0.0}); }
    ventas.push_back(v);
    if(fiado&&cliente){ cliente->saldo+=total; setStatus(L"Venta fiada. Saldo "+toW(cliente->nombre)+L": "+dinero(cliente->saldo)); }
    else { movsCaja.push_back({(long long)time(nullptr),"ingreso",total,"Venta",fechaHoraActual()}); setStatus(L"Venta registrada: "+dinero(total)); }
    carrito.clear(); SetWindowTextW(hVEfectivo,L""); SendMessageW(hVFiado,BM_SETCHECK,BST_UNCHECKED,0); SendMessageW(hVCliente,CB_SETCURSEL,0,0);
    refrescarCarrito(); refrescarProductosVenta(); refrescarComboClientes(); guardarDatos();
}
bool pedirAdmin(){
    if(esAdmin) return true;
    vector<InputField> f={{L"Clave de administrador:",L""}};
    if(!InputDialog(hMain,L"Ingresar como administrador",f)) return false;
    if(toU8(f[0].value)==ADMIN_PASS){ esAdmin=true; SetWindowTextW(hAdminBtn,L"Cerrar sesion admin"); InvalidateRect(hAdminBtn,nullptr,TRUE); setStatus(L"Sesion de administrador iniciada"); return true; }
    MessageBoxW(hMain,L"Clave incorrecta.",L"Administrador",MB_ICONERROR); return false;
}

// ---- Inventario ----
void invNuevo(){
    if(!pedirAdmin()) return;
    vector<InputField> f={{L"Nombre:",L""},{L"Precio:",L""},{L"Stock inicial:",L""},{L"Codigo de barras:",L""},{L"Categoria:",L""}};
    if(!InputDialog(hMain,L"Nuevo producto",f)) return;
    if(f[0].value.empty()){ MessageBoxW(hMain,L"El nombre es obligatorio.",L"Inventario",MB_ICONWARNING); return; }
    productos.push_back({nextProductoId++,toU8(f[0].value),parseD(f[1].value),parseI(f[2].value),toU8(f[3].value),toU8(f[4].value)});
    guardarDatos(); refrescarInventario(); refrescarProductosVenta(); setStatus(L"Producto agregado");
}
void invEditar(){
    if(!pedirAdmin()) return;
    int row=lvSel(hIList); if(row<0){ MessageBoxW(hMain,L"Selecciona un producto.",L"Inventario",MB_ICONINFORMATION); return; }
    Producto* p=prodPorId((int)lvParam(hIList,row)); if(!p) return;
    vector<InputField> f={{L"Nombre:",toW(p->nombre)},{L"Precio:",numW(p->precio)},{L"Stock:",numW(p->stock)},{L"Codigo de barras:",toW(p->codigo)},{L"Categoria:",toW(p->categoria)}};
    if(!InputDialog(hMain,L"Editar producto",f)) return;
    p->nombre=toU8(f[0].value); p->precio=parseD(f[1].value,p->precio); p->stock=parseI(f[2].value,p->stock); p->codigo=toU8(f[3].value); p->categoria=toU8(f[4].value);
    guardarDatos(); refrescarInventario(); refrescarProductosVenta(); setStatus(L"Producto actualizado");
}
void invEliminar(){
    if(!pedirAdmin()) return;
    int row=lvSel(hIList); if(row<0){ MessageBoxW(hMain,L"Selecciona un producto.",L"Inventario",MB_ICONINFORMATION); return; }
    int id=(int)lvParam(hIList,row); Producto* p=prodPorId(id); if(!p) return;
    if(MessageBoxW(hMain,(L"¿Eliminar \""+toW(p->nombre)+L"\"?").c_str(),L"Eliminar",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    productos.erase(remove_if(productos.begin(),productos.end(),[id](const Producto& x){return x.id==id;}),productos.end());
    guardarDatos(); refrescarInventario(); refrescarProductosVenta(); setStatus(L"Producto eliminado");
}
void invStock(){
    if(!pedirAdmin()) return;
    int row=lvSel(hIList); if(row<0){ MessageBoxW(hMain,L"Selecciona un producto.",L"Inventario",MB_ICONINFORMATION); return; }
    Producto* p=prodPorId((int)lvParam(hIList,row)); if(!p) return;
    vector<InputField> f={{L"Cantidad a ingresar (negativo para restar):",L""}};
    if(!InputDialog(hMain,L"Ingreso de mercaderia - "+toW(p->nombre),f)) return;
    p->stock=max(0,p->stock+parseI(f[0].value)); guardarDatos(); refrescarInventario(); refrescarProductosVenta(); setStatus(L"Stock: "+numW(p->stock));
}

// ---- Clientes ----
void cliNuevo(){
    vector<InputField> f={{L"Nombre:",L""},{L"Telefono:",L""}};
    if(!InputDialog(hMain,L"Nuevo cliente",f)) return;
    if(f[0].value.empty()){ MessageBoxW(hMain,L"El nombre es obligatorio.",L"Clientes",MB_ICONWARNING); return; }
    clientes.push_back({nextClienteId++,toU8(f[0].value),toU8(f[1].value),0.0});
    guardarDatos(); refrescarClientes(); refrescarComboClientes(); setStatus(L"Cliente agregado");
}
void cliCobranza(){
    int row=lvSel(hCList); if(row<0){ MessageBoxW(hMain,L"Selecciona un cliente.",L"Clientes",MB_ICONINFORMATION); return; }
    Cliente* c=cliPorId((int)lvParam(hCList,row)); if(!c) return;
    if(c->saldo<=0){ MessageBoxW(hMain,(toW(c->nombre)+L" no tiene deuda.").c_str(),L"Cobranza",MB_ICONINFORMATION); return; }
    vector<InputField> f={{L"Deuda: "+dinero(c->saldo)+L"  -  Monto a cobrar:",L""}};
    if(!InputDialog(hMain,L"Registrar cobranza",f)) return;
    double m=parseD(f[0].value); if(m<=0){ MessageBoxW(hMain,L"Monto invalido.",L"Cobranza",MB_ICONWARNING); return; }
    c->saldo=max(0.0,c->saldo-m);
    movsCaja.push_back({(long long)time(nullptr),"ingreso",m,"Cobranza - "+c->nombre,fechaHoraActual()});
    guardarDatos(); refrescarClientes(); refrescarComboClientes(); setStatus(L"Pago registrado. Saldo: "+dinero(c->saldo));
}

// ---- Proveedores ----
void provNuevo(){
    vector<InputField> f={{L"Nombre / Razon social:",L""},{L"Telefono:",L""},{L"Contacto:",L""}};
    if(!InputDialog(hMain,L"Nuevo proveedor",f)) return;
    if(f[0].value.empty()){ MessageBoxW(hMain,L"El nombre es obligatorio.",L"Proveedores",MB_ICONWARNING); return; }
    proveedores.push_back({nextProveedorId++,toU8(f[0].value),toU8(f[1].value),toU8(f[2].value),0.0});
    guardarDatos(); refrescarProveedores(); setStatus(L"Proveedor agregado");
}
void provEditar(){
    int row=lvSel(hPList); if(row<0){ MessageBoxW(hMain,L"Selecciona un proveedor.",L"Proveedores",MB_ICONINFORMATION); return; }
    Proveedor* p=provPorId((int)lvParam(hPList,row)); if(!p) return;
    vector<InputField> f={{L"Nombre:",toW(p->nombre)},{L"Telefono:",toW(p->telefono)},{L"Contacto:",toW(p->contacto)}};
    if(!InputDialog(hMain,L"Editar proveedor",f)) return;
    p->nombre=toU8(f[0].value); p->telefono=toU8(f[1].value); p->contacto=toU8(f[2].value);
    guardarDatos(); refrescarProveedores(); setStatus(L"Proveedor actualizado");
}
void provEliminar(){
    if(!pedirAdmin()) return;
    int row=lvSel(hPList); if(row<0){ MessageBoxW(hMain,L"Selecciona un proveedor.",L"Proveedores",MB_ICONINFORMATION); return; }
    int id=(int)lvParam(hPList,row); Proveedor* p=provPorId(id); if(!p) return;
    if(MessageBoxW(hMain,(L"¿Eliminar el proveedor \""+toW(p->nombre)+L"\"? El historial de compras se conserva.").c_str(),L"Eliminar",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    proveedores.erase(remove_if(proveedores.begin(),proveedores.end(),[id](const Proveedor& x){return x.id==id;}),proveedores.end());
    guardarDatos(); refrescarProveedores(); setStatus(L"Proveedor eliminado");
}
void provPago(){
    int row=lvSel(hPList); if(row<0){ MessageBoxW(hMain,L"Selecciona un proveedor.",L"Proveedores",MB_ICONINFORMATION); return; }
    Proveedor* p=provPorId((int)lvParam(hPList,row)); if(!p) return;
    if(p->saldo<=0){ MessageBoxW(hMain,(L"No debemos nada a "+toW(p->nombre)).c_str(),L"Pago",MB_ICONINFORMATION); return; }
    vector<InputField> f={{L"Deuda: "+dinero(p->saldo)+L"  -  Monto a pagar:",L""}};
    if(!InputDialog(hMain,L"Pago a proveedor",f)) return;
    double m=parseD(f[0].value); if(m<=0){ MessageBoxW(hMain,L"Monto invalido.",L"Pago",MB_ICONWARNING); return; }
    p->saldo=max(0.0,p->saldo-m);
    movsCaja.push_back({(long long)time(nullptr),"gasto",m,"Pago a proveedor - "+p->nombre,fechaHoraActual()});
    guardarDatos(); refrescarProveedores(); setStatus(L"Pago a proveedor registrado. Deuda: "+dinero(p->saldo));
}
void provCompra(){
    int row=lvSel(hPList); if(row<0){ MessageBoxW(hMain,L"Selecciona el proveedor primero.",L"Compra",MB_ICONINFORMATION); return; }
    Proveedor* p=provPorId((int)lvParam(hPList,row)); if(!p) return;
    vector<InputField> f={
        {L"Producto (nombre o codigo de barras):",L""},{L"Cantidad:",L""},{L"Costo unitario:",L""},
        {L"Forma de pago (credito / pagado):",L"credito"}};
    if(!InputDialog(hMain,L"Registrar compra a "+toW(p->nombre),f)) return;
    string busca=toU8(f[0].value);
    Producto* prod=prodPorCodigo(busca);
    if(!prod){ string q=aMin(busca); for(auto& x:productos) if(aMin(x.nombre)==q){ prod=&x; break; } }
    if(!prod){ string q=aMin(busca); vector<Producto*> m; for(auto& x:productos) if(aMin(x.nombre).find(q)!=string::npos) m.push_back(&x); if(m.size()==1) prod=m[0]; }
    if(!prod){ MessageBoxW(hMain,L"Producto no encontrado. Registralo primero en Inventario.",L"Compra",MB_ICONWARNING); return; }
    int cant=parseI(f[1].value); double costo=parseD(f[2].value);
    if(cant<=0||costo<0){ MessageBoxW(hMain,L"Cantidad y costo invalidos.",L"Compra",MB_ICONWARNING); return; }
    bool pagado = aMin(toU8(f[3].value))=="pagado";
    double total=cant*costo;
    prod->stock+=cant;
    if(pagado) movsCaja.push_back({(long long)time(nullptr),"gasto",total,"Compra a "+p->nombre,fechaHoraActual()});
    else p->saldo+=total;
    Compra c; c.id=(long long)time(nullptr); c.proveedorId=p->id; c.items.push_back({prod->nombre,cant,costo}); c.total=total; c.pagado=pagado; c.fecha=fechaHoraActual();
    compras.push_back(c);
    guardarDatos(); refrescarProveedores(); refrescarInventario(); refrescarProductosVenta();
    setStatus(L"Compra registrada. Stock de "+toW(prod->nombre)+L": "+numW(prod->stock));
}

// ---- Canchas ----
void canchaNueva(){
    vector<InputField> f={{L"Nombre:",L""},{L"Precio por hora:",L""}};
    if(!InputDialog(hMain,L"Nueva cancha",f)) return;
    if(f[0].value.empty()){ MessageBoxW(hMain,L"El nombre es obligatorio.",L"Canchas",MB_ICONWARNING); return; }
    canchas.push_back({nextCanchaId++,toU8(f[0].value),parseD(f[1].value)});
    guardarDatos(); refrescarCanchas(); setStatus(L"Cancha agregada");
}
void canchaEditar(){
    int row=lvSel(hAList); if(row<0){ MessageBoxW(hMain,L"Selecciona una cancha.",L"Canchas",MB_ICONINFORMATION); return; }
    Cancha* c=canchaPorId((int)lvParam(hAList,row)); if(!c) return;
    vector<InputField> f={{L"Nombre:",toW(c->nombre)},{L"Precio por hora:",numW(c->precio)}};
    if(!InputDialog(hMain,L"Editar cancha",f)) return;
    c->nombre=toU8(f[0].value); c->precio=parseD(f[1].value,c->precio);
    guardarDatos(); refrescarCanchas(); setStatus(L"Cancha actualizada");
}
void canchaEliminar(){
    if(!pedirAdmin()) return;
    int row=lvSel(hAList); if(row<0){ MessageBoxW(hMain,L"Selecciona una cancha.",L"Canchas",MB_ICONINFORMATION); return; }
    int id=(int)lvParam(hAList,row); Cancha* c=canchaPorId(id); if(!c) return;
    if(MessageBoxW(hMain,(L"¿Eliminar \""+toW(c->nombre)+L"\"? Tambien se borran sus reservas.").c_str(),L"Eliminar",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    canchas.erase(remove_if(canchas.begin(),canchas.end(),[id](const Cancha& x){return x.id==id;}),canchas.end());
    reservas.erase(remove_if(reservas.begin(),reservas.end(),[id](const Reserva& r){return r.canchaId==id;}),reservas.end());
    guardarDatos(); refrescarCanchas(); setStatus(L"Cancha eliminada");
}
void sincronizarCajaReserva(Reserva& r){
    if(r.cajaMovId) movsCaja.erase(remove_if(movsCaja.begin(),movsCaja.end(),[&](const MovCaja& m){return m.id==r.cajaMovId;}),movsCaja.end());
    r.cajaMovId=0;
    if(r.adelanto>0){
        Cancha* c=canchaPorId(r.canchaId);
        string desc="Alquiler "+(c?c->nombre:"cancha")+(r.cliente.empty()?"":" - "+r.cliente)+(r.pagado?"":" (adelanto)");
        long long id=(long long)time(nullptr)*1000+(long long)(reservas.size()+1);
        movsCaja.push_back({id,"ingreso",r.adelanto,desc,fechaHoraActual()});
        r.cajaMovId=id;
    }
}
bool horarioOcupado(int canchaId,const string& fecha,int hora,int dur,long long excepto){
    for(auto& r:reservas){ if(r.canchaId==canchaId && r.fecha==fecha && r.id!=excepto && hora < r.hora+r.duracion && hora+dur > r.hora) return true; }
    return false;
}
void reservaGuardar(Reserva* edit,int canchaId){
    Cancha* c=canchaPorId(canchaId); if(!c){ MessageBoxW(hMain,L"Cancha invalida.",L"Reserva",MB_ICONWARNING); return; }
    vector<InputField> f={
        {L"Fecha (AAAA-MM-DD):", edit?toW(edit->fecha):toW(fechaHoy())},
        {L"Hora inicio (8 a 22):", edit?to_wstring(edit->hora):L"8"},
        {L"Duracion (horas):", edit?to_wstring(edit->duracion):L"1"},
        {L"Cliente:", edit?toW(edit->cliente):L""},
        {L"Descuento:", edit?numW(edit->descuento):L"0"},
        {L"Adelanto cobrado:", edit?numW(edit->adelanto):L"0"}};
    if(!InputDialog(hMain,edit?L"Editar reserva":L"Nueva reserva - "+toW(c->nombre),f)) return;
    string fecha=toU8(f[0].value); int hora=parseI(f[1].value,8); int dur=max(1,parseI(f[2].value,1));
    string cliente=toU8(f[3].value); double desc=parseD(f[4].value);
    if(fecha.empty()||cliente.empty()){ MessageBoxW(hMain,L"Completa fecha y cliente.",L"Reserva",MB_ICONWARNING); return; }
    double total=max(0.0, c->precio*dur - desc);
    double adel=parseD(f[5].value); if(adel>total) adel=total;
    bool pagado = total>0 && adel>=total;
    long long excepto = edit?edit->id:0;
    if(horarioOcupado(canchaId,fecha,hora,dur,excepto)){ MessageBoxW(hMain,L"Ese horario ya esta reservado en esa cancha.",L"Reserva",MB_ICONWARNING); return; }
    if(edit){
        edit->canchaId=canchaId; edit->fecha=fecha; edit->hora=hora; edit->duracion=dur; edit->cliente=cliente;
        edit->descuento=desc; edit->total=total; edit->adelanto=adel; edit->pagado=pagado;
        sincronizarCajaReserva(*edit); setStatus(L"Reserva actualizada");
    } else {
        Reserva r; r.id=nextReservaId++; r.canchaId=canchaId; r.fecha=fecha; r.hora=hora; r.duracion=dur; r.cliente=cliente;
        r.descuento=desc; r.total=total; r.adelanto=adel; r.pagado=pagado; r.cajaMovId=0;
        sincronizarCajaReserva(r); reservas.push_back(r); setStatus(L"Reserva creada");
    }
    guardarDatos(); refrescarCanchas();
}
void reservaNueva(){
    int row=lvSel(hAList); if(row<0){ MessageBoxW(hMain,L"Selecciona la cancha primero (lista de la izquierda).",L"Reserva",MB_ICONINFORMATION); return; }
    reservaGuardar(nullptr,(int)lvParam(hAList,row));
}
void reservaEditar(){
    int row=lvSel(hARList); if(row<0){ MessageBoxW(hMain,L"Selecciona una reserva.",L"Reserva",MB_ICONINFORMATION); return; }
    long long id=(long long)lvParam(hARList,row);
    for(auto& r:reservas) if(r.id==id){ reservaGuardar(&r,r.canchaId); return; }
}
void reservaEliminar(){
    int row=lvSel(hARList); if(row<0){ MessageBoxW(hMain,L"Selecciona una reserva.",L"Reserva",MB_ICONINFORMATION); return; }
    long long id=(long long)lvParam(hARList,row);
    if(MessageBoxW(hMain,L"¿Eliminar esta reserva?",L"Eliminar",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    for(auto& r:reservas) if(r.id==id && r.cajaMovId) movsCaja.erase(remove_if(movsCaja.begin(),movsCaja.end(),[&](const MovCaja& m){return m.id==r.cajaMovId;}),movsCaja.end());
    reservas.erase(remove_if(reservas.begin(),reservas.end(),[id](const Reserva& r){return r.id==id;}),reservas.end());
    guardarDatos(); refrescarCanchas(); setStatus(L"Reserva eliminada");
}

// ---- Caja ----
void cajaMov(const string& tipo){
    vector<InputField> f={{L"Monto:",L""},{L"Descripcion:",L""}};
    if(!InputDialog(hMain,toW("Registrar "+tipo),f)) return;
    double m=parseD(f[0].value); if(m<=0){ MessageBoxW(hMain,L"Monto invalido.",L"Caja",MB_ICONWARNING); return; }
    string d=toU8(f[1].value); if(d.empty()) d=tipo;
    movsCaja.push_back({(long long)time(nullptr),tipo,m,d,fechaHoraActual()});
    guardarDatos(); refrescarCaja(); setStatus(L"Movimiento registrado");
}
void cajaCerrar(){
    double ing=0,gas=0,ret=0; for(auto& m:movsCaja){ if(m.tipo=="ingreso")ing+=m.monto; else if(m.tipo=="gasto")gas+=m.monto; else if(m.tipo=="retiro")ret+=m.monto; }
    double saldo=cajaSaldoInicial+ing-gas-ret;
    if(MessageBoxW(hMain,(L"Cerrar caja con saldo "+dinero(saldo)+L"?\nEl saldo se trasladara a la nueva sesion.").c_str(),L"Cerrar caja",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
    cajaSaldoInicial=saldo; movsCaja.clear(); guardarDatos(); refrescarCaja(); setStatus(L"Caja cerrada. Saldo trasladado: "+dinero(saldo));
}

// ----------------------------------------------------------------------------
//  Mostrar/ocultar segun pestana
// ----------------------------------------------------------------------------
void mostrar(HWND h,bool v){ ShowWindow(h,v?SW_SHOW:SW_HIDE); }
void aplicarTab(int tab){
    currentTab=tab;
    bool ve=(tab==0),in=(tab==1),cl=(tab==2),pr=(tab==3),ca=(tab==4),kj=(tab==5);
    HWND vs[]={hVBuscar,hVProd,hVAdd,hVCart,hVQuitar,hVLimpiar,hVTotalLbl,hVTotal,hVClienteLbl,hVCliente,hVFiado,hVEfectivoLbl,hVEfectivo,hVVuelto,hVCobrar}; for(HWND h:vs) mostrar(h,ve);
    HWND is[]={hILabel,hIList,hINuevo,hIEditar,hIEliminar,hIStock}; for(HWND h:is) mostrar(h,in);
    HWND cs[]={hCLabel,hCList,hCNuevo,hCCobranza}; for(HWND h:cs) mostrar(h,cl);
    HWND ps[]={hPLabel,hPList,hPNuevo,hPEditar,hPEliminar,hPPago,hPCompra,hPHistLabel,hPHist}; for(HWND h:ps) mostrar(h,pr);
    HWND as[]={hALabel,hAList,hANueva,hAEditar,hAEliminar,hARLabel,hARList,hARNueva,hAREditar,hAREliminar}; for(HWND h:as) mostrar(h,ca);
    HWND ks[]={hKLabel,hKInfo,hKIng,hKGas,hKRet,hKCerrar}; for(HWND h:ks) mostrar(h,kj);
    if(ve){ refrescarProductosVenta(); refrescarCarrito(); refrescarComboClientes(); }
    if(in) refrescarInventario();
    if(cl) refrescarClientes();
    if(pr) refrescarProveedores();
    if(ca) refrescarCanchas();
    if(kj) refrescarCaja();
    for(int i=0;i<6;++i) InvalidateRect(hNav[i],nullptr,TRUE);
}

// ----------------------------------------------------------------------------
//  Dibujo de botones owner-draw (aspecto moderno)
// ----------------------------------------------------------------------------
void fillRound(HDC dc,RECT rc,COLORREF fill,COLORREF border){
    HBRUSH br=CreateSolidBrush(fill); HPEN pen=CreatePen(PS_SOLID,1,border);
    HGDIOBJ ob=SelectObject(dc,br),op=SelectObject(dc,pen);
    RoundRect(dc,rc.left,rc.top,rc.right,rc.bottom,12,12);
    SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(br); DeleteObject(pen);
}
void drawButton(DRAWITEMSTRUCT* dis){
    int id=dis->CtlID; HDC dc=dis->hDC; RECT rc=dis->rcItem;
    bool pressed=(dis->itemState&ODS_SELECTED)!=0;
    wchar_t txt[128]=L""; GetWindowTextW(dis->hwndItem,txt,128);
    COLORREF bg,tx,bd; HFONT fnt=hFontBold;
    if(navIndex.count(id)){
        bool active=(navIndex[id]==currentTab);
        bg = active? C_ACCENT : C_TABIDLE; tx = active? RGB(255,255,255):C_MUTED; bd=bg;
    } else {
        int st = btnStyle.count(id)?btnStyle[id]:BS_GHOST;
        switch(st){
            case BS_PRIMARY: bg=pressed?C_ACCENT_D:C_ACCENT; tx=RGB(255,255,255); bd=bg; break;
            case BS_SUCCESS: bg=pressed?C_SUCCESS_D:C_SUCCESS; tx=RGB(255,255,255); bd=bg; fnt=hFontBig; break;
            case BS_DANGER:  bg=pressed?RGB(170,45,40):C_DANGER; tx=RGB(255,255,255); bd=bg; break;
            default:         bg=pressed?RGB(238,240,243):C_CARD; tx=C_TEXT; bd=C_BORDER; break;
        }
    }
    fillRound(dc,rc,bg,bd);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,tx);
    HGDIOBJ of=SelectObject(dc,fnt);
    DrawTextW(dc,txt,-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    SelectObject(dc,of);
}

// ----------------------------------------------------------------------------
//  Crear controles
// ----------------------------------------------------------------------------
HWND mkBtn(int id,const wchar_t* txt,int x,int y,int w,int h,int style){
    HWND b=CreateWindowW(L"BUTTON",txt,WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,x,y,w,h,hMain,(HMENU)(INT_PTR)id,nullptr,nullptr);
    btnStyle[id]=style; return b;
}
HWND mkStatic(const wchar_t* txt,int x,int y,int w,int h,HFONT font=nullptr){
    HWND s=CreateWindowW(L"STATIC",txt,WS_CHILD,x,y,w,h,hMain,nullptr,nullptr,nullptr);
    SendMessageW(s,WM_SETFONT,(WPARAM)(font?font:hFont),TRUE); return s;
}
HWND mkEdit(int id,int x,int y,int w,int h,DWORD extra=0){
    HWND e=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_BORDER|WS_TABSTOP|extra,x,y,w,h,hMain,(HMENU)(INT_PTR)id,nullptr,nullptr);
    SendMessageW(e,WM_SETFONT,(WPARAM)hFont,TRUE); return e;
}
HWND mkList(int id,int x,int y,int w,int h){
    HWND lv=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,x,y,w,h,hMain,(HMENU)(INT_PTR)id,nullptr,nullptr);
    ListView_SetExtendedListViewStyle(lv,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(lv,C_CARD); ListView_SetTextBkColor(lv,C_CARD); ListView_SetTextColor(lv,C_TEXT);
    SendMessageW(lv,WM_SETFONT,(WPARAM)hFont,TRUE); return lv;
}

void crearControles(){
    int T=118;   // top del area de contenido
    int W=1012;  // ancho util

    // ---- VENTAS ----
    mkStatic(L"Buscar / codigo de barras:",16,T+4,220,20,hFontBold);
    hVBuscar=mkEdit(ID_V_BUSCAR,240,T+2,290,26);
    hVAdd   =mkBtn(ID_V_ADD,L"Agregar al carrito",540,T,170,30,BS_PRIMARY);
    hVProd=mkList(ID_V_PRODLIST,16,T+42,494,392);
    lvAddCol(hVProd,0,L"ID",40); lvAddCol(hVProd,1,L"Producto",212); lvAddCol(hVProd,2,L"Precio",70); lvAddCol(hVProd,3,L"Stock",55); lvAddCol(hVProd,4,L"Codigo",100);
    mkStatic(L"CARRITO",528,T+42,200,20,hFontH2);
    hVCart=mkList(ID_V_CARTLIST,528,T+66,500,236);
    lvAddCol(hVCart,0,L"Producto",228); lvAddCol(hVCart,1,L"Cant",55); lvAddCol(hVCart,2,L"P.Unit",90); lvAddCol(hVCart,3,L"Subtotal",100);
    hVQuitar =mkBtn(ID_V_QUITAR,L"Quitar linea",528,T+308,150,28,BS_GHOST);
    hVLimpiar=mkBtn(ID_V_LIMPIAR,L"Limpiar carrito",686,T+308,150,28,BS_GHOST);
    hVTotalLbl=mkStatic(L"TOTAL:",528,T+346,110,30,hFontH1);
    hVTotal   =mkStatic(L"S/ 0.00",645,T+346,250,30,hFontH1);
    hVClienteLbl=mkStatic(L"Cliente:",528,T+388,60,20);
    hVCliente=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,592,T+384,280,220,hMain,(HMENU)ID_V_CLIENTE,nullptr,nullptr);
    SendMessageW(hVCliente,WM_SETFONT,(WPARAM)hFont,TRUE);
    hVFiado=CreateWindowW(L"BUTTON",L"Fiado",WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX,882,T+386,140,22,hMain,(HMENU)ID_V_FIADO,nullptr,nullptr);
    SendMessageW(hVFiado,WM_SETFONT,(WPARAM)hFont,TRUE);
    hVEfectivoLbl=mkStatic(L"Efectivo S/:",528,T+422,80,20);
    hVEfectivo=mkEdit(ID_V_EFECTIVO,612,T+420,120,26);
    hVVuelto=mkStatic(L"Vuelto: -",748,T+422,280,20,hFontBold);
    hVCobrar=mkBtn(ID_V_COBRAR,L"COBRAR  (F2)",528,T+452,500,44,BS_SUCCESS);

    // ---- INVENTARIO ----
    hILabel=mkStatic(L"Inventario - stock actual",16,T+2,300,22,hFontH2);
    hIList=mkList(ID_I_LIST,16,T+30,W,414);
    lvAddCol(hIList,0,L"ID",40); lvAddCol(hIList,1,L"Producto",260); lvAddCol(hIList,2,L"Precio",80); lvAddCol(hIList,3,L"Stock",70); lvAddCol(hIList,4,L"Codigo",160); lvAddCol(hIList,5,L"Categoria",150); lvAddCol(hIList,6,L"Estado",100);
    hINuevo   =mkBtn(ID_I_NUEVO,L"+ Nuevo producto",16,T+452,160,32,BS_PRIMARY);
    hIEditar  =mkBtn(ID_I_EDITAR,L"Editar",186,T+452,110,32,BS_GHOST);
    hIEliminar=mkBtn(ID_I_ELIMINAR,L"Eliminar",306,T+452,110,32,BS_DANGER);
    hIStock   =mkBtn(ID_I_STOCK,L"Ingresar mercaderia",426,T+452,180,32,BS_GHOST);

    // ---- CLIENTES ----
    hCLabel=mkStatic(L"Clientes y cuenta corriente",16,T+2,320,22,hFontH2);
    hCList=mkList(ID_C_LIST,16,T+30,W,414);
    lvAddCol(hCList,0,L"ID",40); lvAddCol(hCList,1,L"Nombre",300); lvAddCol(hCList,2,L"Telefono",180); lvAddCol(hCList,3,L"Saldo/Deuda",150); lvAddCol(hCList,4,L"Estado",120);
    hCNuevo   =mkBtn(ID_C_NUEVO,L"+ Nuevo cliente",16,T+452,160,32,BS_PRIMARY);
    hCCobranza=mkBtn(ID_C_COBRANZA,L"Registrar cobranza",186,T+452,190,32,BS_SUCCESS);

    // ---- PROVEEDORES ----
    hPLabel=mkStatic(L"Proveedores",16,T+2,300,22,hFontH2);
    hPList=mkList(ID_P_LIST,16,T+30,W,230);
    lvAddCol(hPList,0,L"ID",40); lvAddCol(hPList,1,L"Nombre",260); lvAddCol(hPList,2,L"Telefono",150); lvAddCol(hPList,3,L"Contacto",180); lvAddCol(hPList,4,L"Deuda",130); lvAddCol(hPList,5,L"Estado",110);
    hPNuevo   =mkBtn(ID_P_NUEVO,L"+ Proveedor",16,T+268,140,30,BS_PRIMARY);
    hPEditar  =mkBtn(ID_P_EDITAR,L"Editar",166,T+268,90,30,BS_GHOST);
    hPEliminar=mkBtn(ID_P_ELIMINAR,L"Eliminar",266,T+268,90,30,BS_DANGER);
    hPPago    =mkBtn(ID_P_PAGO,L"Registrar pago",366,T+268,150,30,BS_SUCCESS);
    hPCompra  =mkBtn(ID_P_COMPRA,L"Nueva compra",526,T+268,150,30,BS_PRIMARY);
    hPHistLabel=mkStatic(L"Historial de compras",16,T+312,300,20,hFontBold);
    hPHist=mkList(ID_P_HIST,16,T+336,W,150);
    lvAddCol(hPHist,0,L"Fecha",100); lvAddCol(hPHist,1,L"Proveedor",220); lvAddCol(hPHist,2,L"Detalle",430); lvAddCol(hPHist,3,L"Total",120); lvAddCol(hPHist,4,L"Estado",120);

    // ---- CANCHAS ----
    hALabel=mkStatic(L"Canchas",16,T+2,200,22,hFontH2);
    hAList=mkList(ID_A_CLIST,16,T+30,340,300);
    lvAddCol(hAList,0,L"ID",40); lvAddCol(hAList,1,L"Cancha",180); lvAddCol(hAList,2,L"Precio/h",110);
    hANueva   =mkBtn(ID_A_NUEVA,L"+ Cancha",16,T+338,110,30,BS_PRIMARY);
    hAEditar  =mkBtn(ID_A_EDITAR,L"Editar",136,T+338,90,30,BS_GHOST);
    hAEliminar=mkBtn(ID_A_ELIMINAR,L"Eliminar",236,T+338,110,30,BS_DANGER);
    hARLabel=mkStatic(L"Reservas / alquileres",372,T+2,300,22,hFontH2);
    hARList=mkList(ID_A_RLIST,372,T+30,656,300);
    lvAddCol(hARList,0,L"Fecha",90); lvAddCol(hARList,1,L"Hora",55); lvAddCol(hARList,2,L"Cancha",120); lvAddCol(hARList,3,L"Cliente",120); lvAddCol(hARList,4,L"Dur",45); lvAddCol(hARList,5,L"Total",80); lvAddCol(hARList,6,L"Adelanto",80); lvAddCol(hARList,7,L"Saldo",75); lvAddCol(hARList,8,L"Estado",85);
    hARNueva   =mkBtn(ID_A_RNUEVA,L"+ Nueva reserva",372,T+338,160,30,BS_SUCCESS);
    hAREditar  =mkBtn(ID_A_REDITAR,L"Editar reserva",542,T+338,150,30,BS_GHOST);
    hAREliminar=mkBtn(ID_A_RELIMINAR,L"Eliminar reserva",702,T+338,160,30,BS_DANGER);

    // ---- CAJA ----
    hKLabel=mkStatic(L"Caja - sesion actual",16,T+2,300,22,hFontH2);
    hKInfo=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,16,T+30,W,414,hMain,(HMENU)ID_K_INFO,nullptr,nullptr);
    HFONT hMono=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
    SendMessageW(hKInfo,WM_SETFONT,(WPARAM)hMono,TRUE);
    hKIng   =mkBtn(ID_K_ING,L"+ Ingreso",16,T+452,130,32,BS_SUCCESS);
    hKGas   =mkBtn(ID_K_GAS,L"- Gasto",156,T+452,130,32,BS_GHOST);
    hKRet   =mkBtn(ID_K_RET,L"Retiro",296,T+452,130,32,BS_GHOST);
    hKCerrar=mkBtn(ID_K_CERRAR,L"Cerrar caja",436,T+452,150,32,BS_DANGER);
}

// ----------------------------------------------------------------------------
//  Ventana principal
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        hMain=hwnd;
        hFont    =CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        hFontBold=CreateFontW(-15,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        hFontBig =CreateFontW(-19,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        hFontH1  =CreateFontW(-24,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        hFontH2  =CreateFontW(-17,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");

        // Botones de navegacion (pestanas)
        const wchar_t* tabs[]={L"Ventas",L"Inventario",L"Clientes",L"Proveedores",L"Canchas",L"Caja"};
        int nx=16;
        for(int i=0;i<6;++i){
            int w=118;
            hNav[i]=CreateWindowW(L"BUTTON",tabs[i],WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,nx,66,w,34,hwnd,(HMENU)(INT_PTR)(ID_NAV_BASE+i),nullptr,nullptr);
            navIndex[ID_NAV_BASE+i]=i; nx+=w+6;
        }
        hAdminBtn=CreateWindowW(L"BUTTON",L"Ingresar como admin",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,838,66,190,34,hwnd,(HMENU)ID_ADMIN,nullptr,nullptr);
        btnStyle[ID_ADMIN]=BS_GHOST;

        hStatus=CreateWindowW(STATUSCLASSNAMEW,L"Listo",WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,0,0,0,0,hwnd,(HMENU)0,nullptr,nullptr);

        crearControles();
        aplicarTab(0);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        // banda de cabecera con color de marca
        RECT hd={0,0,rc.right,56};
        HBRUSH b=CreateSolidBrush(C_ACCENT); FillRect(dc,&hd,b); DeleteObject(b);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,RGB(255,255,255));
        HGDIOBJ of=SelectObject(dc,hFontH1);
        RECT t1={20,8,600,40}; DrawTextW(dc,L"POS Quiosco",-1,&t1,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,hFont);
        SetTextColor(dc,RGB(210,225,245));
        RECT t2={20,32,600,52}; DrawTextW(dc,L"Punto de venta",-1,&t2,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,of);
        EndPaint(hwnd,&ps);
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC dc=(HDC)wp; RECT rc; GetClientRect(hwnd,&rc);
        RECT below={0,56,rc.right,rc.bottom}; FillRect(dc,&below,hbrBg);
        return 1;
    }
    case WM_DRAWITEM: { drawButton((DRAWITEMSTRUCT*)lp); return TRUE; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)wp; HWND ctl=(HWND)lp;
        if(ctl==hKInfo){ SetBkColor(dc,C_CARD); SetTextColor(dc,C_TEXT); return (LRESULT)hbrCard; }
        SetBkColor(dc,C_BG); SetTextColor(dc,C_TEXT); return (LRESULT)hbrBg;
    }
    case WM_NOTIFY: {
        LPNMHDR nh=(LPNMHDR)lp;
        if(nh->idFrom==ID_V_PRODLIST && nh->code==NM_DBLCLK){ int r=lvSel(hVProd); if(r>=0) agregarAlCarritoPorId((int)lvParam(hVProd,r)); return 0; }
        if(nh->idFrom==ID_I_LIST && nh->code==NM_DBLCLK){ invEditar(); return 0; }
        if(nh->idFrom==ID_C_LIST && nh->code==NM_DBLCLK){ cliCobranza(); return 0; }
        if(nh->idFrom==ID_P_LIST && nh->code==NM_DBLCLK){ provEditar(); return 0; }
        if(nh->idFrom==ID_A_RLIST && nh->code==NM_DBLCLK){ reservaEditar(); return 0; }
        break;
    }
    case WM_COMMAND: {
        int id=LOWORD(wp), code=HIWORD(wp);
        if(id>=ID_NAV_BASE && id<ID_NAV_BASE+6){ aplicarTab(id-ID_NAV_BASE); return 0; }
        switch(id){
        case ID_ADMIN: if(esAdmin){ esAdmin=false; SetWindowTextW(hAdminBtn,L"Ingresar como admin"); InvalidateRect(hAdminBtn,nullptr,TRUE); setStatus(L"Sesion admin cerrada"); } else pedirAdmin(); break;
        case ID_V_ADD: ventaBuscarYAgregar(); break;
        case ID_V_BUSCAR: if(code==EN_CHANGE) refrescarProductosVenta(); break;
        case ID_V_EFECTIVO: if(code==EN_CHANGE) refrescarCarrito(); break;
        case ID_V_FIADO: refrescarCarrito(); break;
        case ID_V_QUITAR: { int r=lvSel(hVCart); if(r>=0&&r<(int)carrito.size()){ carrito.erase(carrito.begin()+r); refrescarCarrito(); } break; }
        case ID_V_LIMPIAR: carrito.clear(); refrescarCarrito(); break;
        case ID_V_COBRAR: cobrar(); break;
        case ID_I_NUEVO: invNuevo(); break;
        case ID_I_EDITAR: invEditar(); break;
        case ID_I_ELIMINAR: invEliminar(); break;
        case ID_I_STOCK: invStock(); break;
        case ID_C_NUEVO: cliNuevo(); break;
        case ID_C_COBRANZA: cliCobranza(); break;
        case ID_P_NUEVO: provNuevo(); break;
        case ID_P_EDITAR: provEditar(); break;
        case ID_P_ELIMINAR: provEliminar(); break;
        case ID_P_PAGO: provPago(); break;
        case ID_P_COMPRA: provCompra(); break;
        case ID_A_NUEVA: canchaNueva(); break;
        case ID_A_EDITAR: canchaEditar(); break;
        case ID_A_ELIMINAR: canchaEliminar(); break;
        case ID_A_RNUEVA: reservaNueva(); break;
        case ID_A_REDITAR: reservaEditar(); break;
        case ID_A_RELIMINAR: reservaEliminar(); break;
        case ID_K_ING: cajaMov("ingreso"); break;
        case ID_K_GAS: cajaMov("gasto"); break;
        case ID_K_RET: cajaMov("retiro"); break;
        case ID_K_CERRAR: cajaCerrar(); break;
        }
        return 0;
    }
    case WM_SIZE: SendMessageW(hStatus,WM_SIZE,0,0); return 0;
    case WM_DESTROY:
        guardarDatos();
        DeleteObject(hFont); DeleteObject(hFontBold); DeleteObject(hFontBig); DeleteObject(hFontH1); DeleteObject(hFontH2);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}

// ----------------------------------------------------------------------------
//  WinMain
// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&icc);
    hbrBg=CreateSolidBrush(C_BG); hbrCard=CreateSolidBrush(C_CARD);

    ifstream test(DATA_FILE);
    if(test.good()){ test.close(); cargarDatos(); } else { cargarDatosEjemplo(); guardarDatos(); }

    WNDCLASSW wc={};
    wc.lpfnWndProc=WndProc; wc.hInstance=hInst; wc.lpszClassName=L"POSQuioscoMain";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=hbrBg; wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    RegisterClassW(&wc);

    HWND hwnd=CreateWindowW(L"POSQuioscoMain",L"POS Quiosco - Punto de Venta",WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX,
                CW_USEDEFAULT,CW_USEDEFAULT,1060,760,nullptr,nullptr,hInst,nullptr);
    ShowWindow(hwnd,nCmdShow); UpdateWindow(hwnd);

    MSG msg;
    while(GetMessageW(&msg,nullptr,0,0)){
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_F2 && currentTab==0){ cobrar(); continue; }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN && GetFocus()==hVBuscar){ ventaBuscarYAgregar(); continue; }
        if(IsDialogMessageW(hwnd,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    return 0;
}

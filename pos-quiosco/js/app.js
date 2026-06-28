const STORAGE_KEY = 'posQuioscoData';

let products = [
  {id:1,name:'Agua San Luis 625ml',price:1.50,stock:48,barcode:'7751010001234'},
  {id:2,name:'Coca Cola 500ml',price:3.00,stock:24,barcode:'7501055300006'},
  {id:3,name:'Galletas Oreo',price:2.50,stock:36,barcode:'7622210003232'},
  {id:4,name:'Chicles Halls',price:1.00,stock:60,barcode:'0040000004096'},
  {id:5,name:'Papas Lays clásicas',price:3.50,stock:18,barcode:'7501012004040'},
  {id:6,name:'Yogurt Gloria 120g',price:2.00,stock:20,barcode:'7750016000789'},
  {id:7,name:'Jugo Pulp naranja',price:2.50,stock:15,barcode:'7750016100456'},
  {id:8,name:'Pilas AA Duracell x2',price:5.00,stock:30,barcode:'0041333045559'},
];
let cart = [];
let salesHistory = [];
let clients = [];
let providers = [];
let users = [{id:1,name:'Administrador',role:'Administrador'}];
let purchases = [];
let cajaMovs = [];
let clientPayments = [];
let providerPayments = [];
let cierres = [];
let cajaAbiertaDesde = new Date().toISOString();
let nextId = 9;
let nextClientId = 1;
let nextProviderId = 1;
let nextUserId = 2;
let editingId = null;
let editingClientId = null;
let editingProviderId = null;
let editingUserId = null;
let pagoTarget = null;
let cajaMovType = null;
let activeUserId = 1;
let scanTimer = null;
let currentTab = 'venta';
let currentReporte = 'ventas';

function saveData() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify({
    products, salesHistory, clients, providers, users, purchases, cajaMovs,
    clientPayments, providerPayments, cierres, cajaAbiertaDesde,
    nextId, nextClientId, nextProviderId, nextUserId, activeUserId
  }));
}

function loadData() {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (!raw) return;
  try {
    const d = JSON.parse(raw);
    products = d.products || products;
    salesHistory = d.salesHistory || [];
    clients = d.clients || [];
    providers = d.providers || [];
    users = d.users && d.users.length ? d.users : users;
    purchases = d.purchases || [];
    cajaMovs = d.cajaMovs || [];
    clientPayments = d.clientPayments || [];
    providerPayments = d.providerPayments || [];
    cierres = d.cierres || [];
    cajaAbiertaDesde = d.cajaAbiertaDesde || cajaAbiertaDesde;
    nextId = d.nextId || nextId;
    nextClientId = d.nextClientId || nextClientId;
    nextProviderId = d.nextProviderId || nextProviderId;
    nextUserId = d.nextUserId || nextUserId;
    activeUserId = d.activeUserId || activeUserId;
  } catch (e) {}
}

function closeModalById(id) {
  document.getElementById(id).classList.remove('show');
}

function closeModalOuterById(e, id) {
  if (e.target === document.getElementById(id)) closeModalById(id);
}

function setTab(t) {
  currentTab = t;
  const tabs = ['venta','inventario','clientes','proveedores','caja','reportes','usuarios'];
  tabs.forEach((tab,i) => {
    document.getElementById('tab-'+tab).style.display = tab===t ? '' : 'none';
    document.querySelectorAll('.left > .tabs')[0].querySelectorAll('.tab')[i].classList.toggle('active', tab===t);
  });
  if (t==='inventario') renderInventario();
  if (t==='clientes') renderClientes();
  if (t==='proveedores') renderProveedores();
  if (t==='caja') renderCaja();
  if (t==='reportes') renderReportes();
  if (t==='usuarios') renderUsuarios();
}

function prodCardHtml(p) {
  return `
    <div class="prod-card" onclick="addToCart(${p.id})" title="Cód: ${p.barcode}">
      <i class="ti ${p.pinned ? 'ti-pin-filled' : 'ti-pin'} pin-btn" onclick="event.stopPropagation();togglePin(${p.id})" title="${p.pinned ? 'Desanclar' : 'Anclar'}"></i>
      <div class="prod-name">${p.name}</div>
      <div class="prod-price">S/ ${p.price.toFixed(2)}</div>
      <div class="prod-stock">${p.stock} en stock</div>
      <div class="prod-barcode">${p.barcode}</div>
    </div>
  `;
}

function renderPinned() {
  const label = document.getElementById('pinned-label');
  const grid = document.getElementById('pinned-grid');
  const pinned = products.filter(p => p.pinned);
  if (!pinned.length) {
    label.style.display = 'none';
    grid.style.display = 'none';
    return;
  }
  label.style.display = '';
  grid.style.display = '';
  grid.innerHTML = pinned.map(prodCardHtml).join('');
}

function togglePin(id) {
  const p = products.find(x => x.id === id);
  if (!p) return;
  p.pinned = !p.pinned;
  saveData();
  renderPinned();
  renderProducts(document.getElementById('barcode-input').value);
}

function renderProducts(filter='') {
  renderPinned();
  const grid = document.getElementById('products-grid');
  const q = filter.toLowerCase();
  const filtered = products.filter(p => p.name.toLowerCase().includes(q) || p.barcode.includes(q));
  if (!filtered.length) {
    grid.innerHTML = '<div style="color:#bbb;font-size:13px;grid-column:1/-1;padding:1rem 0">Sin resultados para "'+filter+'"</div>';
    return;
  }
  grid.innerHTML = filtered.map(prodCardHtml).join('');
}

function addToCart(id) {
  const prod = products.find(p => p.id === id);
  if (!prod) return;
  if (prod.stock <= 0) { showNotify('Sin stock: ' + prod.name, 'danger'); return; }
  const existing = cart.find(c => c.id === id);
  if (existing) {
    if (existing.qty >= prod.stock) { showNotify('Stock máximo alcanzado', 'danger'); return; }
    existing.qty++;
  } else {
    cart.push({id, name:prod.name, price:prod.price, qty:1, descuento:0});
  }
  renderCart();
  showNotify(prod.name + ' agregado');
}

function changeQty(id, delta) {
  const item = cart.find(c => c.id === id);
  if (!item) return;
  item.qty += delta;
  if (item.qty <= 0) cart = cart.filter(c => c.id !== id);
  renderCart();
}

function removeItem(id) {
  cart = cart.filter(c => c.id !== id);
  renderCart();
}

function renderClienteSelect() {
  const sel = document.getElementById('cart-cliente-select');
  const prev = sel.value;
  sel.innerHTML = '<option value="">Sin cliente / Mostrador</option>' +
    clients.map(c => `<option value="${c.id}">${c.name}</option>`).join('');
  sel.value = clients.find(c => String(c.id)===prev) ? prev : '';
}

function onCarritoClienteChange() {
  const sel = document.getElementById('cart-cliente-select');
  document.getElementById('fiado-row').style.display = sel.value ? 'flex' : 'none';
  if (!sel.value) document.getElementById('fiado-check').checked = false;
  calcVuelto();
}

function cartTotal() {
  return cart.reduce((s,c) => s + Math.max(0, c.price * c.qty - (c.descuento||0)), 0);
}

function renderCart() {
  const el = document.getElementById('cart-items');
  const total = cartTotal();
  const count = cart.reduce((s,c) => s + c.qty, 0);
  document.getElementById('cart-count').textContent = count + ' ítem' + (count!==1?'s':'');
  document.getElementById('total-amount').textContent = 'S/ ' + total.toFixed(2);
  document.getElementById('cobrar-btn').disabled = cart.length === 0;
  calcVuelto();
  if (!cart.length) {
    el.innerHTML = '<div class="cart-empty"><i class="ti ti-barcode"></i><span>Escanea un producto para empezar</span></div>';
    return;
  }
  el.innerHTML = cart.map(item => `
    <div class="cart-item">
      <div class="cart-item-info">
        <div class="cart-item-name">${item.name}</div>
        <div class="cart-item-price">S/ ${item.price.toFixed(2)} c/u</div>
        <div class="cart-item-desc"><label>Desc. S/</label><input type="number" class="desc-input" min="0" step="0.10" value="${item.descuento || ''}" placeholder="0.00" oninput="updateDescuento(${item.id},this.value)"></div>
      </div>
      <div class="qty-ctrl">
        <button class="qty-btn" onclick="changeQty(${item.id},-1)">−</button>
        <span class="qty-num">${item.qty}</span>
        <button class="qty-btn" onclick="changeQty(${item.id},1)">+</button>
      </div>
      <div class="item-subtotal" data-id="${item.id}">S/ ${Math.max(0,item.price*item.qty-(item.descuento||0)).toFixed(2)}</div>
      <i class="ti ti-x del-btn" onclick="removeItem(${item.id})" title="Quitar"></i>
    </div>
  `).join('');
}

function updateDescuento(id, val) {
  const item = cart.find(c => c.id === id);
  if (!item) return;
  let d = parseFloat(val);
  if (isNaN(d) || d < 0) d = 0;
  const max = item.price * item.qty;
  if (d > max) d = max;
  item.descuento = d;
  const subEl = document.querySelector(`.item-subtotal[data-id="${id}"]`);
  if (subEl) subEl.textContent = 'S/ ' + (max - d).toFixed(2);
  document.getElementById('total-amount').textContent = 'S/ ' + cartTotal().toFixed(2);
  calcVuelto();
}

function isFiado() {
  const sel = document.getElementById('cart-cliente-select');
  return !!sel.value && document.getElementById('fiado-check').checked;
}

function calcVuelto() {
  const total = cartTotal();
  const row = document.getElementById('vuelto-row');
  const wrap = document.getElementById('efectivo-row-wrap');
  if (isFiado()) {
    wrap.style.display = 'none';
    row.style.display = 'none';
    return;
  }
  wrap.style.display = 'flex';
  const efectivo = parseFloat(document.getElementById('efectivo-input').value) || 0;
  if (efectivo > 0 && efectivo >= total) {
    document.getElementById('vuelto-amt').textContent = 'S/ ' + (efectivo - total).toFixed(2);
    row.style.display = 'flex';
  } else {
    row.style.display = 'none';
  }
}

function cobrar() {
  const total = cartTotal();
  const fiado = isFiado();
  const clienteId = document.getElementById('cart-cliente-select').value;
  const efectivo = parseFloat(document.getElementById('efectivo-input').value) || 0;
  if (!fiado && efectivo > 0 && efectivo < total) { showNotify('Efectivo insuficiente', 'danger'); return; }
  cart.forEach(item => {
    const prod = products.find(p => p.id === item.id);
    if (prod) prod.stock = Math.max(0, prod.stock - item.qty);
  });
  const cliente = clienteId ? clients.find(c => c.id === Number(clienteId)) : null;
  if (fiado && cliente) cliente.saldo = (cliente.saldo || 0) + total;
  salesHistory.push({
    items:[...cart.map(c=>({...c}))], total, time: new Date(),
    paymentType: fiado ? 'cuenta corriente' : 'efectivo',
    clientId: cliente ? cliente.id : null,
    clientName: cliente ? cliente.name : null,
    userId: activeUserId,
    userName: (users.find(u=>u.id===activeUserId)||{}).name || ''
  });
  if (!fiado) {
    cajaMovs.push({id: Date.now(), type:'ingreso', amount: total, desc: 'Venta', date: new Date().toISOString(), auto:true});
  }
  showNotify(fiado ? 'Venta a cuenta corriente registrada — S/ ' + total.toFixed(2) : 'Venta registrada — S/ ' + total.toFixed(2), 'success');
  clearCart();
  renderProducts(document.getElementById('barcode-input').value);
  saveData();
}

function clearCart() {
  cart = [];
  document.getElementById('efectivo-input').value = '';
  document.getElementById('cart-cliente-select').value = '';
  document.getElementById('fiado-check').checked = false;
  document.getElementById('fiado-row').style.display = 'none';
  renderCart();
}

function onBarcode(val) {
  clearTimeout(scanTimer);
  if (!val) { renderProducts(''); return; }
  scanTimer = setTimeout(() => {
    const prod = products.find(p => p.barcode === val.trim());
    if (prod) { addToCart(prod.id); document.getElementById('barcode-input').value = ''; renderProducts(''); }
  }, 80);
  renderProducts(val);
}

function onKey(e) {
  if (e.key === 'Enter') {
    clearTimeout(scanTimer);
    const val = e.target.value.trim();
    const prod = products.find(p => p.barcode === val);
    if (prod) { addToCart(prod.id); e.target.value = ''; renderProducts(''); }
    else if (val) showNotify('Código no encontrado: ' + val, 'danger');
  }
}

function renderExistingProductSelect(selectedId=null) {
  const sel = document.getElementById('m-existing');
  const sorted = [...products].sort((a,b) => a.name.localeCompare(b.name));
  sel.innerHTML = '<option value="">+ Producto nuevo</option>' +
    sorted.map(p => `<option value="${p.id}">${p.name}</option>`).join('');
  sel.value = selectedId || '';
}

function onExistingProductChange(idStr) {
  const id = idStr ? parseInt(idStr) : null;
  editingId = id;
  const p = id ? products.find(x => x.id === id) : null;
  document.getElementById('modal-title').textContent = id ? 'Editar producto' : 'Nuevo producto';
  document.getElementById('m-stock-label').textContent = id ? 'Stock actual' : 'Stock inicial';
  document.getElementById('m-name').value = p ? p.name : '';
  document.getElementById('m-price').value = p ? p.price : '';
  document.getElementById('m-stock').value = p ? p.stock : '';
  document.getElementById('m-barcode').value = p ? p.barcode : '';
}

function openAddModal(id=null) {
  editingId = id;
  const p = id ? products.find(x => x.id === id) : null;
  renderExistingProductSelect(id);
  document.getElementById('modal-title').textContent = id ? 'Editar producto' : 'Nuevo producto';
  document.getElementById('m-stock-label').textContent = id ? 'Stock actual' : 'Stock inicial';
  document.getElementById('m-name').value = p ? p.name : '';
  document.getElementById('m-price').value = p ? p.price : '';
  document.getElementById('m-stock').value = p ? p.stock : '';
  document.getElementById('m-barcode').value = p ? p.barcode : '';
  document.getElementById('modal-overlay').classList.add('show');
  setTimeout(() => document.getElementById('m-name').focus(), 100);
}

function closeModal() {
  document.getElementById('modal-overlay').classList.remove('show');
  editingId = null;
}

function closeModalOuter(e) {
  if (e.target === document.getElementById('modal-overlay')) closeModal();
}

function saveProduct() {
  const name = document.getElementById('m-name').value.trim();
  const price = parseFloat(document.getElementById('m-price').value);
  const stock = parseInt(document.getElementById('m-stock').value) || 0;
  const barcode = document.getElementById('m-barcode').value.trim();
  if (!name || isNaN(price) || price < 0) { showNotify('Completa nombre y precio', 'danger'); return; }
  if (editingId) {
    const p = products.find(x => x.id === editingId);
    Object.assign(p, {name, price, stock, barcode});
    showNotify('Producto actualizado');
  } else {
    products.push({id:nextId++, name, price, stock, barcode});
    showNotify('Producto agregado');
  }
  closeModal();
  renderProducts(document.getElementById('barcode-input').value);
  if (currentTab === 'inventario') renderInventario();
  renderCompraProductoSelect();
  saveData();
}

function renderInventario() {
  const el = document.getElementById('inventario-list');
  el.innerHTML = products.map(p => {
    const badge = p.stock <= 0
      ? '<span class="badge-out">Sin stock</span>'
      : p.stock <= 5 ? '<span class="badge-low">Stock bajo</span>'
      : '<span class="badge-ok">OK</span>';
    return `<div class="inv-item">
      <div style="flex:1;min-width:0">
        <div style="font-size:13px;font-weight:600;color:#1a1a18">${p.name}</div>
        <div style="font-size:11px;color:#bbb;font-family:monospace">${p.barcode || '—'}</div>
      </div>
      <div style="text-align:right;display:flex;flex-direction:column;align-items:flex-end;gap:4px">
        <div style="font-size:13px;font-weight:700;color:#185fa5">S/ ${p.price.toFixed(2)}</div>
        <div style="display:flex;align-items:center;gap:5px">${badge} <span style="font-size:12px;color:#666">${p.stock} uds</span></div>
      </div>
      <i class="ti ti-pencil" onclick="openAddModal(${p.id})" title="Editar" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px;margin-left:4px"></i>
      <i class="ti ti-trash" onclick="deleteProduct(${p.id})" title="Eliminar" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
    </div>`;
  }).join('');
}

function deleteProduct(id) {
  const p = products.find(x => x.id === id);
  if (!p) return;
  if (!confirm('¿Eliminar "' + p.name + '" del inventario?')) return;
  products = products.filter(x => x.id !== id);
  cart = cart.filter(c => c.id !== id);
  renderInventario();
  renderProducts(document.getElementById('barcode-input').value);
  renderCart();
  renderCompraProductoSelect();
  saveData();
  showNotify('Producto eliminado');
}

function movsSesionActual() {
  const desde = new Date(cajaAbiertaDesde).getTime();
  return cajaMovs.filter(m => new Date(m.date).getTime() >= desde);
}

function renderCaja() {
  const el = document.getElementById('caja-section');
  const movs = movsSesionActual();
  if (!movs.length) {
    el.innerHTML = '<div class="resumen-empty">No hay movimientos registrados en esta sesión</div>'; return;
  }
  const ingresos = movs.filter(m => m.type==='ingreso').reduce((s,m)=>s+m.amount,0);
  const gastos = movs.filter(m => m.type==='gasto').reduce((s,m)=>s+m.amount,0);
  const retiros = movs.filter(m => m.type==='retiro').reduce((s,m)=>s+m.amount,0);
  const saldo = ingresos - gastos - retiros;
  el.innerHTML = `
    <div class="caja-row"><span style="color:#666">Ingresos</span><span style="font-weight:600;color:#3b6d11">S/ ${ingresos.toFixed(2)}</span></div>
    <div class="caja-row"><span style="color:#666">Gastos</span><span style="font-weight:600;color:#a32d2d">S/ ${gastos.toFixed(2)}</span></div>
    <div class="caja-row"><span style="color:#666">Retiros</span><span style="font-weight:600;color:#a32d2d">S/ ${retiros.toFixed(2)}</span></div>
    ${movs.slice().reverse().map(m => `<div class="caja-row"><span style="color:#999">${m.desc} — ${new Date(m.date).toLocaleTimeString('es-PE',{hour:'2-digit',minute:'2-digit'})}</span><span>${m.type==='ingreso'?'+':'-'} S/ ${m.amount.toFixed(2)}</span></div>`).join('')}
    <div class="caja-row"><span>Saldo en caja</span><span style="color:#185fa5">S/ ${saldo.toFixed(2)}</span></div>
  `;
}

function openCajaMovModal(type) {
  cajaMovType = type;
  const titles = {ingreso:'Registrar ingreso', gasto:'Registrar gasto', retiro:'Registrar retiro'};
  document.getElementById('modal-cajamov-title').textContent = titles[type];
  document.getElementById('cajamov-amount').value = '';
  document.getElementById('cajamov-desc').value = '';
  document.getElementById('modal-cajamov-overlay').classList.add('show');
}

function saveCajaMov() {
  const amount = parseFloat(document.getElementById('cajamov-amount').value);
  const desc = document.getElementById('cajamov-desc').value.trim() || cajaMovType;
  if (isNaN(amount) || amount <= 0) { showNotify('Ingresa un monto válido', 'danger'); return; }
  cajaMovs.push({id: Date.now(), type: cajaMovType, amount, desc, date: new Date().toISOString()});
  closeModalById('modal-cajamov-overlay');
  renderCaja();
  showNotify('Movimiento registrado');
  saveData();
}

function cerrarCaja() {
  if (!confirm('¿Cerrar caja? Se archivará el resumen de esta sesión en Reportes.')) return;
  const movs = movsSesionActual();
  const ingresos = movs.filter(m => m.type==='ingreso').reduce((s,m)=>s+m.amount,0);
  const gastos = movs.filter(m => m.type==='gasto').reduce((s,m)=>s+m.amount,0);
  const retiros = movs.filter(m => m.type==='retiro').reduce((s,m)=>s+m.amount,0);
  cierres.push({
    id: Date.now(), desde: cajaAbiertaDesde, hasta: new Date().toISOString(),
    ingresos, gastos, retiros, saldo: ingresos - gastos - retiros
  });
  cajaAbiertaDesde = new Date().toISOString();
  renderCaja();
  showNotify('Caja cerrada');
  saveData();
}

function renderClientes() {
  renderClienteSelect();
  const el = document.getElementById('clientes-list');
  if (!clients.length) { el.innerHTML = '<div class="resumen-empty">No hay clientes registrados</div>'; return; }
  el.innerHTML = clients.map(c => {
    const saldo = c.saldo || 0;
    const badge = saldo > 0 ? `<span class="badge-low">Debe S/ ${saldo.toFixed(2)}</span>` : '<span class="badge-ok">Al día</span>';
    return `<div class="inv-item">
      <div style="flex:1;min-width:0">
        <div style="font-size:13px;font-weight:600;color:#1a1a18">${c.name}</div>
        <div style="font-size:11px;color:#bbb">${c.phone || '—'}</div>
      </div>
      <div style="text-align:right;display:flex;flex-direction:column;align-items:flex-end;gap:4px">${badge}</div>
      <i class="ti ti-cash" onclick="openPagoModal('cliente',${c.id})" title="Registrar pago" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
      <i class="ti ti-pencil" onclick="openClienteModal(${c.id})" title="Editar" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
    </div>`;
  }).join('');
}

function openClienteModal(id=null) {
  editingClientId = id;
  const c = id ? clients.find(x => x.id === id) : null;
  document.getElementById('modal-cliente-title').textContent = id ? 'Editar cliente' : 'Nuevo cliente';
  document.getElementById('cl-name').value = c ? c.name : '';
  document.getElementById('cl-phone').value = c ? c.phone : '';
  document.getElementById('cl-limite').value = c ? c.limite : '';
  document.getElementById('cl-address').value = c ? c.address : '';
  document.getElementById('modal-cliente-overlay').classList.add('show');
}

function saveCliente() {
  const name = document.getElementById('cl-name').value.trim();
  const phone = document.getElementById('cl-phone').value.trim();
  const limite = parseFloat(document.getElementById('cl-limite').value) || 0;
  const address = document.getElementById('cl-address').value.trim();
  if (!name) { showNotify('Ingresa el nombre del cliente', 'danger'); return; }
  if (editingClientId) {
    Object.assign(clients.find(x => x.id === editingClientId), {name, phone, limite, address});
    showNotify('Cliente actualizado');
  } else {
    clients.push({id: nextClientId++, name, phone, limite, address, saldo: 0});
    showNotify('Cliente agregado');
  }
  closeModalById('modal-cliente-overlay');
  renderClientes();
  saveData();
}

function renderProveedores() {
  renderCompraProductoSelect();
  const el = document.getElementById('proveedores-list');
  if (!providers.length) { el.innerHTML = '<div class="resumen-empty">No hay proveedores registrados</div>'; return; }
  el.innerHTML = providers.map(p => {
    const saldo = p.saldo || 0;
    const badge = saldo > 0 ? `<span class="badge-low">Debemos S/ ${saldo.toFixed(2)}</span>` : '<span class="badge-ok">Al día</span>';
    return `<div class="inv-item">
      <div style="flex:1;min-width:0">
        <div style="font-size:13px;font-weight:600;color:#1a1a18">${p.name}</div>
        <div style="font-size:11px;color:#bbb">${p.phone || '—'} ${p.contact ? '· '+p.contact : ''}</div>
      </div>
      <div style="text-align:right;display:flex;flex-direction:column;align-items:flex-end;gap:4px">${badge}</div>
      <i class="ti ti-truck-delivery" onclick="openCompraModal(${p.id})" title="Ingreso de mercadería" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
      <i class="ti ti-cash" onclick="openPagoModal('proveedor',${p.id})" title="Registrar pago" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
      <i class="ti ti-pencil" onclick="openProveedorModal(${p.id})" title="Editar" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
    </div>`;
  }).join('');
}

function openProveedorModal(id=null) {
  editingProviderId = id;
  const p = id ? providers.find(x => x.id === id) : null;
  document.getElementById('modal-proveedor-title').textContent = id ? 'Editar proveedor' : 'Nuevo proveedor';
  document.getElementById('pv-name').value = p ? p.name : '';
  document.getElementById('pv-phone').value = p ? p.phone : '';
  document.getElementById('pv-contact').value = p ? p.contact : '';
  document.getElementById('modal-proveedor-overlay').classList.add('show');
}

function saveProveedor() {
  const name = document.getElementById('pv-name').value.trim();
  const phone = document.getElementById('pv-phone').value.trim();
  const contact = document.getElementById('pv-contact').value.trim();
  if (!name) { showNotify('Ingresa el nombre del proveedor', 'danger'); return; }
  if (editingProviderId) {
    Object.assign(providers.find(x => x.id === editingProviderId), {name, phone, contact});
    showNotify('Proveedor actualizado');
  } else {
    providers.push({id: nextProviderId++, name, phone, contact, saldo: 0});
    showNotify('Proveedor agregado');
  }
  closeModalById('modal-proveedor-overlay');
  renderProveedores();
  saveData();
}

function renderCompraProductoSelect() {
  const sel = document.getElementById('compra-producto');
  if (!sel) return;
  sel.innerHTML = products.map(p => `<option value="${p.id}">${p.name}</option>`).join('');
}

function openCompraModal(providerId) {
  pagoTarget = {type:'compra', id: providerId};
  renderCompraProductoSelect();
  document.getElementById('compra-cantidad').value = '';
  document.getElementById('compra-costo').value = '';
  document.getElementById('modal-compra-overlay').classList.add('show');
}

function saveCompra() {
  const productoId = Number(document.getElementById('compra-producto').value);
  const cantidad = parseInt(document.getElementById('compra-cantidad').value);
  const costo = parseFloat(document.getElementById('compra-costo').value);
  if (!productoId || isNaN(cantidad) || cantidad <= 0 || isNaN(costo) || costo < 0) {
    showNotify('Completa producto, cantidad y costo', 'danger'); return;
  }
  const prod = products.find(p => p.id === productoId);
  const provider = providers.find(p => p.id === pagoTarget.id);
  const total = cantidad * costo;
  prod.stock += cantidad;
  provider.saldo = (provider.saldo || 0) + total;
  purchases.push({id: Date.now(), providerId: provider.id, productId: prod.id, productName: prod.name, cantidad, costo, total, date: new Date().toISOString()});
  closeModalById('modal-compra-overlay');
  renderProveedores();
  if (currentTab === 'inventario') renderInventario();
  showNotify('Compra registrada — stock actualizado');
  saveData();
}

function openPagoModal(type, id) {
  pagoTarget = {type, id};
  document.getElementById('modal-pago-title').textContent = type === 'cliente' ? 'Registrar pago de cliente' : 'Registrar pago a proveedor';
  document.getElementById('pago-amount').value = '';
  document.getElementById('modal-pago-overlay').classList.add('show');
}

function savePago() {
  const amount = parseFloat(document.getElementById('pago-amount').value);
  if (isNaN(amount) || amount <= 0) { showNotify('Ingresa un monto válido', 'danger'); return; }
  if (pagoTarget.type === 'cliente') {
    const c = clients.find(x => x.id === pagoTarget.id);
    c.saldo = Math.max(0, (c.saldo || 0) - amount);
    clientPayments.push({id: Date.now(), clientId: c.id, amount, date: new Date().toISOString()});
    cajaMovs.push({id: Date.now()+1, type:'ingreso', amount, desc: 'Cobranza — ' + c.name, date: new Date().toISOString()});
    renderClientes();
    showNotify('Pago de cliente registrado');
  } else {
    const p = providers.find(x => x.id === pagoTarget.id);
    p.saldo = Math.max(0, (p.saldo || 0) - amount);
    providerPayments.push({id: Date.now(), providerId: p.id, amount, date: new Date().toISOString()});
    cajaMovs.push({id: Date.now()+1, type:'gasto', amount, desc: 'Pago a proveedor — ' + p.name, date: new Date().toISOString()});
    renderProveedores();
    showNotify('Pago a proveedor registrado');
  }
  closeModalById('modal-pago-overlay');
  saveData();
}

function renderUsuarios() {
  renderUsuarioActivoSelect();
  const el = document.getElementById('usuarios-list');
  el.innerHTML = users.map(u => `<div class="inv-item">
      <div style="flex:1;min-width:0">
        <div style="font-size:13px;font-weight:600;color:#1a1a18">${u.name}</div>
        <div style="font-size:11px;color:#bbb">${u.role}</div>
      </div>
      <i class="ti ti-pencil" onclick="openUsuarioModal(${u.id})" title="Editar" style="font-size:16px;color:#bbb;cursor:pointer;padding:4px"></i>
    </div>`).join('');
}

function renderUsuarioActivoSelect() {
  const sel = document.getElementById('usuario-activo-select');
  if (!sel) return;
  sel.innerHTML = users.map(u => `<option value="${u.id}">${u.name}</option>`).join('');
  sel.value = activeUserId;
}

function setUsuarioActivo(id) {
  activeUserId = Number(id);
  saveData();
}

function openUsuarioModal(id=null) {
  editingUserId = id;
  const u = id ? users.find(x => x.id === id) : null;
  document.getElementById('modal-usuario-title').textContent = id ? 'Editar usuario' : 'Nuevo usuario';
  document.getElementById('us-name').value = u ? u.name : '';
  document.getElementById('us-role').value = u ? u.role : 'Vendedor';
  document.getElementById('modal-usuario-overlay').classList.add('show');
}

function saveUsuario() {
  const name = document.getElementById('us-name').value.trim();
  const role = document.getElementById('us-role').value;
  if (!name) { showNotify('Ingresa el nombre del usuario', 'danger'); return; }
  if (editingUserId) {
    Object.assign(users.find(x => x.id === editingUserId), {name, role});
    showNotify('Usuario actualizado');
  } else {
    users.push({id: nextUserId++, name, role});
    showNotify('Usuario agregado');
  }
  closeModalById('modal-usuario-overlay');
  renderUsuarios();
  renderUsuarioActivoSelect();
  saveData();
}

function setReporte(r) {
  currentReporte = r;
  document.querySelectorAll('#tab-reportes .tabs .tab').forEach((el,i) => {
    el.classList.toggle('active', ['ventas','caja','clientes','proveedores','articulos'][i] === r);
  });
  renderReportes();
}

function renderReportes() {
  const el = document.getElementById('reportes-section');
  if (currentReporte === 'ventas') {
    if (!salesHistory.length) { el.innerHTML = '<div class="resumen-empty">No hay ventas registradas</div>'; return; }
    const totalVentas = salesHistory.reduce((s,v)=>s+v.total,0);
    const lineRows = [];
    salesHistory.slice().reverse().forEach(v => {
      const fecha = new Date(v.time).toLocaleString('es-PE',{dateStyle:'short',timeStyle:'short'});
      (v.items||[]).forEach(it => {
        const desc = it.descuento || 0;
        const lineTotal = it.price*it.qty - desc;
        lineRows.push(`<div class="caja-row"><span style="color:#999">${fecha} — ${it.name} · ${it.qty} x S/ ${it.price.toFixed(2)}${desc ? ' · desc S/ '+desc.toFixed(2) : ''}${v.clientName?' · '+v.clientName:''}</span><span>S/ ${lineTotal.toFixed(2)}</span></div>`);
      });
    });
    el.innerHTML = `
      <div class="caja-row"><span style="color:#666">Ventas realizadas</span><span style="font-weight:600">${salesHistory.length}</span></div>
      <div class="caja-row"><span style="color:#666">Ticket promedio</span><span>S/ ${(totalVentas/salesHistory.length).toFixed(2)}</span></div>
      ${lineRows.join('')}
      <div class="caja-row"><span>Total vendido</span><span style="color:#185fa5">S/ ${totalVentas.toFixed(2)}</span></div>
    `;
  } else if (currentReporte === 'caja') {
    if (!cierres.length) { el.innerHTML = '<div class="resumen-empty">No hay cierres de caja registrados</div>'; return; }
    el.innerHTML = cierres.slice().reverse().map(c => `<div class="caja-row"><span style="color:#999">${new Date(c.hasta).toLocaleString('es-PE',{dateStyle:'short',timeStyle:'short'})}</span><span>Ingresos S/ ${c.ingresos.toFixed(2)} · Gastos S/ ${c.gastos.toFixed(2)} · Retiros S/ ${c.retiros.toFixed(2)} · Saldo S/ ${c.saldo.toFixed(2)}</span></div>`).join('');
  } else if (currentReporte === 'clientes') {
    if (!clients.length) { el.innerHTML = '<div class="resumen-empty">No hay clientes registrados</div>'; return; }
    const totalDeuda = clients.reduce((s,c)=>s+(c.saldo||0),0);
    el.innerHTML = clients.map(c => `<div class="caja-row"><span style="color:#999">${c.name}</span><span>S/ ${(c.saldo||0).toFixed(2)}</span></div>`).join('') +
      `<div class="caja-row"><span>Total por cobrar</span><span style="color:#185fa5">S/ ${totalDeuda.toFixed(2)}</span></div>`;
  } else if (currentReporte === 'proveedores') {
    if (!providers.length) { el.innerHTML = '<div class="resumen-empty">No hay proveedores registrados</div>'; return; }
    const totalDeuda = providers.reduce((s,p)=>s+(p.saldo||0),0);
    el.innerHTML = providers.map(p => `<div class="caja-row"><span style="color:#999">${p.name}</span><span>S/ ${(p.saldo||0).toFixed(2)}</span></div>`).join('') +
      `<div class="caja-row"><span>Total por pagar</span><span style="color:#185fa5">S/ ${totalDeuda.toFixed(2)}</span></div>`;
  } else if (currentReporte === 'articulos') {
    const valorInventario = products.reduce((s,p)=>s+p.price*p.stock,0);
    el.innerHTML = products.map(p => `<div class="caja-row"><span style="color:#999">${p.name} (${p.stock} uds)</span><span>S/ ${(p.price*p.stock).toFixed(2)}</span></div>`).join('') +
      `<div class="caja-row"><span>Valor total del inventario</span><span style="color:#185fa5">S/ ${valorInventario.toFixed(2)}</span></div>`;
  }
}

function csvCell(v) {
  const s = String(v ?? '');
  return /[",;\n]/.test(s) ? '"' + s.replace(/"/g,'""') + '"' : s;
}

function downloadCSV(filename, rows) {
  const csv = rows.map(r => r.map(csvCell).join(';')).join('\r\n');
  const blob = new Blob(['﻿' + csv], {type:'text/csv;charset=utf-8;'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

function exportReporteExcel() {
  let rows = [];
  let filename = 'reporte.csv';
  if (currentReporte === 'ventas') {
    filename = 'reporte_ventas.csv';
    rows.push(['Fecha y hora','Producto','Precio unitario','Unidades vendidas','Descuento aplicado','Precio total','Pago','Cliente']);
    salesHistory.slice().reverse().forEach(v => {
      const fecha = new Date(v.time).toLocaleString('es-PE',{dateStyle:'short',timeStyle:'short'});
      (v.items||[]).forEach(it => {
        const desc = it.descuento || 0;
        rows.push([fecha, it.name, it.price.toFixed(2), it.qty, desc.toFixed(2), (it.price*it.qty-desc).toFixed(2), v.paymentType || 'efectivo', v.clientName || '']);
      });
    });
  } else if (currentReporte === 'caja') {
    filename = 'reporte_caja.csv';
    rows.push(['Cierre','Ingresos','Gastos','Retiros','Saldo']);
    cierres.slice().reverse().forEach(c => rows.push([
      new Date(c.hasta).toLocaleString('es-PE',{dateStyle:'short',timeStyle:'short'}),
      c.ingresos.toFixed(2), c.gastos.toFixed(2), c.retiros.toFixed(2), c.saldo.toFixed(2)
    ]));
  } else if (currentReporte === 'clientes') {
    filename = 'reporte_clientes.csv';
    rows.push(['Cliente','Saldo']);
    clients.forEach(c => rows.push([c.name, (c.saldo||0).toFixed(2)]));
  } else if (currentReporte === 'proveedores') {
    filename = 'reporte_proveedores.csv';
    rows.push(['Proveedor','Saldo']);
    providers.forEach(p => rows.push([p.name, (p.saldo||0).toFixed(2)]));
  } else if (currentReporte === 'articulos') {
    filename = 'reporte_articulos.csv';
    rows.push(['Producto','Stock','Precio','Valor total']);
    products.forEach(p => rows.push([p.name, p.stock, p.price.toFixed(2), (p.price*p.stock).toFixed(2)]));
  }
  if (rows.length <= 1) { showNotify('No hay datos para exportar', 'danger'); return; }
  downloadCSV(filename, rows);
}

function showNotify(msg, type='') {
  const el = document.getElementById('notify');
  el.textContent = msg;
  el.style.background = type==='danger' ? '#e24b4a' : type==='success' ? '#639922' : '#185fa5';
  el.classList.add('show');
  clearTimeout(el._t);
  el._t = setTimeout(() => el.classList.remove('show'), 2200);
}

loadData();
renderProducts();
renderClienteSelect();
renderUsuarioActivoSelect();
renderCart();
document.getElementById('barcode-input').focus();

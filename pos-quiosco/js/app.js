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
let nextId = 9;
let editingId = null;
let scanTimer = null;
let currentTab = 'venta';

function setTab(t) {
  currentTab = t;
  ['venta','inventario','caja'].forEach((tab,i) => {
    document.getElementById('tab-'+tab).style.display = tab===t ? '' : 'none';
    document.querySelectorAll('.tab')[i].classList.toggle('active', tab===t);
  });
  if (t==='inventario') renderInventario();
  if (t==='caja') renderCaja();
}

function renderProducts(filter='') {
  const grid = document.getElementById('products-grid');
  const q = filter.toLowerCase();
  const filtered = products.filter(p => p.name.toLowerCase().includes(q) || p.barcode.includes(q));
  if (!filtered.length) {
    grid.innerHTML = '<div style="color:#bbb;font-size:13px;grid-column:1/-1;padding:1rem 0">Sin resultados para "'+filter+'"</div>';
    return;
  }
  grid.innerHTML = filtered.map(p => `
    <div class="prod-card" onclick="addToCart(${p.id})" title="Cód: ${p.barcode}">
      <div class="prod-name">${p.name}</div>
      <div class="prod-price">S/ ${p.price.toFixed(2)}</div>
      <div class="prod-stock">${p.stock} en stock</div>
      <div class="prod-barcode">${p.barcode}</div>
    </div>
  `).join('');
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
    cart.push({id, name:prod.name, price:prod.price, qty:1});
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

function renderCart() {
  const el = document.getElementById('cart-items');
  const total = cart.reduce((s,c) => s + c.price * c.qty, 0);
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
      </div>
      <div class="qty-ctrl">
        <button class="qty-btn" onclick="changeQty(${item.id},-1)">−</button>
        <span class="qty-num">${item.qty}</span>
        <button class="qty-btn" onclick="changeQty(${item.id},1)">+</button>
      </div>
      <div class="item-subtotal">S/ ${(item.price*item.qty).toFixed(2)}</div>
      <i class="ti ti-x del-btn" onclick="removeItem(${item.id})" title="Quitar"></i>
    </div>
  `).join('');
}

function calcVuelto() {
  const total = cart.reduce((s,c) => s + c.price * c.qty, 0);
  const efectivo = parseFloat(document.getElementById('efectivo-input').value) || 0;
  const row = document.getElementById('vuelto-row');
  if (efectivo > 0 && efectivo >= total) {
    document.getElementById('vuelto-amt').textContent = 'S/ ' + (efectivo - total).toFixed(2);
    row.style.display = 'flex';
  } else {
    row.style.display = 'none';
  }
}

function cobrar() {
  const total = cart.reduce((s,c) => s + c.price * c.qty, 0);
  const efectivo = parseFloat(document.getElementById('efectivo-input').value) || 0;
  if (efectivo > 0 && efectivo < total) { showNotify('Efectivo insuficiente', 'danger'); return; }
  cart.forEach(item => {
    const prod = products.find(p => p.id === item.id);
    if (prod) prod.stock = Math.max(0, prod.stock - item.qty);
  });
  salesHistory.push({items:[...cart.map(c=>({...c}))], total, time: new Date()});
  showNotify('Venta registrada — S/ ' + total.toFixed(2), 'success');
  clearCart();
  renderProducts(document.getElementById('barcode-input').value);
}

function clearCart() {
  cart = [];
  document.getElementById('efectivo-input').value = '';
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
    const val = e.target.value.trim();
    const prod = products.find(p => p.barcode === val);
    if (prod) { addToCart(prod.id); e.target.value = ''; renderProducts(''); }
    else if (val) showNotify('Código no encontrado: ' + val, 'danger');
  }
}

function openAddModal(id=null) {
  editingId = id;
  const p = id ? products.find(x => x.id === id) : null;
  document.getElementById('modal-title').textContent = id ? 'Editar producto' : 'Nuevo producto';
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
    </div>`;
  }).join('');
}

function renderCaja() {
  const el = document.getElementById('caja-section');
  if (!salesHistory.length) {
    el.innerHTML = '<div class="resumen-empty">No hay ventas registradas hoy</div>'; return;
  }
  const totalVentas = salesHistory.reduce((s,v) => s + v.total, 0);
  const numVentas = salesHistory.length;
  el.innerHTML = `
    <div class="caja-row"><span style="color:#666">Ventas realizadas</span><span style="font-weight:600">${numVentas}</span></div>
    <div class="caja-row"><span style="color:#666">Ticket promedio</span><span>S/ ${(totalVentas/numVentas).toFixed(2)}</span></div>
    ${salesHistory.map((v,i) => `<div class="caja-row"><span style="color:#999">Venta #${i+1} — ${v.time.toLocaleTimeString('es-PE',{hour:'2-digit',minute:'2-digit'})}</span><span>S/ ${v.total.toFixed(2)}</span></div>`).join('')}
    <div class="caja-row"><span>Total en caja</span><span style="color:#185fa5">S/ ${totalVentas.toFixed(2)}</span></div>
  `;
}

function cerrarCaja() {
  if (!confirm('¿Cerrar caja? Se borrarán las ventas del día registradas en pantalla.')) return;
  salesHistory = [];
  renderCaja();
  showNotify('Caja cerrada');
}

function showNotify(msg, type='') {
  const el = document.getElementById('notify');
  el.textContent = msg;
  el.style.background = type==='danger' ? '#e24b4a' : type==='success' ? '#639922' : '#185fa5';
  el.classList.add('show');
  clearTimeout(el._t);
  el._t = setTimeout(() => el.classList.remove('show'), 2200);
}

renderProducts();
renderCart();
document.getElementById('barcode-input').focus();

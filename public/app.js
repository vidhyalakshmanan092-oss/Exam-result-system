const API = "/api/students";
const SUBJECTS = ["Maths", "DSA", "Physics", "English", "Chemistry"];
const PASS_MARK = 35; // fail if any subject < 35
const $ = id => document.getElementById(id);
let editing = null;

const esc = s => String(s).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const passed = s => s.marks.every(m => m >= PASS_MARK);
const medal = r => (r === 1 ? "🥇" : r === 2 ? "🥈" : r === 3 ? "🥉" : "");

function say(text, ok) { const m = $("msg"); m.textContent = text; m.className = ok ? "ok" : "err"; }

async function load() {
  const res = await fetch(`${API}?sort=${$("sort").value}`);
  const list = await res.json();
  renderRows(list); renderStats(list); renderPodium(list);
}

function renderRows(list) {
  $("rows").innerHTML = list.length ? list.map(s => `
    <tr>
      <td><span class="rank ${s.rank <= 3 ? "r" + s.rank : ""}">${s.rank}</span></td>
      <td>${s.roll}</td><td>${esc(s.name)}</td>
      ${s.marks.map(m => `<td>${m}</td>`).join("")}
      <td><b>${s.total}</b></td><td>${s.percentage.toFixed(2)}</td>
      <td class="${passed(s) ? "pass" : "fail"}">${passed(s) ? "Pass" : "Fail"}</td>
      <td><button class="sm" onclick='edit(${JSON.stringify(s)})'>Edit</button>
          <button class="sm del" onclick="del(${s.roll})">Delete</button></td>
    </tr>`).join("") : `<tr><td colspan="12">No students yet. Add one above.</td></tr>`;
}

function renderStats(list) {
  if (!list.length) { $("stats").innerHTML = ""; return; }
  const avg = list.reduce((a, s) => a + s.percentage, 0) / list.length;
  const top = Math.max(...list.map(s => s.total)), low = Math.min(...list.map(s => s.total));
  const pass = list.filter(passed).length;
  const card = (v, l) => `<div class="stat"><b>${v}</b><span>${l}</span></div>`;
  $("stats").innerHTML = card(list.length, "Students") + card(avg.toFixed(1) + "%", "Class average") +
    card(top + "/500", "Highest total") + card(low + "/500", "Lowest total") +
    card(((pass / list.length) * 100).toFixed(0) + "%", "Pass percentage");
}

function renderPodium(list) {
  const top = list.filter(s => s.rank <= 3).sort((a, b) => a.rank - b.rank);
  $("podium").innerHTML = top.map(s => `<div class="pod"><div class="medal">${medal(s.rank)}</div>
    <b>${esc(s.name)}</b><br><small>Rank ${s.rank} &middot; Roll ${s.roll}</small><br>${s.total}/500 (${s.percentage.toFixed(1)}%)</div>`).join("");
}

$("form").addEventListener("submit", async e => {
  e.preventDefault();
  const p = new URLSearchParams({ roll: $("roll").value, name: $("name").value });
  document.querySelectorAll(".mark").forEach((el, i) => p.append("m" + (i + 1), el.value));
  const res = await fetch(API, { method: editing ? "PUT" : "POST", body: p });
  const data = await res.json();
  if (!res.ok) return say(data.error, false);
  say(editing ? "Student updated" : "Student added", true);
  resetForm(); load();
});

function edit(s) {
  editing = s.roll;
  $("roll").value = s.roll; $("roll").disabled = true; $("name").value = s.name;
  document.querySelectorAll(".mark").forEach((el, i) => el.value = s.marks[i]);
  $("formTitle").textContent = "Edit Student"; $("saveBtn").textContent = "Update Student";
  $("cancelBtn").hidden = false; window.scrollTo({ top: 0, behavior: "smooth" });
}
function resetForm() {
  editing = null; $("form").reset(); $("roll").disabled = false;
  $("formTitle").textContent = "Add Student"; $("saveBtn").textContent = "Add Student"; $("cancelBtn").hidden = true;
}
$("cancelBtn").onclick = () => { resetForm(); say("", true); };

async function del(roll) {
  if (!confirm("Delete student with roll " + roll + "?")) return;
  await fetch(`${API}?roll=${roll}`, { method: "DELETE" });
  load();
}

$("searchBtn").onclick = async () => {
  const roll = $("searchRoll").value;
  if (!roll) return;
  const d = await (await fetch(`/api/search?roll=${roll}`)).json();
  $("searchResult").innerHTML = d.found
    ? `✅ Found: <b>${esc(d.student.name)}</b> (Roll ${d.student.roll}) &mdash; Rank <b>${d.student.rank}</b>, Total ${d.student.total}/500. <small>(binary search took ${d.steps} step${d.steps > 1 ? "s" : ""})</small>`
    : `❌ No student with roll ${esc(roll)}. <small>(binary search took ${d.steps} step${d.steps !== 1 ? "s" : ""})</small>`;
};

$("sort").onchange = load;
load();
